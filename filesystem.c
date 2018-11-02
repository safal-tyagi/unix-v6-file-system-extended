#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <sys/types.h>

const int BLOCK_SIZE = 1024;
const int INODE_SIZE = 32;

//***************************************************************************
// UNIX V6 FILE SYSTEM STRUCTURES
//***************************************************************************
// SUPER-BLOCK
//***************************************************************************
typedef struct {
    unsigned short isize;
    unsigned short fsize;
    unsigned short nfree;
    unsigned short free[250];
    unsigned short ninode;
    unsigned short inode[250];
    char flock;
    char ilock;
    char fmod;
    unsigned short time[2];
} SuperBlock;

// I-NODE
//***************************************************************************
typedef struct {
    unsigned short flags;
    char nlinks;
    char uid;
    char gid;
    char size0;
    unsigned short size1;
    unsigned short addr[24];
    unsigned short actime[2];
    unsigned short modtime[2];
} INode;

// I-NODE FLAGS (FILE TYPE AND OTHER INFO)
//***************************************************************************
typedef struct{
    unsigned short allocated; // 16th: file allocated
    // 15th & 14th: file type (00: plain | 01: char type special | 10: directory | 11: block-type special)
    unsigned short fileType; // a directory
    unsigned short largeFile; // 13th bit: a large file
    // other flags : NOT USED
    // 12th: user id | 11th: group id | 10th: none
    // 9th: read (owner) | 8th: write (owner) | 8th: exec (owner)
    // 6th: read (group) | 5th: write (group) | 4th: exec (group)
    // 3rd: read (others) | 2nd: write (others) | 1st: exec (others)
} INodeFlags;

//***************************************************************************
// OTHER HELPER STRUCTURES
//***************************************************************************
// FREE BLOCK
//***************************************************************************
typedef struct {
    unsigned short nfree;
    unsigned short free[250];
} FreeBlock;

// FILE NAME
//***************************************************************************
typedef struct {
    unsigned short inodeOffset;
    char fileName[14];
} FileName;


int InitFS(int fileDesc, long nBlocks, int nInodes);
void AddFreeBlock(int fileDesc, long blockNumber);


//***************************************************************************
// InitFS: INITIALIZES THE FILE SYSTEM
//***************************************************************************
/*
* @fileDesc: file descriptor
* @nBlocks: total blocks of file system
* @nInodes: number of inodes
*/
int InitFS(int fileDesc, long nBlocks, int nInodes)
{
    char buffer[BLOCK_SIZE]; // char buffer to read/write the blocks to/from file
    long writtenBytes=0, i = 0;

    // BOOT LOADER (FIRST) BLOCK
    memset(buffer, 0, BLOCK_SIZE);

    // SUPER-BLOCK: max size 1024
    SuperBlock superBlock;

    // Calculate i-node blocks requirement | Note: 1024 block size = 32 i-nodes of 32 bytes
    int nInodeBlocks = 0;
    if (nInodes % 32 == 0) // perfect fit to blocks
        nInodeBlocks = nInodes / 32;
    else
        nInodeBlocks = nInodes / 32 + 1;

    superBlock.isize = nInodeBlocks;
    superBlock.fsize = nBlocks; // max allowed allocation

    // Set number of free-blocks and free-list to nulls
    superBlock.nfree = 0;
    for (i = 0; i < 250; i++)
        superBlock.free[i] = 0;

    // Set number of i-nodes and i-node list to nulls
    superBlock.ninode = 250;
    for (i = 0; i < 250; i++)
        superBlock.inode[i] = 0;

    superBlock.flock = "flock"; // dummy
    superBlock.ilock = "ilock"; // dummy
    superBlock.fmod = "fmod"; // dummy

    superBlock.time[0] = (unsigned short) time(0); // current time
    superBlock.time[1] = 0; //empty

    // Write super block to file system
    lseek(fileDesc, BLOCK_SIZE, SEEK_SET);
    write(fileDesc, &superBlock, sizeof(superBlock)); // Note: sizeof(superBlock) <= BLOCK_SIZE

    // SUPER-BLOCK: FREE LIST
    // first free block = bootBlock + superBlock + inodeBlocks
    int firstFreeBlock = 2 + nInodeBlocks + 1; // +1 as bootblock is 0th
    int nextFreeBlock;

    // TEST ONLY: fill blocks with 0s to confirm size
    char charbuffer[BLOCK_SIZE];
    memset(charbuffer, 0, BLOCK_SIZE);
    for (nextFreeBlock = firstFreeBlock; nextFreeBlock < nBlocks; nextFreeBlock++ ) {
        lseek(fileDesc, nextFreeBlock * BLOCK_SIZE, SEEK_SET);
        write(fileDesc, charbuffer, BLOCK_SIZE);
    }

    // Initialize free blocks
    for (nextFreeBlock = firstFreeBlock; nextFreeBlock < nBlocks; nextFreeBlock++ ){
        //printf("\nCall AddFreeBlock for block: %i",nextFreeBlock); // TO DEBUG
        AddFreeBlock(fileDesc, nextFreeBlock);
    }

    // ROOT DIRECTORY INODE
    INode inode; // inode object
    INodeFlags flags; // flags to manage file type
    FileName fileName; // file structure

    // Update only first i-node as root node, remainig to be done in Project2
    INode rootInode;
    rootInode.flags = 0; //Initialize
    rootInode.flags |=1 <<15; //Set first bit to 1 - i-node is allocated
    rootInode.flags |=1 <<14; // Set 2-3 bits to 10 - i-node is directory
    rootInode.nlinks = 2;
    rootInode.uid=0;
    rootInode.gid=0;
    rootInode.size0=0;
    rootInode.size1=16*2; //File size is two records each is 16 bytes.
    rootInode.addr[0]=firstFreeBlock - 1;
    for (int i = 1; i < 8; i++)
        rootInode.addr[i] = 0;
    for (int i = 1; i < 2; i++)
        rootInode.actime[i]=(short) time(0);
    for (int i = 1; i < 2; i++)
        rootInode.modtime[i]=(short) time(0);

    // Initialize with "." and ".." Set offset to 1st i-node
    fileName.inodeOffset = 1;
    strcpy(fileName.fileName, ".");
    int allocBlock = firstFreeBlock-1;      // Allocate block for file_directory
    lseek(fileDesc, allocBlock*BLOCK_SIZE, SEEK_SET); //move to the beginning of first available block
    write(fileDesc, &fileName,sizeof(fileName));
    strcpy(fileName.fileName, "..");
    write(fileDesc, &fileName,sizeof(fileName));

    //Point first inode to file directory
    printf("\nDirectory in the block %i", allocBlock);
    rootInode.addr[0] = allocBlock;
    lseek(fileDesc, BLOCK_SIZE*2, SEEK_SET); //move to the beginning of inode with INodeNumber
    write(fileDesc, &rootInode,sizeof(rootInode));

    return 0;
}

//***************************************************************************
// AddFreeBlock: Adds a free block and update the super-block accordingly
//***************************************************************************
void AddFreeBlock(int fileDesc, long blockNumber)
{
    SuperBlock superBlock;
    FreeBlock copyToBlock;

    // read and update existing superblock
    lseek(fileDesc, BLOCK_SIZE, SEEK_SET);
    read(fileDesc, &superBlock, sizeof(superBlock));

    // if free array is full
    // copy content of superBlock to new block and point to it
    if (superBlock.nfree == 250){
        //printf("\nCopy free list to block %i",blockNumber);
        copyToBlock.nfree=250;
        for (int i = 0; i < 250; i++)
            copyToBlock.free[i] = superBlock.free[i];
        lseek(fileDesc,blockNumber*BLOCK_SIZE,SEEK_SET);
        write(fileDesc, &copyToBlock,sizeof(copyToBlock));
        superBlock.nfree = 1;
        superBlock.free[0] = blockNumber;
    }
    else { // free array is NOT full
        superBlock.free[superBlock.nfree] = blockNumber;
        superBlock.nfree++;
    }

    // write updated superblock to filesystem
    lseek(fileDesc, BLOCK_SIZE, SEEK_SET);
    write(fileDesc, &superBlock, sizeof(superBlock));
}

//***************************************************************************
// MAIN: driver
//***************************************************************************
int main()
{
    printf("\n## WELCOME TO UNIX V6 FILE SYSTEM ##\n");
    printf("\n## PLEASE ENTER ONE OF THE FOLLOWING COMMAND ##\n");
    printf("\n\t ~initfs - To initialize the file system \n");
    printf("\t\t ~Example :: initfs /home/venky/behappy.txt 5000 300 \n");
    printf("\n\t ~q - To quit the program\n");

    int cmdStatus; // status of file system creation
    int fileDesc = 0; // filesystem descriptor
    int cmdCount = 0; // cmdCount of commands

    // Accept userCmd, max lenght of userCmd : 256 characters
    char userCmd[256];

    while(1) { // wait for user input: 'q' command
        printf("Enter Command : ");
        cmdCount++;

        // get userCmd from user line-by-line
        scanf(" %[^\n]s", userCmd);

        // if user press q, exit saving changes
        if (strcmp(userCmd, "q")==0){
            printf("Total commands executed : %i\n",cmdCount);
            exit(0);
        }

        char * pCommand; // function to execute
        pCommand = strtok (userCmd, " ");

        // if pCommand is InitFS initilize file system
        if (strcmp(pCommand, "initfs")==0) {
            char * pFilePath = strtok (NULL, " ");
            char * pnBlocks = strtok (NULL, " ");
            char * pnInode = strtok (NULL, " ");
            long nBlocks = atoi(pnBlocks);
            int nInodes = atoi(pnInode);

            printf("FileSystem creation initiated: %s\n",pFilePath);

            if (nBlocks > 4194304) {// max allowed file size: 4GB
                printf("\nFAILED! Max allowed number of blocks: 4194304\n");
                exit(0);
            }

            // check if the file exists
            if( access( pFilePath, F_OK ) != -1 ) { // if exists
                fileDesc = open(pFilePath, O_RDWR);
                if (fileDesc == -1) {
                    printf("\nERROR: Not sufficient file descriptors.\n");
                    exit(0);
                }
                else
                    printf("\nFile opens %s \n",pFilePath);
            }
            else { // file doesn't exist
                fileDesc = open(pFilePath, O_RDWR | O_CREAT);
                if (fileDesc == -1) {
                    printf("\nERROR: Not sufficient file descriptors.\n");
                    exit(0);
                }
                else
                    printf("\nCreating new file system for the file %s .\n",pFilePath);
            }

            // if less than 4GB: INITIALIZE file system
            cmdStatus = InitFS(fileDesc, nBlocks, nInodes);
            if (cmdStatus == 0)
                printf("\nFile system initialization SUCCESSFUL!\n");
            else
                printf("\nFile system initialization FAILED!\n");
        }
        else
            printf("FAILED: Invalid Command. Accepted commands: InitFS and q.\n");
    }
}


