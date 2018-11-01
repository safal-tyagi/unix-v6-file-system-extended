#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

int BLOCK_SIZE = 1024;
int INODE_SIZE = 32;

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
    unsigned short addr[8];
    unsigned short actime[2];
    unsigned short modtime[2];
} INode;

// I-NODE FLAGS (FILE TYPE AND OTHER INFO)
//***************************************************************************
typedef struct{
    // Note: bit shift from 0th to 15th (actually 1 to 16 bits)
    unsigned short allocated: 1 << 15; // 16th: file allocated
    // 15th & 14th: file type (00: plain | 01: char type special | 10: directory | 11: block-type special)
    unsigned short fileType: 1 << 14; // a directory
    unsigned short largeFile: 0 << 12; // 13th bit: a large file (false)
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



//***************************************************************************
// InitFS: INITIALIZES THE FILE SYSTEM 
//***************************************************************************
/*
* @nBlocks: 
* @nInodes: 
* @fileSystem : 
*/
int InitFS(FILE* fileSystem, long nBlocks, int nInodes)
{
	char buffer[BLOCK_SIZE]; // char buffer to read/write the blocks to/from file

    // // DUMMY FILE SYSTEM INITIALIZATION
    // //***************************************************************************
    // // Initialize file system with zeros to all blocks using buffer
    long writtenBytes=0, i = 0;
	// for (i=0;i<nBlocks;i++){
	// 	writtenBytes += fwrite(buffer, BLOCK_SIZE , nBlocks, fileSystem);
    //     // printf("\nBytes (%i) written\n", writtenBytes); // to confirm the written bytes 
	// }
	// printf("\nBytes (%i) written\n", writtenBytes);
    
    // // Set the pointer again to the beginning of the file system
	// rewind(fileSystem); 

    // BOOT LOADER (FIRST) BLOCK
    //***************************************************************************
    // Fill with 0s
    memset(buffer, 0, BLOCK_SIZE);

    // SUPER-BLOCK: max size 1024
    //***************************************************************************
    SuperBlock superBlock;

    // unsigned short isize;
    // unsigned short fsize;
    // unsigned short nfree;
    // unsigned short free[250];
    // unsigned short ninode;
    // unsigned short inode[250];
    // char flock;
    // char ilock;
    // char fmod;
    // unsigned short time[2];

    // Calculate i-node blocks requirement
    // Note: 1024 block size = 32 i-nodes of 32 bytes
    int nInodeBlocks = 0;
    if (nInodes % 32 == 0) // parfect fit to blocks
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

    superBlock.time[0] = (short) time(0); // current time
    superBlock.time[1] = 0; //empty 

    // Write super block to file system
    fseek(fileSystem, BLOCK_SIZE, SEEK_SET);
    fwrite(&superBlock, sizeof(superBlock), 1, fileSystem); // Note: sizeof(superBlock) <= BLOCK_SIZE
    //***************************************************************************

    
    // CHAIN DATA BLOCKS: To be done [SKT] or [PRV]
    //***************************************************************************
    // https://github.com/pradeepananth/UNIXFileSystemRedesign/blob/master/fsaccess.c
    // chaindatablocks(total_blcks);


    // SUPER-BLOCK: FREE LIST
    //***************************************************************************
    // first block + superBlock + inode blocks
	int firstFreeBlock = 2+nInodeBlocks; 
	int nextFreeBlock;

	// Initialize free blocks
	for (nextFreeBlock=firstFreeBlock;nextFreeBlock<nBlocks; nextFreeBlock++ ){ 
        //printf("\nCall AddFreeBlock for block: %i",nextFreeBlock); 
        AddFreeBlock(nextFreeBlock, fileSystem); // To be updated [SKT]
	}

    // ROOT DIRECTORY INODE: To be updated [SKT]
    //***************************************************************************

    INode inode; // inode object 
	INodeFlags flags; // flags to manage file type
	FileName fileName; // file structure

	// Need to Initialize only first i-node and root directory file will point to it
	INode rootInode;
	rootInode.flags=0;       //Initialize
	rootInode.flags |=1 <<15; //Set first bit to 1 - i-node is allocated
	rootInode.flags |=1 <<14; // Set 2-3 bits to 10 - i-node is directory
	rootInode.flags |=0 <<13;
	rootInode.nlinks=0;
	rootInode.uid=0;
	rootInode.gid=0;
	rootInode.size0=0;
	rootInode.size1=16*2; //File size is two records each is 16 bytes.
	rootInode.addr[0]=0;
	rootInode.addr[1]=0;
	rootInode.addr[2]=0;
	rootInode.addr[3]=0;
	rootInode.addr[4]=0;
	rootInode.addr[5]=0;
	rootInode.addr[6]=0;
	rootInode.addr[7]=0;
	rootInode.actime[0]=0;
	rootInode.actime[1]=0;
	rootInode.modtime[0]=0;
	rootInode.modtime[1]=0;

	// Create root directory file and initialize with "." and ".." Set offset to 1st i-node
	fileName.inodeOffset = 1;
	strcpy(fileName.fileName, ".");
	int AllocateBlock = firstFreeBlock-1;      // Allocate block for file_directory
	fseek(fileSystem,AllocateBlock*BLOCK_SIZE,SEEK_SET); //move to the beginning of first available block
	fwrite(&fileName,16,1,fileSystem);
	strcpy(fileName.fileName, "..");
	fwrite(&fileName,16,1,fileSystem);

	//Point first inode to file directory
	printf("\nDirectory in the block %i",AllocateBlock);
	rootInode.addr[0]=AllocateBlock;
	//write_inode(1,rootInode,fileSystem);
	// write_inode function (*)
	int INodeNumber = 1;
    fseek(fileSystem,(BLOCK_SIZE*2+INodeSize*(INodeNumber-1)),SEEK_SET); //move to the beginning of inode with INodeNumber
	fwrite(&rootInode,INodeSize,1,fileSystem);

	return 0;
}

void AddFreeBlock(long BlockNumber, FILE* fileSystem){
	SuperBlock superBlock;
	FreeBlock CopyToBlock;
	fseek(fileSystem, BLOCK_SIZE, SEEK_SET);
	fread(&superBlock,sizeof(superBlock),1,fileSystem);
	if (superBlock.nfree == 250){ //free array is full - copy content of superBlock to new block and point to it
        //printf("\nCopy free list to block %i",BlockNumber);
        CopyToBlock.nfree=250;
        //copy_int_array(superBlock.free, CopyToBlock.free, 200);
        //copy_int_array function (*)
        int i;
        for (i=0;i<250;i++){
            CopyToBlock.free[i] = superBlock.free[i];
        }
        fseek(fileSystem,BlockNumber*BLOCK_SIZE,SEEK_SET);
        fwrite(&CopyToBlock,sizeof(CopyToBlock),1,fileSystem);
        superBlock.nfree = 1;
        superBlock.free[0] = BlockNumber;
	}
	else {
			superBlock.free[superBlock.nfree] = BlockNumber;
			superBlock.nfree++;
			}
	fseek(fileSystem, BLOCK_SIZE, SEEK_SET);
	fwrite(&superBlock,sizeof(superBlock),1,fileSystem);
}

//***************************************************************************
// MAIN
//***************************************************************************
int main()
{
    printf("\n## WELCOME TO UNIX V6 FILE SYSTEM ##\n");
    printf("\n## PLEASE ENTER ONE OF THE FOLLOWING COMMAND ##\n");
	printf("\n\t ~InitFS - To initialize the file system \n");
	printf("\t\t ~Example :: InitFS /home/venky/behappy.txt 5000 300 \n");
	printf("\n\t ~q - To quit the program\n");

    int cmdStatus; // status of file system creation
    FILE* fileSystem = NULL; // filesystem descriptor
    int cmdCount = 0; // cmdCount of commands

     // Accept userCmd, max lenght of userCmd : 256 characters
    char userCmd[256];

    while(1) {
        //While loop, wait for user input
        printf("Enter userCmd : ");
        cmdCount++;

		// Getting userCmd from user
		scanf(" %[^\n]s", userCmd);

        char * command; // function to execute
        char * arg; // arguments to function

        // In case user typed userCmd with argument, split to userCmd and argument
        command = strtok (userCmd," ,\n");
        if (command != NULL){
            arg = strtok (NULL, "\n");
            }

        // if user press q, exit saving changes
        if (strcmp(userCmd,"q")==0){
            printf("Total commands executed : %i\n",cmdCount);
            fclose(fileSystem);
            exit(0);
        }

        // else initilize file system
        if (strcmp(userCmd,"InitFS")==0){
            char *fileName;
            char *p;
            fileName = strtok (arg, " ");
            
            printf("FileSystem creation initiated: %s\n",fileName);
            p = strtok (NULL, " ");
            long nBlocks = atoi(p);
            p = strtok (NULL, " "); 
            int nInodes = atoi(p);

            // check if the file exists
            if( access( fileName, F_OK ) != -1 ) { // if exists
                printf("\nFile opens %s \n",fileName);
                fileSystem = fopen(fileName, "r+");
                }
            else { // file doesn't exist
                fileSystem = fopen(fileName, "w+");
                printf("\nCreating new file system for the file %s .\n",fileName);
                }

            if (nBlocks > 4194304) { // max allowed file size: 4GB
                printf("\nFAILED! Max allowed number of blocks: 4194304\n");
                }
            else { 
                // if less than 4GB: INITIALIZE file system
                cmdStatus = InitFS(fileSystem, nBlocks, nInodes);

                if (cmdStatus == 0)
                    printf("\nFile system initialization SUCCESSFUL!\n");
                else
                    printf("\nFile system initialization FAILED!\n");
                }
        }

      else
      	printf("FAILED: Invalid Command. Accepted commands: InitFS and q.\n");

    }
}

