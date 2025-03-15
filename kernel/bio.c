// Buffer cache.
//
// 缓冲区缓存是 buf 结构的链接列表，持有
// 磁盘块内容的缓存副本。缓存磁盘块
// in memory 减少了磁盘读取次数，并且还提供
// 多个进程使用的磁盘块的同步点。
//
// Interface:
// * 要获取特定磁盘块的缓冲区，请调用 bread。
// * 更改缓冲区数据后，调用 bwrite 将其写入磁盘。
// * 完成缓冲区后，调用 brelse.
// * 调用 brelse 后不要使用缓冲区。
// * 一次只有一个进程可以使用缓冲区，
// 因此，不要将它们保留超过必要的时间。

#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"
// 哈希表中的桶号索引。根据提示，设置质数个桶可以降低哈希冲突的可能性
#define NBUFMAP_BUCKET 13
// 哈希索引
#define BUFMAP_HASH(dev, blockno) ((((dev) << 27) | (blockno)) % NBUFMAP_BUCKET)
struct
{
  // struct spinlock lock;
  struct buf buf[NBUF];
  struct spinlock eviction_lock; // 驱逐锁
  // 哈希桶
  struct buf bufmap[NBUFMAP_BUCKET];
  struct spinlock bufmap_locks[NBUFMAP_BUCKET]; // 桶锁

  // 所有缓冲区的链接列表，通过 prev/next。
  // 按缓冲区的最近使用时间排序。
  // head.next 是最新的，head.prev 是最不重要的。
  // struct buf head;
} bcache;

void binit(void)
{
  // 初始化桶锁
  for (int i = 0; i < NBUFMAP_BUCKET; i++)
  {
    initlock(&bcache.bufmap_locks[i], "bcache_bufmap");
    bcache.bufmap[i].next = 0;
  }
  for (int i = 0; i < NBUF; i++)
  {
    // 初始化缓存区块
    struct buf *b = &bcache.buf[i];
    initsleeplock(&b->lock, "buffer");
    b->lastuse = 0;
    b->refcnt = 0;

    //  将所有缓存区块添加到bufmap[0]
    b->next = bcache.bufmap[0].next;
    bcache.bufmap[0].next = b;
  }
  initlock(&bcache.eviction_lock, "bcache_eviction");
}

// 在缓冲区缓存中查找设备 dev 上的块。
// 如果未找到，则分配缓冲区。
// 无论哪种情况，都返回 locked buffer。
static struct buf *
bget(uint dev, uint blockno)
{
  struct buf *b;
  uint key = BUFMAP_HASH(dev, blockno);
  acquire(&bcache.bufmap_locks[key]);

  for (b = bcache.bufmap[key].next; b; b = b->next)
  {
    if (b->dev == dev && b->blockno == blockno)
    {
      b->refcnt++;
      acquiresleep(&b->lock);
      release(&bcache.bufmap_locks[key]);
      return b;
    }
  }

  // 没找到缓存，此时进行驱逐
  // 遍历所有hash桶
  // 为了防止死锁，释放当前手中的锁
  release(&bcache.bufmap_locks[key]);

  //防止重复刷新缓存
  acquire(&bcache.eviction_lock);

  // 有可能在释放锁和获取锁之间，其他进程已经将该块加载到缓存中
  for (b = bcache.bufmap[key].next; b; b = b->next)
  {
    if (b->dev == dev && b->blockno == blockno)
    {
      
      acquire(&bcache.bufmap_locks[key]); //添加引用次数时需要加锁
      b->refcnt++;  
      release(&bcache.bufmap_locks[key]);
      release(&bcache.eviction_lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // 记录当前驱逐对象,为了保持驱逐对象的状态不变化（被其他线程访问，改变refcnt）,一直持有驱逐对象的锁，除非出现新的驱逐对象，或者驱逐完毕才释放锁，
  struct buf *befo_evict = 0; // LRU-buf的前一个块
  int hoding_bucket = -1;           // 记录哪个桶持有锁
  // 如果还是没有发生，那么就需要进行驱逐
  for (int i = 0; i < NBUFMAP_BUCKET; i++)
  {
    // 疑问？如果这里进行了驱逐，但是其他线程使用了驱逐对象岂不是会出现问题
    int newfound = 0;
    acquire(&bcache.bufmap_locks[i]);
    for (b = &bcache.bufmap[i]; b->next; b = b->next)
    {
      if (b->next->refcnt == 0 && (!befo_evict || b->next->lastuse < befo_evict->next->lastuse))
      {
        befo_evict = b;
        newfound = 1;
      }
    }
    if (newfound == 0)
      release(&bcache.bufmap_locks[i]);
    else
    {
      if (hoding_bucket != -1)
      {
        release(&bcache.bufmap_locks[hoding_bucket]);
      }
      hoding_bucket = i;
    }
  }

  if (hoding_bucket == -1)
  {
    panic("bget: no buffers");
  }

  // 将驱逐对象进行位移
  b = befo_evict->next;
  if (hoding_bucket != key)
  {
    befo_evict->next = befo_evict->next->next;
    release(&bcache.bufmap_locks[hoding_bucket]);
    // 将驱逐对象加入需求桶中
    acquire(&bcache.bufmap_locks[key]);
    b->next = bcache.bufmap[key].next;
    bcache.bufmap[key].next = b;
  }
  b->blockno = blockno;
  b->dev = dev;
  b->valid = 0;
  b->refcnt = 1;
  release(&bcache.bufmap_locks[key]);
  release(&bcache.eviction_lock);
  acquiresleep(&b->lock);
  return b;
}

// 返回一个锁定的 buf 以及指示块的内容。
struct buf *
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if (!b->valid)
  {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// R 将 b 的内容写入磁盘。必须被锁定。
void bwrite(struct buf *b)
{
  if (!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

void brelse(struct buf *b)
{
  if (!holdingsleep(&b->lock))
    panic("brelse");
  releasesleep(&b->lock);

  uint key = BUFMAP_HASH(b->dev, b->blockno);
  acquire(&bcache.bufmap_locks[key]);
  b->refcnt--;
  if (b->refcnt == 0)
    b->lastuse = ticks;
  release(&bcache.bufmap_locks[key]);
}

void bpin(struct buf *b)
{
  uint key = BUFMAP_HASH(b->dev, b->blockno);
  acquire(&bcache.bufmap_locks[key]);
  b->refcnt++;
  release(&bcache.bufmap_locks[key]);
}

void bunpin(struct buf *b)
{
  uint key = BUFMAP_HASH(b->dev, b->blockno);
  acquire(&bcache.bufmap_locks[key]);
  b->refcnt--;
  release(&bcache.bufmap_locks[key]);
}
