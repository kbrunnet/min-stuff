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
   fread(iTable, sizeof(struct inode), numInodes, image);
   // printf("numInode %d\n", numInodes);

   // printInodeFiles(iTable);

   // printInode(iTable[16]);
   // printInodeFiles(&iTable[16]);
   
   // printf("\n");
   // printf("\n");
   // printf("\n");
   struct inode destFile = traversePath(iTable, 
      config.sb.ninodes, options.path);
   // printf("INODE RETURNED: \n");
   if (MIN_ISDIR(destFile.mode)) {
      printf("%s:\n", options.path);
   }
   printInodeFiles(&destFile);

   exit(EXIT_SUCCESS);
}

void printInodeFiles(struct inode *in) {
   // printInode(*in);
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

void printFiles(struct fileEntry *fileEntries, int numFiles) {
   int i;
   for(i = 0; i < numFiles; i++) {
      // printf("%u\n", fileEntries[i].inode);
      if (fileEntries[i].inode) {
         printFile(&fileEntries[i]);
      }
   }
}

void printFile(struct fileEntry *file) {
   // printf("%d ", file->inode);
   struct inode *iNode = (struct inode *) getInode(file->inode);
   if (iNode == NULL) {
      //if it gets here the inode is either
      //not stored inside our iTable 
      //OR, the inode was zero which is an
      //invalid inode
      // printf("null Inode\n");
   }
   else {   
      printPermissions(iNode->mode);
      printf("%10u ", iNode->size);
      printf("%s\n", file->name);
   }
}

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

void printSinglePerm(int print, char c) {
   if (print) {
      printf("%c", c);
   }
   else {
      printf("-");
   }
}

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
