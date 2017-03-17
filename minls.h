#include "minCommon.h"

void printPartition(struct part_entry partitionPtr);
void printSuperblock(struct superblock sb);
void printFile(struct fileEntry *file);
void printFiles(struct fileEntry *fileEntries, int numFiles);
void printInode(struct inode in);
void printInodeFiles(struct inode *in);
void printPermissions(uint16_t mode);
void printSinglePerm(int print, char c);