#include "minls.h"

char fullPath[PATH_MAX] = "";

int main(int argc, char *const argv[])
{
   struct minOptions options;
   options.verbose = 0;
   options.partition = -1;
   options.subpartition = -1;
   options.imagefile = malloc(NAME_MAX);
   options.path = malloc(PATH_MAX);
   options.fullPath = malloc(PATH_MAX);
   
   if (!options.imagefile) {
      fprintf(stderr, "Malloc is failing\n");
   }
   options.path = malloc(PATH_MAX);
   if (!options.path) {
      fprintf(stderr, "Malloc is failing\n");
   }
   options.fullPath = malloc(PATH_MAX);
   if (!options.fullPath) {
      fprintf(stderr, "Malloc is failing\n");
   }

   parseArgs(argc, argv, &options);
   strcpy(fullPath, options.fullPath);


   struct minixConfig config;
   config.image = NULL;

   getMinixConfig(options, &config);
   image = config.image;
   zone_size = config.zone_size;
   numInodes = config.sb.ninodes;

   /* Read the root directory table */
   fseekPartition(image, (2 + config.sb.i_blocks + config.sb.z_blocks) 
                         * config.sb.blocksize, 
                  SEEK_SET);

   iTable = (struct inode*) malloc(numInodes * sizeof(struct inode));
   size_t result = fread(iTable, sizeof(struct inode), numInodes, image);
   if (result != numInodes) {
      fprintf(stderr, "Error reading in the image to the iNode table\n");
   }

   struct inode destFile = traversePath(iTable, 
      config.sb.ninodes, options.path);
   if (MIN_ISDIR(destFile.mode)) {
      printf("%s:\n", options.path);
   }
   printInodeFiles(&destFile);

   exit(EXIT_SUCCESS);
}

/* Given an inode, if the inode is a file,
   it prints the permissions, size and name of the file.
   If the file is a directory, then this function prints
   all the contents of the directory 
*/ 
void printInodeFiles(struct inode *in) {
   if (MIN_ISREG(in->mode)) {
      printPermissions(in->mode);
      printf("%10u ", in->size);
      printf("%s\n", fullPath);
   }

   if (MIN_ISDIR(in->mode)) {
      struct fileEntry *fileEntries = getFileEntries(*in);
      int numFiles = in->size/sizeof(struct fileEntry);
      printFiles(fileEntries, numFiles);
   }
}

/* Given a point to a list of file entries in a directory,
   this function traverses through those file entries
   and calls a function that prints information about 
   each file 
*/
void printFiles(struct fileEntry *fileEntries, int numFiles) {
   int i;
   for(i = 0; i < numFiles; i++) {
      /* only print the contents of the file if the inode
         is valid (if the inode is zero, it signifies a
         deleted file which we don't want to print) */
      if (fileEntries[i].inode) {
         printFile(&fileEntries[i]);
      }
   }
}

/* This function takes in a pointer to a single 
   file entry in a directory and prints out the 
   permissions, size, and name of the file 
*/
void printFile(struct fileEntry *file) {
   struct inode *iNode = (struct inode *) getInode(file->inode);
   if (iNode != NULL) {   
      printPermissions(iNode->mode);
      printf("%10u ", iNode->size);
      printf("%s\n", file->name);
   }
}

/*
   This function prints out the permissions
   of a file given the mode of the inode. 
*/
void printPermissions(uint16_t mode) {
   printSinglePerm(MIN_ISDIR(mode), 'd');
   printSinglePerm(mode & MIN_IRUSR, 'r');
   printSinglePerm(mode & MIN_IWUSR, 'w');
   printSinglePerm(mode & MIN_IXUSR, 'x');
   printSinglePerm(mode & MIN_IRGRP, 'r');
   printSinglePerm(mode & MIN_IWGRP, 'w');
   printSinglePerm(mode & MIN_IXGRP, 'x');
   printSinglePerm(mode & MIN_IROTH, 'r');
   printSinglePerm(mode & MIN_IWOTH, 'w');
   printSinglePerm(mode & MIN_IXOTH, 'x');
}

/*
   Prints out a single permission. 
*/
void printSinglePerm(int print, char c) {
   if (print) {
      printf("%c", c);
   }
   else {
      printf("-");
   }
}

/* 
   Prints the contents of the partition entry struct
   passed in
*/
void printPartition(struct part_entry  partitionPtr) {
   printf("  %X\n", partitionPtr.bootind);
   printf("  %X\n", partitionPtr.start_head);
   printf("  %X\n", partitionPtr.start_sec);
   printf("  %X\n", partitionPtr.start_cyl);
   printf("  %X\n", partitionPtr.sysind);
   printf("  %X\n", partitionPtr.last_head);
   printf("  %X\n", partitionPtr.last_sec);
   printf("  %X\n", partitionPtr.last_cyl);
   printf("  %lX\n", (unsigned long) partitionPtr.lowsec);
   printf("  %lX\n", (unsigned long) partitionPtr.size);
}

/* 
   Prints the contents of the superblock struct passed in
*/
void printSuperblock(struct superblock sb) {
   puts("SuperBlock: ");
   printf("  ninodes: %d\n", sb.ninodes);
   printf("  pad1: %d\n", sb.pad1);
   printf("  i_blocks: %d\n", sb.i_blocks);
   printf("  z_blocks: %d\n", sb.z_blocks);
   printf("  firstdata: %d\n", sb.firstdata);
   printf("  log_zone_size: %d\n", sb.log_zone_size);
   printf("  pad2: %d\n", sb.pad2);
   printf("  max_file: %u\n", sb.max_file);
   printf("  zones: %d\n", sb.zones);
   printf("  magic: 0x%x\n", sb.magic);
   printf("  pad3: 0x%x\n", sb.pad3);
   printf("  blocksize: %d\n", sb.blocksize);
   printf("  subversion: %d\n", sb.subversion);
}

/* 
   Prints the contents of the inode struct passed in
*/
void printInode(struct inode in) {
   int z;
   puts("inode: ");
   printf("  mode: 0x%x\n", in.mode);
   printf("  links: %u\n", in.links);
   printf("  uid: %u\n", in.uid);
   printf("  gid: %u\n", in.gid);
   printf("  size: %u\n", in.size);
   printf("  atime: %u\n", in.atime);
   printf("  mtime: %u\n", in.mtime);
   printf("  ctime: %u\n", in.ctime);
   printf("  Direct zones: \n");
   for (z = 0; z < DIRECT_ZONES; z++) {
      printf("\tzone[%u]\t=\t%u\n", z, in.zone[z]);
   }
   printf("  indirect: %u\n", in.indirect);
   printf("  double: %u\n", in.two_indirect);
}
