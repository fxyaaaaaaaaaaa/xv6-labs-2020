#define T_DIR     1   // Directory
#define T_FILE    2   // File
#define T_DEVICE  3   // Device


//用于描述文件状态的结构体,包含了文件系统中一个文件的属性.
//存储文件的元数据。
struct stat {
  int dev;     // 文件系统的磁盘设备
  uint ino;    // Inode号
  short type;  // 文件类型
  short nlink; // 文件的链接数
  uint64 size; // 文件的大小(字节为单位)
};
