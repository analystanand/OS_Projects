/*****************************************************************************/
/*
                        File System Checker
*/
/*****************************************************************************/    
/************************** Header Files *************************************/

#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

/*******************************************************************************/

/************************ Declarations from types.h ****************************/

typedef unsigned int   uint;
typedef unsigned short ushort;
typedef unsigned char  uchar;
typedef uint pde_t;

/*******************************************************************************/

/************************** Declarations from fs.h *****************************/

/*
    On-disk file system format.
    Block 0 is unused.
    Block 1 is super block.
    Inodes start at block 2.
*/

#define ROOTINO 1  // root i-number
#define BSIZE 512  // block size

// File system super block
struct superblock {
  uint size;         // Size of file system image (blocks)
  uint nblocks;      // Number of data blocks
  uint ninodes;      // Number of inodes.
};

#define NDIRECT 12
#define NINDIRECT (BSIZE / sizeof(uint))
#define MAXFILE (NDIRECT + NINDIRECT)

// On-disk inode structure
struct dinode {
  short type;           // File type
  short major;          // Major device number (T_DEV only)
  short minor;          // Minor device number (T_DEV only)
  short nlink;          // Number of links to inode in file system
  uint size;            // Size of file (bytes)
  uint addrs[NDIRECT+1];   // Data block addresses
};

// Inodes per block.
#define IPB           (BSIZE / sizeof(struct dinode))

// Block containing inode i
#define IBLOCK(i)     ((i) / IPB + 2)

// Bitmap bits per block
#define BPB           (BSIZE*8)

// Block containing bit for block b
#define BBLOCK(b, ninodes) (b/BPB + (ninodes)/IPB + 3)

// Directory is a file containing a sequence of dirent structures.
#define DIRSIZ 14

struct dirent {
  ushort inum;
  char name[DIRSIZ];
};

// Directories per block
#define DPB           (BSIZE / sizeof(struct dirent))

/*******************************************************************************/

/*************************** Global Declarations *******************************/

void *image_pointer;
struct superblock *sb;
unsigned long on_disk_bmap[BSIZE*8];
int addresses[BSIZE];
char *bmap_blocks;

void bitmap();
void dir_check();
void dir_fmt_check();
void bad_inode();
void bad_addr();
void bmap_check();

char bitarr[8] = { 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80 };
#define BITSET(bitmapblocks, blockaddr) ((*(bitmapblocks + blockaddr / 8)) & (bitarr[blockaddr % 8]))

typedef struct _img_info{
   uint  inode_blocks_count;
   uint  bitmap_blocks_count;
   uint  root_data_block;
   struct superblock *sb;
   char* image_addr;
   char* inode_block_start;
   char* bitmap_block_start;
   char* data_block_start;

} img_info;

img_info disk_info;

/*******************************************************************************/

/********************************** Main ***************************************/

int main(int argc, char *argv[]){   

    if(argc < 2) {
        fprintf(stderr, "Usage: fcheck <file_system_image>\n");
        exit(1);
    }

    int fd = open(argv[1], O_RDWR);
    if(fd < 0) {
        fprintf(stderr, "image not found.\n");
        exit(1);
    }

    struct stat buffer;
    fstat(fd, &buffer);
    image_pointer = mmap(NULL, buffer.st_size, PROT_READ, MAP_PRIVATE, fd , 0);
    if(image_pointer == MAP_FAILED) {
        fprintf(stderr, "ERROR: mmap failed.\n");
        exit(1);
    }

    
    disk_info.image_addr = image_pointer;

    sb = (struct superblock *)(image_pointer + BSIZE);
    bmap_blocks = ((image_pointer + 2 * BSIZE) + (((sb->ninodes/IPB) + 1) * BSIZE));

    disk_info.sb = sb;
    disk_info.inode_blocks_count = (sb->ninodes/IPB)+1; //Ex. 25 Blocks
    disk_info.bitmap_blocks_count = (sb->size/BPB)+1;// Ex. 1 Block 
    disk_info.inode_block_start = (char *) (image_pointer + BSIZE * 2);
    disk_info.bitmap_block_start = (char *)(disk_info.inode_block_start + BSIZE*disk_info.inode_blocks_count);
    disk_info.data_block_start = (char*)(disk_info.bitmap_block_start+ BSIZE*disk_info.bitmap_blocks_count);
    disk_info.root_data_block = disk_info.inode_blocks_count + disk_info.bitmap_blocks_count+2;

    bitmap();
    dir_fmt_check();
    bad_inode();
    dir_check();
    bad_addr();
    bmap_check();
}

/******************************* End of Main ***********************************/

/***************************** On Disk Bitmap ***********************************/

void bitmap() { 
    int k;
    char *bmap_start = disk_info.bitmap_block_start;;
    int bitindex = 0;
    long unsigned zero = 0;
    long unsigned one = 1;
    long unsigned value; 

    for(k=0;k<BSIZE;k++){
        long unsigned i = 0;
        value = *bmap_start;
        while(i < 8) {
            long unsigned bit = value & (1 << i);
            i++;
            if(bit == zero) {
                on_disk_bmap[bitindex] = zero;
            } else {
                on_disk_bmap[bitindex] = one;
            }
            bitindex++;
        }
        bmap_start++;
    }
    
    /*for(int z=0;z<(sizeof(on_disk_bmap)/sizeof(on_disk_bmap[0]));z++){
      printf("%d -> %lu \n",z,on_disk_bmap[z]);
    }*/
}

/*******************************************************************************/

/***************************** Check 1:Bad Inode *******************************/

void bad_inode() {  
    int i;
    struct dinode *temp_dip = (struct dinode *)(disk_info.inode_block_start);
    for(i=0; i <= sb->ninodes; i++) {
        if(temp_dip->type != 0 && temp_dip->type != 1 && temp_dip->type != 2 && temp_dip->type != 3) {
            fprintf(stderr, "ERROR: bad inode.\n");
            exit(1);       
        }
        temp_dip++;
    }
}

/*******************************************************************************/

/******************************* Check 2,5,7 & 8 *******************************/
/*
    2 : Bad direct and indirect address
    5 : Address used by inode but marked free in bitmap
    7 : Direct Address used more than once
    8 : Indirect Address used more than once

*/
/*******************************************************************************/

void bad_addr() {   
    int i,j,k;
    struct dinode *temp_dip = (struct dinode *)(disk_info.inode_block_start);
    for (i = 1; i <= sb->ninodes; i++)
    {
        //printf("inode %d: %p\n",i,temp_dip);
        //printf("inode %d: %d\n",i,temp_dip->addrs[0]);
        if (temp_dip->type != 0) {

            for (j = 0; j < NDIRECT+1; j++)
            {   
                //printf("direct %d\n",temp_dip->addrs[j]);
                if (temp_dip->addrs[j] != 0)
                {
                    if(temp_dip->addrs[j] < 0 || temp_dip->addrs[j] >= (sb->size)) {
                        fprintf(stderr, "ERROR: bad direct address in inode.\n");
                        exit(1);
                    }

                    if (on_disk_bmap[temp_dip->addrs[j]] == 0) {
                        fprintf(stderr,"ERROR: address used by inode but marked free in bitmap.\n");
                        exit(1);
                    }

                    if (temp_dip->addrs[j] == addresses[temp_dip->addrs[j]] && temp_dip->addrs[j] != 0) {
                        fprintf(stderr,"ERROR: direct address used more than once.\n");
                        exit(1);
                    }

                    addresses[temp_dip->addrs[j]] = temp_dip->addrs[j];

                }
                                
            }   // End of for

            if (temp_dip->addrs[NDIRECT] < 0 || temp_dip->addrs[NDIRECT] >= (sb->size)) {
                fprintf(stderr, "ERROR: bad indirect address in inode.\n");
                exit(1);
            }

            unsigned int *indirect_blk =(unsigned int *)(disk_info.image_addr + ((temp_dip->addrs[NDIRECT]) * BSIZE));
            //printf("indirect-> %u \n", *indirect_blk);
            for (k = 0; k < NINDIRECT; k++) {   

                if (*indirect_blk != 0)
                {
                    //printf("indirect-> %d \n",*indirect_blk);
                    if (*indirect_blk < 0 || *indirect_blk >= (sb->size)) {
                        fprintf(stderr, "ERROR: bad indirect address in inode.\n");
                        exit(1);
                    }

                    if (on_disk_bmap[*indirect_blk] == 0) {
                        fprintf(stderr, "ERROR: address used by inode but marked free in bitmap.\n");
                        exit(1);
                    }

                    if (*indirect_blk == addresses[*indirect_blk] && *indirect_blk != 0) {
                        fprintf(stderr,"ERROR: indirect address used more than once.\n");
                        exit(1);
                    }
                
                    addresses[*indirect_blk] = *indirect_blk;
                    indirect_blk++;    
                }

            }   // End of for
            
        }        
        temp_dip++;
    }   // End of for 
}

/*******************************************************************************/

/*************** Traverse Directory and update inode bitmap ********************/

void traverse_dir(struct dinode *rootinode, int *inodemap)
{   
    int i,j;
    uint blockaddr;
    uint *indirect;
    struct dinode *inode;
    struct dirent *dir;
    
    if (rootinode->type == 1) {
        for (i = 0; i < NDIRECT; i++) { 
            blockaddr = rootinode->addrs[i];
            if (blockaddr == 0)
                continue;

            dir = (struct dirent *) (image_pointer + (blockaddr * BSIZE));
            for (j = 0; j < DPB; j++, dir++) {    
                if (dir->inum != 0 && strcmp(dir->name, ".") != 0 && strcmp(dir->name, "..") != 0) {
                    inode = ((struct dinode *) (image_pointer + 2 * BSIZE)) + (dir->inum);
                    inodemap[dir->inum]++;
                    traverse_dir(inode, inodemap);
                }
            }
        }

        blockaddr = rootinode->addrs[NDIRECT];
        if (blockaddr != 0) {
            indirect = (uint *) (image_pointer + blockaddr * BSIZE);
            for (i = 0; i < NINDIRECT; i++, indirect++) {
                blockaddr = *(indirect);
                if (blockaddr == 0)
                    continue;

                dir = (struct dirent *) (image_pointer + blockaddr * BSIZE);

                for (j = 0; j < DPB; j++, dir++) {
                    if (dir->inum != 0 && strcmp(dir->name, ".") != 0 && strcmp(dir->name, "..") != 0) {
                        inode = ((struct dinode *)(disk_info.inode_block_start)) + (dir->inum);
                        inodemap[dir->inum]++;
                        traverse_dir(inode, inodemap);
                    }
                }
            }
        }
    }
}

/*******************************************************************************/

/**************************** Check 9, 10, 11 & 12 *****************************/
/* 
    9 : inode marked use but not found in a directory.
    10 : inode referred to in directory but marked free.
    11 : bad reference count for file.
    12 : directory appears more than once in file system.
*/
/*******************************************************************************/

void dir_check(){   

    int i;
    int inodemap[sb->ninodes];
    memset(inodemap, 0, sizeof(int) * sb->ninodes);
    struct dinode *inode_temp, *rootinode;

    inode_temp = (struct dinode *)(disk_info.inode_block_start);
    rootinode = ++inode_temp;
   
    inodemap[0]++;
    inodemap[1]++;
   
    traverse_dir(rootinode, inodemap);

    inode_temp++;
    for (i = 2; i < sb->ninodes; i++) {

        if (inode_temp->type != 0 && inodemap[i] < 1) {
            fprintf(stderr,"ERROR: inode marked use but not found in a directory.\n");
            exit(1);
        }

        if (inodemap[i] > 0 && inode_temp->type == 0) {
            fprintf(stderr,"ERROR: inode referred to in directory but marked free.\n");
            exit(1);
        }

        if (inode_temp->type == 2 && inode_temp->nlink != inodemap[i]) {
            fprintf(stderr,"ERROR: bad reference count for file.\n");
            exit(1);
        }
   
        if (inode_temp->type == 1 && inodemap[i] > 1) {
            fprintf(stderr,"ERROR: directory appears more than once in file system.\n");
            exit(1);
        }
        inode_temp++;
    }
}

/*******************************************************************************/

/****************************** Check : 3, 4 ***********************************/
/*
    3 : root directory does not exist.
    4 : directory not properly formatted.
*/
/*******************************************************************************/

int dir_fmt(struct dinode *inode, int inum){    
    
    int x, y, parent=0, self=0;
    uint blockaddr;
    struct dirent *direcc;
   
    for (x = 0; x < NDIRECT; x++){
        blockaddr = inode->addrs[x];
        if (blockaddr == 0){
            continue;
        }

        direcc = (struct dirent *) (image_pointer + blockaddr * BSIZE);
        for (y = 0; y < DPB; y++, direcc++){
            if (!self && strcmp(".", direcc->name) == 0){
                self = 1;
                if (direcc->inum != inum){
                    return 1;
                }
            }

            if (!parent && strcmp("..", direcc->name) == 0){
                parent = 1;
                if (inum != 1 && direcc->inum == inum){
                    return 1;
                }
                if (inum == 1 && direcc->inum != inum){
                    return 1;
                }
            }
            if (parent && self){
                break;
            }
        }

        if (parent && self){
            break;
        }
    }

    if (!parent || !self){
        return 1;
    }
    return 0;
}

void dir_fmt_check(){   
    
    struct dinode *inode;
    int x, unalloc = 0;
    inode = (struct dinode *)(disk_info.inode_block_start);

    for(x = 0; x < sb->ninodes; x++, inode++){
        
        if (inode->type == 0){
            unalloc++;
            continue;
        }

        if (x == 1 && (inode->type != 1 || dir_fmt(inode, x) != 0)){
            fprintf(stderr,"ERROR: root directory does not exist.\n");
            exit(1);
        }

        if (inode->type == 1 && dir_fmt(inode, x) != 0){
            fprintf(stderr,"ERROR: directory not properly formatted.\n");
            exit(1);
        }
    }
    inode = NULL;
}

/*******************************************************************************/

/**************************** Bitmap Comparison ********************************/
/*
    Check 6 : bitmap marks block in use but it is not in use.
*/
/*******************************************************************************/

void bmap_check(){
    
    struct dinode *inode;
    int i,j,k;
    int dblcks[sb->nblocks];
    uint blockaddr;
    uint *indirect;
    memset(dblcks, 0, sb->nblocks * sizeof(int));

    inode = (struct dinode *)(disk_info.inode_block_start);
    for(i = 0; i < sb->ninodes; i++, inode++) {
        if (inode->type == 0) {
            continue;
        }

        for (k = 0; k < (NDIRECT + 1); k++) {
            blockaddr = inode->addrs[k];
            if (blockaddr == 0) {
                continue;
            }

            dblcks[blockaddr - disk_info.root_data_block] = 1;

            if (k == NDIRECT) {
                indirect = (uint *) (image_pointer + blockaddr * BSIZE);
                for (j = 0; j < NINDIRECT; j++, indirect++) {
                    blockaddr = *(indirect);
                
                    if (blockaddr == 0) {
                        continue;
                    }

                dblcks[blockaddr - disk_info.root_data_block] = 1;
                }   
            }
        }
    }
   
    for (i = 0; i < sb->nblocks; i++) {
        blockaddr = (uint) (i + disk_info.root_data_block);
        if (dblcks[i] == 0 && BITSET(disk_info.bitmap_block_start, blockaddr)) {
            fprintf(stderr, "ERROR: bitmap marks block in use but it is not in use.\n");
            exit(1);
        }
    }
    inode = NULL;
}

/*******************************************************************************/

/******************************* End of Code ***********************************/


