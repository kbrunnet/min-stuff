#include "minget.h"

char fullPath[PATH_MAX] = "";

int main(int argc, char *const argv[])
{
   /* This code allocates space for the path names
      and sets all integer options to default values */
   struct minOptions options;
   options.verbose = 0;
   options.partition = INVALID_OPTION;
   options.subpartition = INVALID_OPTION;
   options.imagefile = malloc(NAME_MAX);
   options.path = malloc(PATH_MAX);
   options.fullPath = malloc(PATH_MAX);

   /* calls the function in mincommon.c 
   that parses the arguments */

   parseArgs(argc, argv, &options);
   strcpy(fullPath, options.fullPath);

   struct minixConfig config;
   config.image = NULL;


   /* gets the image */
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

   /* traverses through the root to find the file
      user searched for */ 
   struct inode destFile = traversePath(iTable, 
   	config.sb.ninodes, options.path);


   /* gets the all the contents of the zones 
      (including direct, indirect, and double)
      for the file the user is searching for */
   char *data = copyZones(destFile);
   if (MIN_ISREG(destFile.mode)) {
   	printf("%s", data);
   }
   else {
   	printf("%s: Not a regular file\n", fullPath);
   }

   exit(EXIT_SUCCESS);
}