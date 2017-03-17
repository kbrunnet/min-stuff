#include "minCommon.h"

#define PARTITION_MSG \
"Partition %d out of range.  Must be 0..3.\n"

#define SUBPARTITION_MSG \
"Subpartition %d out of range.  Must be 0..3.\n"

#define SUB_INVALID "Not a Minix subpartition.\n"

#define PART_INVALID "Not a Minix partition.\n"

#define BAD_MAGIC \
"Bad magic number. (0x%.4x)\nThis doesn't look like a MINIX filesystem.\n"

#define USAGE_MSG \
"usage: %s  [ -v ] [ -p num [ -s num ] ] imagefile [ path ]\n\
Options:\n\
\t-p\t part    --- select partition for filesystem (default: none)\n\
\t-s\t sub     --- select subpartition for filesystem (default: none)\n\
\t-h\t help    --- print usage information and exit\n\
\t-v\t verbose --- increase verbosity level\n"

static uint32_t partitionOffset = 0;
static uint32_t partitionSize = -1;
char fullPathName[PATH_MAX] = "";
static int verbose;

/* Parse the arguments for the minls and minget programs */
void parseArgs(int argc, char *const argv[], struct minOptions *options) {
   int opt;
   opterr = 0;

   /* traverse through the given command-line args */
   while ((opt = getopt(argc, argv, "vp:s:")) != -1) {
      switch (opt) {
         /* verbose */
         case 'v':
            options->verbose++;
         break;

         /* partition number */
         case 'p':
            options->partition = atoi(optarg);
            if (options->partition < 0 || options->partition > 3) {
               fprintf(stderr, PARTITION_MSG, options->partition);
               fprintf(stderr, USAGE_MSG, argv[0]);
               exit(EXIT_FAILURE);
            }
         break;
         
         /* subpartition number */
         case 's':
            options->subpartition = atoi(optarg);
            if (options->subpartition < 0 || options->subpartition > 3) {
               fprintf(stderr, SUBPARTITION_MSG, options->subpartition);
               fprintf(stderr, USAGE_MSG, argv[0]);
               exit(EXIT_FAILURE);
            }
         break;

         /* invalid arg */
         default:
            fprintf(stderr, USAGE_MSG, argv[0]);
            exit(EXIT_FAILURE);
      }
   }
   /* required image file */
   if (optind < argc) {
      strcpy(options->imagefile, argv[optind]);
   }
   else {
      fprintf(stderr, USAGE_MSG, argv[0]);
   }
   optind++;
   /* optional source path */
   if (optind < argc) {
      strcpy(options->path, argv[optind]);
      strcpy(options->fullPath, options->path);
   }
   else {
      strcpy(options->path, "/");
   }
   /* relative paths become absolute from / */
   if (options->path[0] != '/') {
      char pathBase[PATH_MAX] = "/";
      strcat(pathBase, options->path);
      strcpy(options->path, pathBase);
   }
   strcpy(fullPathName, options->path);

   verbose = options->verbose;
}

/* Gets configuration data for a Minix image file */
void getMinixConfig(struct minOptions options, struct minixConfig *config) {
   /* open the image file */
   FILE *fp = fopen(options.imagefile, "rb");
   if (!fp) {
      fprintf(stderr, "Failed to open file %s (errno: %d)\n", 
                      options.imagefile, 
                      errno);
      exit(EXIT_FAILURE);
   }
   config->image = fp;

   /* set global partition offsets if necessary */
   if (options.partition >= 0) {
      setPartitionOffset(config->image, options.partition);
      if (options.subpartition >= 0) {
         setSubpartitionOffset(config->image, options.subpartition);
      }
   }

   /* Read the superblock */
   fseekPartition(config->image, 1024, SEEK_SET);
   fread(&(config->sb), sizeof(struct superblock), 1, config->image);
   if (ferror(config->image)) {
      fprintf(stderr, "error reading file (%d)\n", ferror(config->image));
      exit(EXIT_FAILURE);
   }

   if (verbose) {
      // printSuperblock(config->sb);
   }

   /* check Minix magin number */
   if (config->sb.magic != MIN_MAGIC) {
      fprintf(stderr, BAD_MAGIC,
         config->sb.magic);
      exit(EXIT_FAILURE);
   }

   /* set zone size to log_zone_size << 2, if it's not 0 */
   config->zone_size = config->sb.log_zone_size ? 
   (config->sb.blocksize << config->sb.log_zone_size) : config->sb.blocksize;
}

/* Wrapper for setOffset on top-level partitions */
void setPartitionOffset(FILE *image, int partitionNum) {
   setOffset(image, partitionNum, IS_PART);
}

/* Wrapper for setOffset on subpartitions */
void setSubpartitionOffset(FILE *image, int partitionNum) {
   setOffset(image, partitionNum, IS_SUB_PART);
}

/* Sets a global offset when seeking in partitioned images */
void setOffset(FILE *image, int partitionNum, int isSub) {
   /* Read the partition table */
   fseekPartition(image, 0x1BE, SEEK_SET);

   struct part_entry partition_table[4];
   fread(partition_table, sizeof(struct part_entry), 4, image);
   if (ferror(image)) {
      fprintf(stderr, "error reading file (%d)\n", ferror(image));
      exit(EXIT_FAILURE);
   }

   if (verbose) {
      // for (i = 0; i < 4; i++) {
         // printf("i: %d\n", i);
         // printPartition(partition_table[i]);
      // }
   }

   /* make sure partition table is valid */
   uint16_t *ptValid = malloc(sizeof(uint16_t));
   fread(ptValid, sizeof(uint16_t), 1, image);
   if (ferror(image)) {
      fprintf(stderr, "error reading file (%d)\n", ferror(image));
      exit(EXIT_FAILURE);
   }
   if (*ptValid != 0xAA55) {
      fprintf(stderr, "not a valid partition table (%X)\n", *ptValid);
      exit(EXIT_FAILURE);
   }

   /* make sure subpartition is Minix */
   struct part_entry *partition = partition_table + partitionNum;
   if (partition->sysind != 0x81) {
      fprintf(stderr, isSub == IS_SUB_PART ? SUB_INVALID : PART_INVALID);
      exit(EXIT_FAILURE);
   }

   /* set offset globals */
   partitionOffset = partition->lowsec * 512;
   partitionSize = partition->size;
}

/* 
 * Takes the root inode and an absolute path, and returns the inode 
 * of the requested file or directory.
 */
struct inode traversePath(struct inode *inodeTable, 
   uint32_t ninodes, char *path) {

   struct inode currnode = inodeTable[0];

   /* traverse through file path */
   char *file = strtok(path, "/");
   while (file) {
      int numFiles = currnode.size / sizeof(struct fileEntry);
      struct fileEntry *fileEntries;
      fileEntries = getFileEntries(currnode);

      /* traverse through directory, looking for the file name */
      struct fileEntry *currEntry = fileEntries;
      while (strcmp(currEntry->name, file) && 
             currEntry < fileEntries + numFiles) {
         currEntry++;
      }

      /* didn't find the file */
      if (currEntry >= fileEntries + numFiles) {
         fprintf(stderr, "%s: File not found.\n", fullPathName);
         exit(EXIT_FAILURE);
      }

      /* found it, get its inode */
      currnode = *(struct inode *)getInode(currEntry->inode);
      file = strtok(NULL, "/");
   }

   return currnode;
}

/* Assumes that the given inode is a directory, returning all its entries */
struct fileEntry *getFileEntries(struct inode directory) {
   struct fileEntry *entries = (struct fileEntry *) copyZones(directory);
   return entries;
}

/* Returns the inode at the given index in the inode Table */
void *getInode(int inodeNum) {
   if (inodeNum == 0) {          /* invalid inode */
      return NULL;
   }
   if (inodeNum > numInodes) {   /* invalid inode */
      return NULL;
   }

   return &iTable[inodeNum - 1];
}

/* Copies all valid direct, indirect, and double-indirect zones in the given
 * inode, zongregating them into a single block of returned memory
 */ 
void *copyZones(struct inode file) {
   char *data, *nextData;
   /* round size of the returned data up to the nearest zone_size */
   uint32_t dataSize = (((file.size - 1) / zone_size) + 1) * zone_size;
   data = nextData = malloc(dataSize);

   int zoneIdx = 0;

   /* Direct Zones */
   /* loop through all direct zones while there's more data */
   while (nextData < data + file.size &&
          zoneIdx < DIRECT_ZONES) {

      uint32_t zoneNum = file.zone[zoneIdx];

      if (zoneNum) {
         /* copy the zone */
         fseekPartition(image, zoneNum * zone_size, SEEK_SET);
         fread(nextData, zone_size, 1, image);
         if (ferror(image)) {
            fprintf(stderr, "error reading file (%d)\n", ferror(image));
            exit(EXIT_FAILURE);
         }
      }
      else {         
         /* fill with zeros */
         memset(nextData, 0, zone_size);
      }
      nextData += zone_size;
      zoneIdx++;
   }

   if (nextData >= data + file.size) {
      return data;
   }

   /* Indirect Zones */
   int zoneNumsPerZone = zone_size / sizeof(uint32_t);
   uint32_t *indirectZones = malloc(sizeof(uint32_t) * zoneNumsPerZone);
   fseekPartition(image, file.indirect * zone_size, SEEK_SET);
   fread(indirectZones, sizeof(uint32_t), zoneNumsPerZone, image);
   if (ferror(image)) {
      fprintf(stderr, "error reading file (%d)\n", ferror(image));
      exit(EXIT_FAILURE);
   }
   zoneIdx = 0;

   /* loop through all indirect zones while there's more data */
   while (nextData < data + file.size &&
          zoneIdx < zoneNumsPerZone) {
      uint32_t zoneNum = indirectZones[zoneIdx];

      if (zoneNum) {
         /* copy the zone */
         fseekPartition(image, zoneNum * zone_size, SEEK_SET);
         fread(nextData, zone_size, 1, image);
         if (ferror(image)) {
            fprintf(stderr, "error reading file (%d)\n", ferror(image));
            exit(EXIT_FAILURE);
         }
      }
      else {         
         /* fill with zeros */
         memset(nextData, 0, zone_size);
      }
      nextData += zone_size;
      zoneIdx++;
   }

   if (nextData >= data + file.size) {
      return data;
   }

   /* Double-Indirect Zones */
   uint32_t *doubleIndirect = malloc(sizeof(uint32_t) * zoneNumsPerZone);
   fseekPartition(image, file.two_indirect * zone_size, SEEK_SET);
   fread(doubleIndirect, sizeof(uint32_t), zoneNumsPerZone, image);
   if (ferror(image)) {
      fprintf(stderr, "error reading file (%d)\n", ferror(image));
      exit(EXIT_FAILURE);
   }
   zoneIdx = 0;

   /* loop through all double-indirect zones while there's more data */
   while (nextData < data + file.size &&
          zoneIdx < zoneNumsPerZone) {
      fseekPartition(image, doubleIndirect[zoneIdx] * zone_size, SEEK_SET);
      fread(indirectZones, sizeof(uint32_t), zoneNumsPerZone, image);

      int indirectZoneIdx = 0;

      while (nextData < data + file.size &&
             indirectZoneIdx < zoneNumsPerZone) {
         uint32_t zoneNum = indirectZones[indirectZoneIdx];

         if (zoneNum) {
            /* copy the data*/
            fseekPartition(image, zoneNum * zone_size, SEEK_SET);
            fread(nextData, zone_size, 1, image);
            if (ferror(image)) {
               fprintf(stderr, "error reading file (%d)\n", ferror(image));
               exit(EXIT_FAILURE);
            }
         }  
         else {         
            /* fill with zeros */
            memset(nextData, 0, zone_size);
         }
         nextData += zone_size;
         indirectZoneIdx++;
      }
      zoneIdx++;
   }

   return data;
}

/* Wrapper for seeking into a potentially partitioned image */
size_t fseekPartition(FILE *stream, long int offset, int whence) {
   if (partitionSize > -1 && offset > partitionSize) {
      fprintf(stderr, "Attempting to seek outside of partition\n");
      exit(EXIT_FAILURE);
   }

   int ret = fseek(stream, offset + partitionOffset, whence);
   if (ret < 0) {
      fprintf(stderr, "error seeking in file (%d)\n", errno);
      exit(EXIT_FAILURE);
   }
   return ret;
}