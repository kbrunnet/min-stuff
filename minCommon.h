#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <linux/limits.h>

/* constants */
#define PTABLE_OFFSET 0x1BE
#define PMAGIC510 0x55
#define PMAGIC511 0xAA
#define MINIXPART 0x81
#define MIN_MAGIC 0x4d5a         /* the minix magic number */
#define MIN_MAGIC_REV 0x5a4d     /* the minix magic number reversed */
#define MIN_MAGIC_OLD 0x2468     /* the v2 minix magic number */
#define MIN_MAGIC_REV_OLD 0x6824 /* the v2 magic number reversed */
                                 /* we have an endian problem */
#define MIN_ISREG(m) (((m)&0170000)==0100000)
#define MIN_ISDIR(m) (((m)&0170000)==0040000)
#define MIN_IRUSR 0400
#define MIN_IWUSR 0200
#define MIN_IXUSR 0100
#define MIN_IRGRP 0040
#define MIN_IWGRP 0020
#define MIN_IXGRP 0010
#define MIN_IROTH 0004
#define MIN_IWOTH 0002
#define MIN_IXOTH 0001

#define INVALID_OPTION -1

unsigned int zone_size;
struct inode *iTable;
FILE *image;
int numInodes;

struct part_entry {
   uint8_t bootind;      /* boot indicator 0/ACTIVE_FLAG   */
   uint8_t start_head;   /* head value for first sector    */
   uint8_t start_sec;    /* sector value + cyl bits for first sector */
   uint8_t start_cyl;    /* track value for first sector   */
   uint8_t sysind;       /* system indicator      */
   uint8_t last_head;    /* head value for last sector  */
   uint8_t last_sec;     /* sector value + cyl bits for last sector */
   uint8_t last_cyl;     /* track value for last sector    */
   uint32_t lowsec;       /* logical first sector     */
   uint32_t size;         /* size of partition in sectors   */
};

#define ACTIVE_FLAG  0x80  /* value for active in bootind field (hd0) */
#define NR_PARTITIONS   4  /* number of entries in partition table */
#define  PART_TABLE_OFF 0x1BE /* offset of partition table in boot sector */

/* Partition types. */
#define NO_PART      0x00  /* unused entry */
#define MINIX_PART   0x81  /* Minix partition type */


struct superblock { /* Minix Version 3 Superblock
                     * this structure found in fs/super.h
                     * in minix 3.1.1
                     */
   /* on disk. These fields and orientation are non–negotiable */

   uint32_t ninodes;       /* number of inodes in this filesystem */
   uint16_t pad1;          /* make things line up properly */
   int16_t i_blocks;       /* # of blocks used by inode bit map */
   int16_t z_blocks;       /* # of blocks used by zone bit map */
   uint16_t firstdata;     /* number of first data zone */
   int16_t log_zone_size;  /* log2 of blocks per zone */
   int16_t pad2;           /* make things line up again */
   uint32_t max_file;      /* maximum file size */
   uint32_t zones;         /* number of zones on disk */
   int16_t magic;          /* magic number */
   int16_t pad3;           /* make things line up again */
   uint16_t blocksize;     /* block size in bytes */
   uint8_t subversion;     /* filesystem sub–version */
};

#define DIRECT_ZONES 7

struct inode {
   uint16_t mode;    /* mode */
   uint16_t links;   /* number or links */
   uint16_t uid;
   uint16_t gid;
   uint32_t size;
   int32_t atime;
   int32_t mtime;
   int32_t ctime;
   uint32_t zone[DIRECT_ZONES];
   uint32_t indirect;
   uint32_t two_indirect;
   uint32_t unused;
};

#ifndef DIRSIZ
#define DIRSIZ 60
#endif
struct fileEntry {
   uint32_t inode;
   char name[DIRSIZ];
};

struct minOptions {
   int verbose;
   int partition;
   int subpartition;
   char *imagefile;
   char *path;
   char *fullPath;
};

struct minixConfig {
   FILE *image;
   struct superblock sb;
   unsigned int zone_size;
};

void parseArgs(int argc, char *const argv[], struct minOptions *options);
void getMinixConfig(struct minOptions options, struct minixConfig *config);
void setPartitionOffset(FILE *image, int partitionNum);
void setSubpartitionOffset(FILE *image, int partitionNum);
void setOffset(FILE *image, int partitionNum, int isSub);
struct inode traversePath(struct inode *root, 
                          unsigned int ninodes, 
                          char *path);
struct fileEntry *getFileEntries(struct inode directory);
void *getInode(int inodeNum);
void *copyZones(struct inode file);
size_t fseekPartition(FILE *stream, long int offset, int whence);