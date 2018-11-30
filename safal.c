#include <sys/types.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <math.h>
#include <time.h>

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
    unsigned short addr[22];
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

// Primary methods
int InitializeFS(int nBlocks, int nInodes);
int CopyIN(const char* extFile,const char* fsFile);
int CopyOUT(const char* fsFile,const char* extFile);
int MakeDirectory(const char* dirName);
int Remove(const char* filename);

// Directory
int AddFileToDirectory(const char* fsFile);
void RemoveFileFromDirectory(int fileInodeNum);

// Block management
void AddFreeBlock(int blockNumber);
int GetFreeBlock();
void CopyFreeArray(unsigned short *from_array, unsigned short *to_array, int buf_len);
unsigned short GetBlockSmall(int fileInodeNum,int block_number_order);
unsigned short GetBlockLarge(int fileInodeNum,int block_number_order);
void AddBlockToInodeSmall(int block_order_num, int blockNumber, int inodeNumber);
void AddBlockToInodeLarge(int block_order_num, int blockNumber, int inodeNumber);
unsigned int GetFileSize(int inodeNumber);

// Inode management
int GetFileInode(const char* fsFile);
int UpdateInode(int inode_num, INode inode);
INode GetInodeData(int inodeNumber);
INode InitInode(int inodeNumber, unsigned int file_size);

#define LAST(k,n) ((k) & ((1<<(n))-1))
#define MID(k,m,n) LAST((k)>>(m),((n)-(m)))

int BLOCK_SIZE=1024;
int inode_size=64;

int fsDesc;        //file descriptor
SuperBlock superBlock; // SUPER-BLOCK: max size 1024


//***************************************************************************
// MAIN: driver
//***************************************************************************
int main() {
    printf("\n## WELCOME TO UNIX V6 FILE SYSTEM ##\n");
    printf("\n## PLEASE ENTER ONE OF THE FOLLOWING COMMAND ##\n");
    printf("\n\t ~initfs - To initialize the file system \n");
    printf("\t\t ~Example :: initfs /home/venky/disk 5000 300 \n");
    printf("\n\t ~q - To quit the program\n");

    short fsStatus; // status of file system creation
    short cmdStatus; // status of command execution
    int cmdCount = 0; // cmdCount of commands

    // Accept userCmd, max lenght of userCmd : 256 characters
    char userCmd[256];

    while (1) { // wait for user input: 'q' command
        printf("Enter Command : ");
        cmdCount++;

        // get userCmd from user line-by-line
        scanf(" %[^\n]s", userCmd);

        // if user press q, exit saving changes
        if (strcmp(userCmd, "q") == 0) {
            printf("Total commands executed : %i\n", cmdCount);
            exit(0);
        }

        char *pCommand; // function to execute
        pCommand = strtok(userCmd, " ");

        // if pCommand is InitFS initilize file system
        if (strcmp(pCommand, "initfs") == 0) {
            char *pFilePath = strtok(NULL, " ");
            char *pnBlocks = strtok(NULL, " ");
            char *pnInode = strtok(NULL, " ");
            long nBlocks = atoi(pnBlocks);
            int nInodes = atoi(pnInode);

            if (nBlocks > 4194304) {// max allowed file size: 4GB
                printf("\nFAILED! Max allowed number of blocks: 4194304\n");
                exit(0);
            }

            // check if the file exists
            if (access(pFilePath, F_OK) != -1) { // if exists
                printf("\nFile System already exists %s.", pFilePath);
                fsDesc = open(pFilePath, O_RDWR);
                if (fsDesc == -1) {
                    printf("\nERROR: Unable to open file system.");
                    exit(0);
                } else {
                    // Load super block
                    lseek(fsDesc, BLOCK_SIZE, SEEK_SET);
                    read(fsDesc, &superBlock, BLOCK_SIZE); // Note: sizeof(superBlock) <= BLOCK_SIZE
                    fsStatus = 0;
                    printf("\nOpened. Continue.\n");
                }
            } else { // file doesn't exist
                fsDesc = open(pFilePath, O_CREAT | O_RDWR, S_IWUSR | S_IRUSR);
                if (fsDesc == -1) {
                    printf("\nERROR: Not sufficient file descriptors.\n");
                    exit(0);
                } else {
                    printf("\nCreating new file system at %s .\n", pFilePath);
                    // if less than 4GB: INITIALIZE file system
                    fsStatus = InitializeFS(nBlocks, nInodes);
                    if (fsStatus == 0)
                        printf("\nFile system initialization SUCCESSFUL!\n");
                    else
                        printf("\nFile system initialization FAILED!\n");
                }
            }
        }
            // if pCommand is cpin initilize file system
        else if (strcmp(pCommand, "cpin") == 0) {
            if (fsStatus != 0)
                printf("Error! Launch (initfs) first to open/create file system\n");
            else {
                char *pExtFile = strtok(NULL, " ");
                char *pFSFile = strtok(NULL, " ");

                if (strlen(pFSFile) > 14) {
                    printf("Target file name %s is TOO LONG: %i, maximum allowed length: 14", pFSFile, strlen(pFSFile));
                    continue;
                } else {
                    CopyIN(pExtFile, pFSFile);
                    printf("\nSUCCESS! File imported\n");
                }
            }

        } else if (strcmp(pCommand, "cpout") == 0) {
            if (fsStatus != 0)
                printf("Error! Launch (initfs) first to open/create file system\n");
            else {
                char *pFSFile = strtok(NULL, " ");
                char *pExtFile = strtok(NULL, " ");

                CopyOUT(pFSFile, pExtFile);
                printf("\nSUCCESS! File exported\n");
            }
        } else if (strcmp(pCommand, "mkdir") == 0) {
            if (fsStatus != 0)
                printf("Error! Launch (initfs) first to open/create file system\n");
            else {
                char *pDir = strtok(NULL, " ");
                MakeDirectory(pDir);
                printf("\nSUCCESS! Make directory\n");
            }
        }  else if (strcmp(pCommand, "rm") == 0) {
            if (fsStatus != 0)
                printf("Error! Launch (initfs) first to open/create file system\n");
            else {
                char *pFile = strtok(NULL, " ");
                Remove(pFile);
                printf("\nSUCCESS! Remove\n");
            }
        } else
            printf("FAILED: Invalid Command. Accepted commands: InitFS and q.\n");
    }
}

//***************************************************************************
// InitializeFS
//***************************************************************************
int InitializeFS(int nBlocks, int nInodes)
{
    long i = 0;

    // SUPER-BLOCK: max size 1024
    SuperBlock superBlock;

    // TEST ONLY: fill blocks with 0s to confirm size
    char charbuffer[BLOCK_SIZE];  // char buffer to read/write the blocks to/from file
    memset(charbuffer, 0, BLOCK_SIZE);
    for (i = 0; i < nBlocks; i++ ) {
        lseek(fsDesc, i * BLOCK_SIZE, SEEK_SET);
        write(fsDesc, charbuffer, BLOCK_SIZE);
    }

    // Calculate i-node blocks requirement | Note: 1024 block size = 16 i-nodes of 64 bytes
    int number_inode_blocks = 0;
    if (nInodes % 16 == 0) // perfect fit to blocks
        number_inode_blocks = nInodes / 16;
    else
        number_inode_blocks = nInodes / 16 + 1;
    printf("\nInitialize file_system with %i number of blocks and %i number of i-nodes\n",nBlocks,nInodes);

    superBlock.isize = number_inode_blocks;
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
    lseek(fsDesc, BLOCK_SIZE, SEEK_SET);
    write(fsDesc, &superBlock, sizeof(superBlock)); // Note: sizeof(superBlock) <= BLOCK_SIZE

    // SUPER-BLOCK: FREE LIST
    // first free block = bootBlock + superBlock + inodeBlocks
    int firstFreeBlock = 2 + number_inode_blocks + 1; // +1 for the root directory
    int next_free_block;

    // Initialize free blocks
    for (next_free_block=0;next_free_block<nBlocks; next_free_block++ ){
        AddFreeBlock(next_free_block);
    }

    //Initialize free i-node list
    superBlock.ninode=249;
    int next_free_inode=1; //First i-node is reserved for Root Directory
    for (i=0;i<=249;i++){
        superBlock.inode[i]=next_free_inode;
        next_free_inode++;
    }
    //Go to beginning of the second block
    lseek(fsDesc,BLOCK_SIZE,SEEK_SET);
    write(fsDesc,&superBlock,BLOCK_SIZE);

    // ROOT DIRECTORY INODE
    //***************************************************************************
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
    rootInode.size1=0; //File size is two records each is 16 bytes.
    int a = 0;
    for (a = 1; a < 23; a++)
        rootInode.addr[a] = 0;
    for (a = 0; a < 2; a++)
        rootInode.actime[a]=0;
    for (a = 0; a < 2; a++)
        rootInode.modtime[a]=0;

    // Create root directory file and initialize with "." and ".." Set offset to 1st i-node
    fileName.inodeOffset = 1;
    strcpy(fileName.fileName, ".");
    int allocBlock = firstFreeBlock-1;      // Allocate block for file_directory
    lseek(fsDesc,allocBlock*BLOCK_SIZE,SEEK_SET); //move to the beginning of first available block
    write(fsDesc,&fileName,16);
    strcpy(fileName.fileName, "..");
    write(fsDesc,&fileName,16);

    //Point first inode to file directory
    printf("\nDirectory in the block %i",allocBlock);
    rootInode.addr[0]=allocBlock; // Allocate block to file directory
    UpdateInode(1,rootInode);

    return 0;
}

//***************************************************************************
// CopyIN
//***************************************************************************
int CopyIN(const char* extFile,const char* fsFile){
    printf("\nInside cpin, copy from %s to %s \n",extFile, fsFile);
    INode iNode;
    int newblocknum;
    unsigned long file_size;
    unsigned char reader[BLOCK_SIZE];

    int fileInodeNum = GetFileInode(fsFile);
    if (fileInodeNum !=-1){
        printf("\nFile %s already exists. Choose different name",fsFile);
        return -1;
    }

    //open external file
    int srcfd = -1;
    if ((srcfd = open(extFile, O_RDONLY)) == -1) {
        printf("\nError opening file: %s \n", extFile);
        return -1;
    }
    // confirm size
    struct stat srcInfo;
    fstat(srcfd, &srcInfo);
    file_size = srcInfo.st_size;

    if (file_size > pow(2,32)){
        printf("\nThe file is too big %s, maximum supported size is 4GB",file_size);
        return 0;
    }

    //Add file name to directory
    int inodeNumber = AddFileToDirectory(fsFile);
    //Initialize and load inode of new file
    iNode = InitInode(inodeNumber,file_size);
    UpdateInode(inodeNumber, iNode);

    //Copy content of extFile into to_file_name
    int num_blocks_read=1;
    int total_num_blocks=0;
    lseek(srcfd, 0L, SEEK_SET);	 // Move to beginning of the input file
    int block_order=0;
    while(num_blocks_read == 1){
        // Read one block at a time from source file
        num_blocks_read = fread(&reader,BLOCK_SIZE,1,srcfd);
        total_num_blocks+= num_blocks_read;
        newblocknum = GetFreeBlock();

        if (newblocknum == -1){
            printf("\nNo free blocks left. Total blocks read so far:%i",total_num_blocks);
            return -1;
        }
        if (file_size > BLOCK_SIZE*23)
            AddBlockToInodeLarge(block_order,newblocknum, inodeNumber);
        else
            AddBlockToInodeSmall(block_order,newblocknum, inodeNumber);
        // Write one block at a time into target file
        lseek(fsDesc, newblocknum*BLOCK_SIZE, SEEK_SET);
        write(fsDesc, &reader, sizeof(reader));
        block_order++;
    }
    return 0;
}

//***************************************************************************
// CopyOUT
//***************************************************************************
int CopyOUT(const char* fsFile,const char* extFile){
    int fileInodeNum;
    fileInodeNum = GetFileInode(fsFile);
    if (fileInodeNum ==-1){
        printf("\nFile %s not found",fsFile);
        return -1;
    }
    int extDesc = -1;
    unsigned char buffer[BLOCK_SIZE];
    int next_block_number, block_number_order, number_of_blocks,file_size;
    extDesc = fopen(extFile,"w");
    block_number_order=0;
    file_size = GetFileSize(fileInodeNum);

    printf("\nFile size %i",file_size);
    int number_of_bytes_last_block = file_size%BLOCK_SIZE;
    unsigned char last_buffer[number_of_bytes_last_block];
    if (number_of_bytes_last_block ==0){
        number_of_blocks = file_size/BLOCK_SIZE;
    }
    else
        number_of_blocks = file_size/BLOCK_SIZE +1; //The last block is not full

    while(block_number_order < number_of_blocks){
        if (file_size > BLOCK_SIZE*23)
            next_block_number = GetBlockLarge(fileInodeNum,block_number_order);
        else
            next_block_number = GetBlockSmall(fileInodeNum,block_number_order);

        if(next_block_number == 0)
            return 0;

        lseek(fsDesc, next_block_number*BLOCK_SIZE, SEEK_SET);
        if ((block_number_order <(number_of_blocks-1)) || (number_of_bytes_last_block ==0)){
            read(fsDesc, buffer,sizeof(buffer));
            write(extDesc, buffer,sizeof(buffer));
        }
        else {
            read(fsDesc, last_buffer,sizeof(last_buffer));
            write(extDesc, last_buffer,sizeof(last_buffer));
        }
        block_number_order++;
    }

    close(extDesc);
    return 0;
}

//***************************************************************************
// MakeDirectory
//***************************************************************************
int MakeDirectory(const char* dirName){
    INode directory_inode, free_node;
    FileName fileName;
    int inodeNumber, flag, fileInodeNum;

    fileInodeNum = GetFileInode(dirName);
    if (fileInodeNum !=-1){
        printf("\nDirectory %s already exists. Choose different name",dirName);
        return -1;
    }

    int found = 0;
    inodeNumber=1;
    while(found==0){
        inodeNumber++;
        free_node = GetInodeData(inodeNumber);
        flag = MID(free_node.flags,15,16);
        if (flag == 0){
            found = 1;
        }
    }

    directory_inode = GetInodeData(1);

    // Move to the end of directory file
    printf("\nDirectory node block number is %i",directory_inode.addr[0]);
    lseek(fsDesc,(BLOCK_SIZE*directory_inode.addr[0]+directory_inode.size1),SEEK_SET);
    // Add record to file directory
    fileName.inodeOffset = inodeNumber;
    strcpy(fileName.fileName, dirName);
    write(fsDesc, &fileName,16);

    //Update Directory file inode to increment size by one record
    directory_inode.size1+=16;
    UpdateInode(1,directory_inode);

    //Initialize new directory file-node
    free_node.flags=0;       //Initialize
    free_node.flags |=1 <<15; //Set first bit to 1 - i-node is allocated
    free_node.flags |=1 <<14; // Set 2-3 bits to 10 - i-node is directory
    free_node.flags |=0 <<13;

    UpdateInode(inodeNumber, free_node);

    return 0;
}

//***************************************************************************
// Remove
//***************************************************************************
int Remove(const char* filename){
    printf("\nInside Remove, remove file %s",filename);
    int fileInodeNum, file_size, block_number_order, next_block_number,i;
    INode file_inode;
    unsigned char bit_14; //Plain file or Directory bit
    fileInodeNum = GetFileInode(filename);
    if (fileInodeNum ==-1){
        printf("\nFile %s not found",filename);
        return -1;
    }
    file_inode = GetInodeData(fileInodeNum);
    bit_14 = MID(file_inode.flags,14,15);
    if (bit_14 == 0){ //Plain file
        printf("\nRemove Plain file");
        file_size = GetFileSize(fileInodeNum);
        block_number_order = file_size/BLOCK_SIZE;
        if (file_size%BLOCK_SIZE !=0)
            block_number_order++;  //Take care of truncation of integer devision

        block_number_order--; //Order starts from zero
        while(block_number_order>0){
            if (file_size > BLOCK_SIZE*23)
                next_block_number = GetBlockLarge(fileInodeNum,block_number_order);
            else
                next_block_number = file_inode.addr[block_number_order];

            AddFreeBlock(next_block_number);
            block_number_order--;
        }
    }
    file_inode.flags=0; //Initialize inode flag and make it unallocated
    for (i = 0;i<23;i++)
        file_inode.addr[i] = 0;
    UpdateInode(fileInodeNum, file_inode);
    RemoveFileFromDirectory(fileInodeNum);
    fflush(fdopen(fsDesc, 'r+'));
    return 0;
}


//***************************************************************************
// AddFileToDirectory
//***************************************************************************
int AddFileToDirectory(const char* fsFile){
    INode directory_inode, free_node;
    FileName fileName;
    int inodeNumber, flag;
    int found = 0;
    inodeNumber=1;
    while(found==0){
        inodeNumber++;
        free_node = GetInodeData(inodeNumber);
        flag = MID(free_node.flags,15,16);
        if (flag == 0){
            found = 1;
        }
    }

    directory_inode = GetInodeData(1);

    // Move to the end of directory file
    lseek(fsDesc,(BLOCK_SIZE*directory_inode.addr[0]+directory_inode.size1),SEEK_SET);
    // Add record to file directory
    fileName.inodeOffset = inodeNumber;
    strcpy(fileName.fileName, fsFile);
    write(fsDesc,&fileName,16);

    //Update Directory file inode to increment size by one record
    directory_inode.size1+=16;
    UpdateInode(1,directory_inode);

    return inodeNumber;

}

//***************************************************************************
// RemoveFileFromDirectory
//***************************************************************************
void RemoveFileFromDirectory(int fileInodeNum){
    INode directory_inode;
    FileName fileName;

    directory_inode = GetInodeData(1);

    // Move to the beginning of directory file (3rd record)
    lseek(fsDesc,(BLOCK_SIZE*directory_inode.addr[0]),SEEK_SET); //Move to third record in directory
    int records=(BLOCK_SIZE-2)/sizeof(fileName);
    int i;
    read(fsDesc,&fileName,sizeof(fileName));

    for(i=0;i<records; i++){

        if (fileName.inodeOffset == fileInodeNum){
            printf("File found! removing from the directory.");
            lseek(fsDesc, (-1)*sizeof(fileName), SEEK_CUR); //Go one record back
            fileName.inodeOffset = 0;
            memset(fileName.fileName,0,sizeof(fileName.fileName));
            write(fsDesc, &fileName,sizeof(fileName));
            return;
        }

        read(fsDesc,&fileName,sizeof(fileName));
    }
    printf("not able to delete from directory");
    return;
}

//***************************************************************************
// AddFreeBlock: Adds a free block and update the super-block accordingly
//***************************************************************************
void AddFreeBlock(int blockNumber){
    SuperBlock superBlock;
    FreeBlock copyToBlock;

    // read and update existing superblock
    lseek(fsDesc, BLOCK_SIZE, SEEK_SET);
    read(fsDesc,&superBlock,sizeof(superBlock));

    // if free array is full
    // copy content of superblock to new block and point to it
    if (superBlock.nfree == 250){
        copyToBlock.nfree=250;
        CopyFreeArray(superBlock.free, copyToBlock.free, 250);
        lseek(fsDesc,blockNumber*BLOCK_SIZE,SEEK_SET);
        write(fsDesc,&copyToBlock,sizeof(copyToBlock));
        superBlock.nfree = 1;
        superBlock.free[0] = blockNumber;
    }
    else {  // free array is NOT full
        superBlock.free[superBlock.nfree] = blockNumber;
        superBlock.nfree++;
    }

    // write updated superblock to filesystem
    lseek(fsDesc, BLOCK_SIZE, SEEK_SET);
    write(fsDesc, &superBlock,sizeof(superBlock));
}

//***************************************************************************
// GetFreeBlock
//***************************************************************************
int GetFreeBlock(){
    SuperBlock superBlock;
    FreeBlock copy_from_block;
    int free_block;

    lseek(fsDesc, BLOCK_SIZE, SEEK_SET);
    read(fsDesc, &superBlock,sizeof(superBlock));
    superBlock.nfree--;
    free_block = superBlock.free[superBlock.nfree];
    if (free_block ==0){ 											// No more free blocks left
        printf("(\nNo free blocks left");
        lseek(fsDesc, BLOCK_SIZE, SEEK_SET);
        write(fsDesc, &superBlock,sizeof(superBlock));
        fflush(fdopen(fsDesc, 'r+'));
        return -1;
    }

    // Check if need to copy free blocks from linked list
    if (superBlock.nfree == 0) {
        lseek(fsDesc, BLOCK_SIZE*superBlock.free[superBlock.nfree], SEEK_SET);
        read(fsDesc, &copy_from_block,sizeof(copy_from_block));
        superBlock.nfree = copy_from_block.nfree;
        CopyFreeArray(copy_from_block.free, superBlock.free, 250);
        superBlock.nfree--;
        free_block = superBlock.free[superBlock.nfree];
    }

    lseek(fsDesc, BLOCK_SIZE, SEEK_SET);
    write(fsDesc, &superBlock,sizeof(superBlock));
    fflush(fdopen(fsDesc, 'r+'));
    return free_block;
}

//***************************************************************************
// CopyFreeArray
//***************************************************************************
void CopyFreeArray(unsigned short *from_array, unsigned short *to_array, int buf_len){
    int i;
    for (i=0;i<buf_len;i++){
        to_array[i] = from_array[i];
    }
}

//***************************************************************************
// GetBlockSmall
//***************************************************************************
unsigned short GetBlockSmall(int fileInodeNum,int block_number_order){
    INode file_inode;
    file_inode = GetInodeData(fileInodeNum);
    return (file_inode.addr[block_number_order]);
}

//***************************************************************************
// GetBlockLarge
//***************************************************************************
unsigned short GetBlockLarge(int fileInodeNum,int block_number_order){
    INode file_inode;
    unsigned short block_num_tow;
    unsigned short sec_ind_block;
    unsigned short third_ind_block;

    file_inode = GetInodeData(fileInodeNum);
    int logical_block = (block_number_order-22)/512;
    int logical_block2 = logical_block/512;
    int word_in_block = (block_number_order-22) % (512*512);
    if (block_number_order <22){
        return (file_inode.addr[block_number_order]);
    }
    else{
        // Read block number of second indirect block
        lseek(fsDesc, file_inode.addr[22]*BLOCK_SIZE+(logical_block2)*2, SEEK_SET);
        read(fsDesc,&sec_ind_block,sizeof(sec_ind_block));

        // Read third indirect block number from second indirect block
        lseek(fsDesc, sec_ind_block*BLOCK_SIZE+logical_block*2, SEEK_SET);
        read(fsDesc,&third_ind_block,sizeof(block_num_tow));

        // Read target block number from third indirect block
        lseek(fsDesc, third_ind_block*BLOCK_SIZE+word_in_block*2, SEEK_SET);
        read(fsDesc, &block_num_tow,sizeof(block_num_tow));
    }

    return block_num_tow;

}

//***************************************************************************
// AddBlockToInodeSmall
//***************************************************************************
void AddBlockToInodeSmall(int block_order_num, int blockNumber, int inodeNumber){
    //Assume small file
    INode file_inode;
    unsigned short block_num_tow = blockNumber;
    file_inode = GetInodeData(inodeNumber);
    file_inode.addr[block_order_num] = block_num_tow;
    UpdateInode(inodeNumber, file_inode);
    return;
}

//***************************************************************************
// AddBlockToInodeLarge
//***************************************************************************
void AddBlockToInodeLarge(int block_order_num, int blockNumber, int inodeNumber){  //Assume large file
    INode file_inode;
    unsigned short block_num_tow = blockNumber;
    unsigned short sec_in;
    unsigned short third_in;
    unsigned short first_in;
    file_inode = GetInodeData(inodeNumber);


    int logical_block = (block_order_num-22)/512;
    int prev_block = (block_order_num-23)/512;
    int logical_block2 = logical_block/512;
    int prev_block2 = (block_order_num-23)/(512*512);
    int word_in_block2 = (block_order_num-22) % (512*512);
    if (block_order_num <22){
        file_inode.addr[block_order_num] = block_num_tow;
    }
    else{
        if(block_order_num == 22 && word_in_block2 == 0){
            third_in = GetFreeBlock();
            file_inode.addr[block_order_num] = third_in;
        }
        if(prev_block2 < logical_block2){
            sec_in = GetFreeBlock();
            lseek(fsDesc, file_inode.addr[22]*BLOCK_SIZE+(logical_block2)*2, SEEK_SET);
            write(fsDesc, &sec_in,sizeof(sec_in));
            fflush(fdopen(fsDesc, 'r+'));
        }

        if(prev_block < logical_block){
            first_in = GetFreeBlock();

            lseek(fsDesc, file_inode.addr[22]*BLOCK_SIZE+(logical_block2)*2, SEEK_SET);
            read(fsDesc, &sec_in,sizeof(sec_in));
            lseek(fsDesc, sec_in*BLOCK_SIZE+(logical_block)*2, SEEK_SET);
            write(fsDesc, &first_in,sizeof(first_in));
            fflush(fdopen(fsDesc, 'r+'));
        }

        lseek(fsDesc, sec_in*BLOCK_SIZE+(logical_block)*2, SEEK_SET);
        read(fsDesc,&first_in,sizeof(first_in));

        lseek(fsDesc, first_in*BLOCK_SIZE+(word_in_block2)*2, SEEK_SET);
        write(fsDesc, &block_num_tow,sizeof(block_num_tow));

    }

    UpdateInode(inodeNumber, file_inode);

}

//***************************************************************************
// GetFileSize
//***************************************************************************
unsigned int GetFileSize(int inodeNumber){
    INode to_file_inode;
    unsigned int file_size;

    unsigned short bit0_15;
    unsigned char bit16_23;
    unsigned short bit24;

    to_file_inode = GetInodeData(inodeNumber);

    bit24 = LAST(to_file_inode.flags,1);
    bit16_23 = to_file_inode.size0;
    bit0_15 = to_file_inode.size1;

    file_size = (bit24<<24) | ( bit16_23 << 16) | bit0_15;
    return file_size;
}

//***************************************************************************
// GetFileInode
//***************************************************************************
int GetFileInode(const char* filename){
    INode directory_inode;
    FileName fileName;

    directory_inode = GetInodeData(1);

    // Move to the end of directory file
    lseek(fsDesc,(BLOCK_SIZE*directory_inode.addr[0]),SEEK_SET);
    int records=(BLOCK_SIZE-2)/sizeof(fileName);
    int i;
    for(i=0;i<records; i++){
        read(fsDesc, &fileName,sizeof(fileName));
        if (strcmp(filename,fileName.fileName) == 0){
            if (fileName.inodeOffset == 0){
                printf("\nFile %s not found", filename);
                return -1;
            }
            return fileName.inodeOffset;
        }
    }
    printf("\nFile %s not found", filename);
    return -1;
}

//***************************************************************************
// UpdateInode
//***************************************************************************
int UpdateInode(int inode_num, INode inode){
    lseek(fsDesc,(BLOCK_SIZE*2+inode_size*(inode_num-1)),SEEK_SET); //move to the beginning of inode with inode_num
    write(fsDesc, &inode,inode_size);
    return 0;
}

//***************************************************************************
// InitInode
//***************************************************************************
INode InitInode(int inodeNumber, unsigned int file_size){
    INode to_file_inode;
    unsigned short bit0_15;
    unsigned char bit16_23;
    unsigned short bit24;

    bit0_15 = LAST(file_size,16);
    bit16_23 = MID(file_size,16,24);
    bit24 = MID(file_size,24,25);

    to_file_inode.flags=0;       //Initialize
    to_file_inode.flags |=1 <<15; //Set first bit to 1 - i-node is allocated
    to_file_inode.flags |=0 <<14; // Set 2-3 bits to 10 - i-node is plain file
    to_file_inode.flags |=0 <<13;
    if (bit24 == 1){                  // Set most significant bit of file size
        to_file_inode.flags |=1 <<0;
    }
    else{
        to_file_inode.flags |=0 <<0;
    }
    if (file_size<=7*BLOCK_SIZE){
        to_file_inode.flags |=0 <<12; //Set 4th bit to 0 - small file
    }
    else{
        to_file_inode.flags |=1 <<12; //Set 4th bit to 0 - large file
    }
    to_file_inode.nlinks=0;
    to_file_inode.uid=0;
    to_file_inode.gid=0;
    to_file_inode.size0=bit16_23; // Middle 8 bits of file size
    to_file_inode.size1=bit0_15; //Leeast significant 16 bits of file size
    int a = 0;
    for (a = 1; a < 23; a++)
        to_file_inode.addr[a] = 0;

    to_file_inode.actime[0]=0;
    to_file_inode.actime[1]=0;
    to_file_inode.modtime[0]=0;
    to_file_inode.modtime[1]=0;
    return to_file_inode;
}

//***************************************************************************
// GetInodeData
//***************************************************************************
INode GetInodeData(int inodeNumber){
    INode to_file_inode;
    lseek(fsDesc,(BLOCK_SIZE*2+inode_size*(inodeNumber-1)),SEEK_SET); //move to the beginning of inode number inodeNumber
    read(fsDesc,&to_file_inode,inode_size);
    return to_file_inode;
}

