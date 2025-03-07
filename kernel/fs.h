// On-disk file system format.
// Both the kernel and user programs use this header file.

//fs.c的代码设计三部分内容：对盘块操作代码，对inode操作代码，对目录操作代码。这样的顺序是自底向上的顺序
//对盘块操作：给出盘块号然后对盘块进行操作。
//inode操作：通过直到fd以及文件的偏移量，将逻辑块bn通过怕bmap函数转换为inode中记录的盘块号进行操作。
//以上操作，他们都不是目录 + 文件名的形式进行的。而目录操作是通过文件路径名查找文件的索引节点，以及文件路径名构成的属性目录结构进行的。

#define ROOTINO  1   // root i-number
#define BSIZE 1024  // block size

// Disk layout:
// [ boot block | super block | log | inode blocks |
//                                          free bit map | data blocks]
//
// mkfs computes the super block and builds an initial file system. The
// super block describes the disk layout:
struct superblock {
  uint magic;        // Must be FSMAGIC
  uint size;         // Size of file system image (blocks)
  uint nblocks;      // Number of data blocks
  uint ninodes;      // Number of inodes.
  uint nlog;         // Number of log blocks
  uint logstart;     // Block number of first log block
  uint inodestart;   // Block number of first inode block
  uint bmapstart;    // Block number of first free map block
};

#define FSMAGIC 0x10203040

#define NDIRECT 12
#define NINDIRECT (BSIZE / sizeof(uint))
#define MAXFILE (NDIRECT + NINDIRECT)

// On-disk inode structure
struct dinode {
  short type;           // File type
  short major;          // Major device number (T_DEVICE only)
  short minor;          // Minor device number (T_DEVICE only)
  short nlink;          // Number of links to inode in file system
  uint size;            // Size of file (bytes)
  uint addrs[NDIRECT+1];   // Data block addresses
};

//  每个区块的 Inode。
#define IPB           (BSIZE / sizeof(struct dinode))   //1024/64 = 16

//  编号为i的inode属于fs的第几个块
#define IBLOCK(i, sb)     ((i) / IPB + sb.inodestart)

// 每个块的位图位数
#define BPB           (BSIZE*8)

// 包含块 b 的位的自由映射块
#define BBLOCK(b, sb) ((b)/BPB + sb.bmapstart)

// 目录的block 是一个包含一系列 dirent 结构的文件。
#define DIRSIZ 14

struct dirent {
  ushort inum;
  char name[DIRSIZ];
};

