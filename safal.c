// ***********************************************************************
#include<stdio.h>
#include<sys/types.h>
#include<fcntl.h>
#include<unistd.h>
#include<errno.h>
#include<math.h>
#include<string.h>
#include<stdlib.h>
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

// I-NODE FLAGS (FILE TYPE AND OTHER INFO) : NOT USED
//***************************************************************************
typedef struct {
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
    unsigned short inode;
    char filename[14];
} FileName;


// GLOBAL
//***************************************************************************
// CONSTANTS
const int BLOCK_SIZE = 1024;
const int INODE_SIZE = 64;

// SUPER-BLOCK: max size 1024
SuperBlock superBlock;
INode iNode;
FileName fileName, dir;

// INODE FLAGS
const unsigned short allocated = 100000;
const unsigned short plainfile = 000000;
const unsigned short largefile = 010000;
const unsigned short directory = 040000;
const unsigned short permissions = 000777;

//global variables
int fd;        //file descriptor
unsigned int chainarray[256];        //array used while chaining data blocks.

// Primary operations
int InitializeFS(int fileDesc, long nBlocks, int nInodes);
void CopyIN(const char * extFile, const char * fsFile);
void CopyOUT(const char * fsFile, const char * extFile);
void MakeDirectory(char* dirName, unsigned int iNode);

// Directory
void CreateRootDirectory();
int WriteDirectory(INode rootinode, FileName FileName);

// Allocation
void AddFreeBlock(int fileDesc, long blockNumber); // initFS
void AllocateFreeBlock(unsigned int block);
void LinkDataBlocks(unsigned short total_blcks);
unsigned int AllocateDataBlock();
unsigned short AllocateInode();
int MakeIndirectBlock(int fd, int block_num);

// Read-Write
void ReadCharBlock(char *target, unsigned int blocknum);
void ReadIntBlock(int *target, unsigned int blocknum);
void WriteIntBlock(unsigned int *target, unsigned int blocknum);
void WriteCharBlock(char *target, unsigned int blocknum);
void WriteInode(INode inodeinstance, unsigned int inodenumber);


//***************************************************************************
// MAIN: driver
//***************************************************************************
int main() {
    printf("\n## WELCOME TO UNIX V6 FILE SYSTEM ##\n");
    printf("\n## PLEASE ENTER ONE OF THE FOLLOWING COMMAND ##\n");
    printf("\n\t ~initfs - To initialize the file system \n");
    printf("\t\t ~Example :: initfs /home/venky/behappy.txt 5000 300 \n");
    printf("\n\t ~q - To quit the program\n");

    short fsStatus; // status of file system creation
    short cmdStatus; // status of command execution
    int fileDesc = 0; // filesystem descriptor
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

            printf("FileSystem creation initiated: %s\n", pFilePath);

            if (nBlocks > 4194304) {// max allowed file size: 4GB
                printf("\nFAILED! Max allowed number of blocks: 4194304\n");
                exit(0);
            }

            // check if the file exists
            if (access(pFilePath, F_OK) != -1) { // if exists
                fileDesc = open(pFilePath, O_RDWR);
                if (fileDesc == -1) {
                    printf("\nERROR: Not sufficient file descriptors.\n");
                    exit(0);
                } else
                    printf("\nFile opens %s \n", pFilePath);
            } else { // file doesn't exist
                fileDesc = open(pFilePath, O_RDWR | O_CREAT);
                if (fileDesc == -1) {
                    printf("\nERROR: Not sufficient file descriptors.\n");
                    exit(0);
                } else
                    printf("\nCreating new file system for the file %s .\n", pFilePath);
            }

            // if less than 4GB: INITIALIZE file system
            fsStatus = InitializeFS(fileDesc, nBlocks, nInodes);
            if (fsStatus == 0)
                printf("\nFile system initialization SUCCESSFUL!\n");
            else
                printf("\nFile system initialization FAILED!\n");
        }
            // if pCommand is cpin initilize file system
        else if (strcmp(pCommand, "cpin") == 0) {
            if(fsStatus != 0)
                printf("Error! File System NOT found. Initializing file system (initfs)\n");
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
            if(fsStatus != 0)
                printf("Error! File System NOT found. Initializing file system (initfs)\n");
            else {
                char *pFSFile = strtok(NULL, " ");
                char *pExtFile = strtok(NULL, " ");

                CopyOUT(pFSFile, pExtFile);
                printf("\nSUCCESS! File exported\n");
            }
        }
        else if (strcmp(pCommand, "mkdir") == 0) {
            if(fsStatus != 0)
                printf("Error! File System NOT found. Initializing file system (initfs)\n");
            else {
                char *pDir = strtok(NULL, " ");
                unsigned short iNum = AllocateInode();
                MakeDirectory(pDir, iNum);
                printf("\nSUCCESS! Make directory\n");
            }
        }
        else
            printf("FAILED: Invalid Command. Accepted commands: InitFS and q.\n");
    }
}

//***************************************************************************
// InitializeFS: INITIALIZES THE FILE SYSTEM
//***************************************************************************
/*
* @fileDesc: file descriptor
* @nBlocks: total blocks of file system
* @nInodes: number of inodes
*/
int InitializeFS(int fileDesc, long nBlocks, int nInodes) {
    char buffer[BLOCK_SIZE]; // char buffer to read/write the blocks to/from file
    long writtenBytes = 0, i = 0;

    // BOOT LOADER (FIRST) BLOCK
    memset(buffer, 0, BLOCK_SIZE);

    // Calculate i-node blocks requirement | Note: 1024 block size = 16 i-nodes * 64 bytes
    int nInodeBlocks = 0;
    if (nInodes % 64 == 0) // perfect fit to blocks
        nInodeBlocks = nInodes / 64;
    else
        nInodeBlocks = nInodes / 64 + 1;

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

    // fill blocks with 0s to confirm size
    char charbuffer[BLOCK_SIZE];
    memset(charbuffer, 0, BLOCK_SIZE);
    for (nextFreeBlock = firstFreeBlock; nextFreeBlock < nBlocks; nextFreeBlock++) {
        lseek(fileDesc, nextFreeBlock * BLOCK_SIZE, SEEK_SET);
        write(fileDesc, charbuffer, BLOCK_SIZE);
    }

    // Initialize free blocks
    for (nextFreeBlock = firstFreeBlock; nextFreeBlock < nBlocks; nextFreeBlock++) {
        //printf("\nCall AddFreeBlock for block: %i",nextFreeBlock); // TO DEBUG
        AddFreeBlock(fileDesc, nextFreeBlock);
    }

    // ROOT DIRECTORY INODE
    //***************************************************************************
    INodeFlags flags; // flags to manage file type

    // Need to Initialize only first i-node and root directory file will point to it
    iNode.flags = 0; //Initialize
    iNode.flags |= 1 << 15; //Set first bit to 1 - i-node is allocated
    iNode.flags |= 1 << 14; // Set 2-3 bits to 10 - i-node is directory
    iNode.nlinks = 2;
    iNode.uid = 0;
    iNode.gid = 0;
    iNode.size0 = 0;
    iNode.size1 = 16 * 2; //File size is two records each is 16 bytes.
    iNode.addr[0] = firstFreeBlock - 1;
    for (i = 1; i < 8; i++)
        iNode.addr[i] = 0;
    for (i = 1; i < 2; i++)
        iNode.actime[i] = (short) time(0);
    for (i = 1; i < 2; i++)
        iNode.modtime[i] = (short) time(0);

    // Create root directory file and initialize with "." and ".." Set offset to 1st i-node
    fileName.inode = 1;
    strcpy(fileName.filename, ".");
    int allocBlock = firstFreeBlock - 1;      // Allocate block for file_directory
    lseek(fileDesc, allocBlock * BLOCK_SIZE, SEEK_SET); //move to the beginning of first available block
    write(fileDesc, &fileName, sizeof(fileName));
    strcpy(fileName.filename, "..");
    write(fileDesc, &fileName, sizeof(fileName));

    //Point first inode to file directory
    printf("\nDirectory in the block %i", allocBlock);
    iNode.addr[0] = allocBlock;
    lseek(fileDesc, BLOCK_SIZE * 2, SEEK_SET); //move to the beginning of inode with INodeNumber
    write(fileDesc, &iNode, sizeof(iNode));

    return 0;
}


//***************************************************************************
// CopyIN: imports external file to file-system
//***************************************************************************
/*
*command : CopyIN <source_file_path_in_foreign_operating_system> <destination_filename_in_v6>
*performs of copy of contents from source file in foreign operating system(external file) to
*existing or newly created destination file given in the command.
*/
void CopyIN(const char *src, const char *targ) {
    int indirect = 0;
    int indirectfn_return = 1;
    char reader[BLOCK_SIZE];
    int bytes_read;
    int srcfd;
    int extfilesize = 0;
    //open external file
    if ((srcfd = open(src, O_RDONLY)) == -1) {
        printf("\nerror opening file: %s \n", src);
        return;
    }
    unsigned int inumber = AllocateInode();
    if (inumber < 0) {
        printf("Error : ran out of inodes \n");
        return;
    }
    unsigned int newblocknum;

    //preapare new file in V6 file system
    fileName.inode = inumber;
    memcpy(fileName.filename, targ, strlen(targ));
    //write inode for the new file
    iNode.flags = allocated | plainfile | permissions;
    iNode.nlinks = 1;
    iNode.uid = '0';
    iNode.gid = '0';
    iNode.size0 = '0';

    int i = 0;

    //start reading external file and perform file size calculation simultaneously
    while (1) {
        if ((bytes_read = read(srcfd, reader, BLOCK_SIZE)) != 0) {
            newblocknum = AllocateDataBlock();
            WriteCharBlock(reader, newblocknum);
            iNode.addr[i] = newblocknum;
            // When bytes returned by read() system call falls below the block size of
            //1024, reading and writing are complete. Print file size in bytes and exit
            if (bytes_read < BLOCK_SIZE) {
                extfilesize = i * BLOCK_SIZE + bytes_read;
                printf("Small file copied\n");
                iNode.size1 = extfilesize;
                printf("File size = %d bytes\n", extfilesize);
                break;
            }
            i++;

            //if the counter i exceeds 21(maximum number of elements in addr[] array,
            //transfer control to new function that creates indirect blocks which
            //handles large files(file size > 56 KB).
            if (i > 22) {

                indirectfn_return = MakeIndirectBlock(srcfd, iNode.addr[0]);
                indirect = 1;
                break;
            }
        }
            // When bytes returned by read() system call is 0,
            // reading and writing are complete. Print file size in bytes and exit
        else {
            extfilesize = i * BLOCK_SIZE;
            printf("Small file copied\n");
            iNode.size1 = extfilesize;
            printf("File size = %d bytes\n", extfilesize);
            break;
        }

    }
    iNode.actime[0] = 0;
    iNode.modtime[0] = 0;
    iNode.modtime[1] = 0;

//if call is made to function that creates indirect blocks,
//it is a large file. Set flags for large file to 1.
    if (indirect == 1) {
        iNode.flags = iNode.flags | largefile;
    }
//write to inode and directory data block
    if (indirectfn_return > -1) {
        WriteInode(iNode, inumber);
        lseek(fd, 2 * BLOCK_SIZE, 0);
        read(fd, &iNode, INODE_SIZE);
        iNode.nlinks++;
        WriteDirectory(iNode, fileName);
    }
    if (indirectfn_return == -1) {
        printf("\nExitting as file is large..");
    }
}


//***************************************************************************
// CopyOUT: exports file-system to external file
//***************************************************************************
/*
//command : CopyOUT <source_file_name_in_v6> <destination_filename_in_foreign_operating_system>
//performs of copy of contents from existing source file in V6 to
//existing or newly created destination file in foreign operating system(external file)
//given in the command.
*/
void CopyOUT(const char *src, const char *targ) {
    int indirect = 0;
    int found_dir = 0;
    int src_inumber;
    char reader[BLOCK_SIZE];                        //reader array to read characters (contents of blocks or file contents)
    int reader1[BLOCK_SIZE];                        //reader array to read integers (block numbers contained in add[] array)
    int bytes_read;
    int targfd;
    int i = 0;
    int j = 0;
    int addrcount = 0;
    int total_blocks = 0;
    int remaining_bytes = 0;
    int indirect_block_chunks = 0;                        //each chunk of indirect blocks contain 256 elements that point to data blocks
    int remaining_indirectblks = 0;
    int indirectblk_counter = 0;
    int bytes_written = 0;

    //open or create external file(target file) for read and write
    if ((targfd = open(targ, O_RDWR | O_CREAT, 0600)) == -1) {
        printf("\nerror opening file: %s\n", targ);
        return;
    }
    lseek(fd, 2 * BLOCK_SIZE, 0);
    read(fd, &iNode, INODE_SIZE);

    //find the source V6 file in the root directory
    for (addrcount = 0; addrcount <= 21; addrcount++) {
        if (found_dir != 1) {
            lseek(fd, (iNode.addr[addrcount] * BLOCK_SIZE), 0);
            for (i = 0; i < 64; i++) {
                if (found_dir != 1) {
                    read(fd, &dir, 16);

                    if (strcmp(dir.filename, src) == 0) {

                        src_inumber = dir.inode;
                        found_dir = 1;
                    }
                }

            }
        }
    }

    if (src_inumber == 0) {
        printf("File not found in the file system. Unable to proceed\n");
        return;
    }
    lseek(fd, (2 * BLOCK_SIZE + INODE_SIZE * src_inumber), 0);
    read(fd, &iNode, 64);

    //check if file is directory. If so display information and return.
    if (iNode.flags & directory) {
        printf("The given file name is a directory. A file is required. Please retry.\n");
        return;
    }

    //check if file is a plainfile. If so display information and return.
    if ((iNode.flags & plainfile)) {
        printf("The file name is not a plain file. A plain file is required. Please retry.\n");
        return;
    }
    //check if file is a large file
    if (iNode.flags & largefile) {
        indirect = 1;
    }

    total_blocks = (int) ceil(iNode.size1 / 1024.0);
    remaining_bytes = iNode.size1 % BLOCK_SIZE;


    //read and write small file to external file
    if (indirect == 0)                //check if it is a small file. indirect = 0 implies the function that makes indirect blocks was not called during CopyIN.
    {
        printf("file size = %d \n", iNode.size1);

        for (i = 0; i < total_blocks; i++) {
            ReadCharBlock(reader, iNode.addr[i]);
            //if counter reaches end of the blocks, write remaining bytes(bytes < 1024) and return.
            if (i == (total_blocks - 1)) {
                write(targfd, reader, remaining_bytes);
                printf("Contents were transferred to external file\n");
                return;
            }
            write(targfd, reader, BLOCK_SIZE);
        }
    }


    //read and write large file to external file
    if (indirect == 1)            //check if it is a large file. indirect = 1 implies the function that makes indirect blocks was called during CopyIN.
    {
        total_blocks = iNode.size1 / 1024;
        indirect_block_chunks = (int) ceil(
                total_blocks / 256.0);    //each chunk of indirect blocks contain 256 elements that point to data blocks
        remaining_indirectblks = total_blocks % 256;
        printf("file size = %d \n", iNode.size1);

        //Loop for chunks of indirect blocks
        for (i = 0; i < indirect_block_chunks; i++) {
            ReadIntBlock(reader1,
                         iNode.addr[i]);                //store block numbers contained in addr[] array in integer reader array )

            //if counter reaches last chunk of indirect blocks, program loops the remaining and exits after writing the remaining bytes
            if (i == (indirect_block_chunks - 1))
                total_blocks = remaining_indirectblks;
            for (j = 0; j < 256 && j < total_blocks; j++) {

                ReadCharBlock(reader,
                              reader1[j]);            //store block contents pointed by addr[] array in character  reader array )
                if ((bytes_written = write(targfd, reader, BLOCK_SIZE)) == -1) {
                    printf("\n Error in writing to external file\n");
                    return;
                }
                if (j == (total_blocks - 1)) {
                    write(targfd, reader, remaining_bytes);
                    printf("Contents were transferred to external file\n");
                    return;
                }
            }
        }
    }
}

//***************************************************************************
// MakeDirectory: create a new directory with the given name under root
//***************************************************************************
void MakeDirectory(char *filename, unsigned int newinode) {
    int blocks_read;
    unsigned int parentinum = 1;                //since parent is always root directory for this project, inumber is 1.
    char buffertemp[BLOCK_SIZE];
    int i = 0;
    unsigned int block_num = AllocateDataBlock();
    strncpy(fileName.filename, filename, 14);        //string copy filename contents to directory structure's field
    fileName.inode = newinode;
    lseek(fd, 2 * BLOCK_SIZE, 0);


    iNode.nlinks++;
    // set up this directory's inode
    iNode.flags = allocated | directory | permissions;
    iNode.nlinks = 2;
    iNode.uid = '0';
    iNode.gid = '0';
    iNode.size0 = '0';
    iNode.size1 = 64;
    for (i = 1; i < 22; i++)
        iNode.addr[i] = 0;
    iNode.addr[0] = block_num;
    iNode.actime[0] = 0;
    iNode.modtime[0] = 0;
    iNode.modtime[1] = 0;
    WriteInode(iNode, newinode);
    lseek(fd, 2 * BLOCK_SIZE, 0);
    blocks_read = read(fd, &iNode, 64);

    iNode.nlinks++;

    if (WriteDirectory(iNode, fileName))
        return;
    for (i = 0; i < BLOCK_SIZE; i++)
        buffertemp[i] = 0;
    // copying to inode numbers and filenames to directory's data block for ".".
    memcpy(buffertemp, &newinode,
           sizeof(newinode));        //memcpy(used for fixed width character array inbuilt function copies n bytes from memory area newinode to memory  area buffertemp
    buffertemp[2] = '.';
    buffertemp[3] = '\0';
    // copying to inode numbers and filenames to directory's data block for ".."
    memcpy(buffertemp + 16, &parentinum,
           sizeof(parentinum));        //memcpy(used for fixed width character array inbuilt function copies n bytes from memory area newinode to memory  area buffertemp
    buffertemp[18] = '.';
    buffertemp[19] = '.';
    buffertemp[20] = '\0';
    WriteCharBlock(buffertemp, block_num);            //writing character array to newly allocated block

    printf("\n Directory created \n");
}


//***************************************************************************
//function to create root directory and its corresponding inode.
//***************************************************************************
void CreateRootDirectory() {
    unsigned int i = 0;
    unsigned short bytes_written;
    unsigned int datablock = AllocateDataBlock();

    for (i = 0; i < 14; i++)
        fileName.filename[i] = 0;

    fileName.filename[0] = '.';            //root directory's file name is .
    fileName.filename[1] = '\0';
    fileName.inode = 1;                    // root directory's inode number is 1.

    iNode.flags = allocated | directory | 000077;        // flag for root directory
    iNode.nlinks = 2;
    iNode.uid = '0';
    iNode.gid = '0';
    iNode.size0 = '0';
    iNode.size1 = INODE_SIZE;
    iNode.addr[0] = datablock;

    for (i = 1; i < 22; i++)
        iNode.addr[i] = 0;

    iNode.actime[0] = 0;
    iNode.modtime[0] = 0;
    iNode.modtime[1] = 0;

    WriteInode(iNode, 0);

    lseek(fd, datablock * BLOCK_SIZE, 0);

    //filling 1st entry with .
    if ((bytes_written = write(fd, &fileName, 16)) < 16)
        printf("\n Error in writing root directory \n ");

    fileName.filename[1] = '.';
    fileName.filename[2] = '\0';
    // filling with .. in next entry(16 bytes) in data block.

    if ((bytes_written = write(fd, &fileName, 16)) < 16)
        printf("\n Error in writing root directory ");

}


//***************************************************************************
// WriteDirectory: function to write to directory's data block
//***************************************************************************
/*
gets inode(always root directory's inode from mkdir) and directory (struct's) reference as inputs.
*/
int WriteDirectory(INode rootinode, FileName fileName) {

    int duplicate = 0;        //to find duplicate named directories.
    unsigned short addrcount = 0;
    char dirbuf[BLOCK_SIZE];        //array to
    int i = 0;
    for (addrcount = 0; addrcount <= 21; addrcount++) {
        lseek(fd, rootinode.addr[addrcount] * BLOCK_SIZE, 0);
        for (i = 0; i < 64; i++) {
            read(fd, &dir, 16);
            if (strcmp(dir.filename, fileName.filename) == 0)            //check for duplicate named directories
            {
                printf("Cannot create directory.The directory name already exists.\n");
                duplicate = 1;
                break;
            }
        }
    }
    if (duplicate != 1) {
        for (addrcount = 0; addrcount <=
                            21; addrcount++)            //for each of the address elements ( addr[0],addr[1] till addr[21]), check which inode is not allocated
        {
            ReadCharBlock(dirbuf, rootinode.addr[addrcount]);
            for (i = 0; i <
                        64; i++)                                        //Looping for each directory entry (1024/16 = 64 entries in total, where 1024 is block size and 16 bytes is directory entry size)
            {

                if (dirbuf[16 * i] == 0) // if inode is not allocated
                {
                    memcpy(dirbuf + 16 * i, &fileName.inode, sizeof(fileName.inode));
                    memcpy(dirbuf + 16 * i + sizeof(fileName.inode), &fileName.filename,
                           sizeof(fileName.filename));        //using memcpy function to copy contents of filename and inode number, to store it in directory entry.
                    WriteCharBlock(dirbuf, rootinode.addr[addrcount]);
                    return duplicate;
                }
            }
        }
    }
    return duplicate;
}


//***************************************************************************
// MakeIndirectBlock: function that creates indirect blocks. handles large file (file size > 56 KB)
//***************************************************************************
/*largest file size handled : ( 22 * 256 * 1024 ) /1024 = 28672 KB.*/
int MakeIndirectBlock(int fd, int block_num) {
    char reader[BLOCK_SIZE];
    unsigned int indirectblocknum[256];                //integer array to store indirect blocknumbers
    int i = 0;
    int j = 0;
    int bytes_read;
    int blocks_written = 0;
    int extfilesize = 22 * BLOCK_SIZE;            //filesize is initialized to small file size since data would have been read upto this size.
    for (i = 0; i < 22; i++)
        indirectblocknum[i] = iNode.addr[i];        //transfer existing block numbers in addr[] array to new temporary array

    iNode.addr[0] = AllocateDataBlock();        //allocate a data block which will be used to store the temporary integer array of indirect block numbers

    for (i = 1; i < 22; i++)
        iNode.addr[i] = 0;

    i = 22;
    while (1) {
        if ((bytes_read = read(fd, reader, BLOCK_SIZE)) != 0) {
            indirectblocknum[i] = AllocateDataBlock();  //allocate a data block which will be used to store the temporary integer array of indirect block numbers
            WriteCharBlock(reader, indirectblocknum[i]);
            i++;

            // When bytes returned by read() system call falls below the block size of
            //1024, reading and writing are complete. Print file size in bytes and exit
            if (bytes_read < BLOCK_SIZE) {
                WriteIntBlock(indirectblocknum, iNode.addr[j]);
                printf("Large File copied\n");
                extfilesize = extfilesize + blocks_written * BLOCK_SIZE + bytes_read;
                iNode.size1 = extfilesize;
                printf("File size = %d bytes\n", extfilesize);
                break;
            }
            blocks_written++;
            //When counter i reaches 256, first indirect block is full. So reset counters to 0
            //allocate new block to store it in addr[] array that will be the new indirect block.

            if (i > 255 && j <= 21) {
                WriteIntBlock(indirectblocknum, iNode.addr[j]);
                iNode.addr[++j] = AllocateDataBlock();
                i = 0;
                extfilesize = extfilesize + 256 * BLOCK_SIZE;
                blocks_written = 0;
            }
            //if all the elements in addr[] array have been exhausted with indirect blocks, maximum capacity of
            //28672 KB has been reached. Throw an error that the file is too large for this file system.
            if (j > 21) {
                printf("This file copy is not supported by the file system as the file is very large\n");
                return -1;
                break;
            }
        }
            // When bytes returned by read() system call is 0,
            // reading and writing are complete. Print file size in bytes and exit
        else {
            WriteIntBlock(indirectblocknum, iNode.addr[j]);
            iNode.size1 = extfilesize;
            printf("Large File copied\n");
            printf("File size = %d bytes\n", extfilesize);
            break;
        }

    }
    return 0;
}

//***************************************************************************
// Data blocks chaining procedure
//***************************************************************************
void LinkDataBlocks(unsigned short total_blcks) {
    unsigned int emptybuffer[256];   // buffer to fill with zeros to entire blocks. Since integer size is 4 bytes, 256 * 4 = 1024 bytes.
    unsigned int blockcounter;
    unsigned int no_chunks = total_blcks / 250;            //splitting into blocks of 250
    unsigned int remainingblocks = total_blcks % 250;        //getting remaining/left over blocks
    unsigned int i = 0;

    for (i = 0; i < 256; i++)
        emptybuffer[i] = 0;                //setting character array to 0 to remove any bad/junk data
    for (i = 0; i < 256; i++)
        chainarray[i] = 0;                //setting integer array to 0 to remove any bad/junk data

//chaining for chunks of blocks 250 blocks at a time
    for (blockcounter = 0; blockcounter < no_chunks; blockcounter++) {
        chainarray[0] = 250;

        for (i = 0; i < 250; i++) {
            if (blockcounter == (no_chunks - 1) && remainingblocks == 0 && i == 0) {
                chainarray[i + 1] = 0;
                continue;
            }
            chainarray[i + 1] = 2 + superBlock.isize + i + 250 * (blockcounter + 1);
        }
        WriteIntBlock(chainarray, 2 + superBlock.isize + 250 * blockcounter);

        for (i = 1; i <= 250; i++)
            WriteIntBlock(emptybuffer, 2 + superBlock.isize + i + 250 * blockcounter);
    }

//chaining for remaining blocks
    chainarray[0] = remainingblocks;
    chainarray[1] = 0;
    for (i = 1; i <= remainingblocks; i++)
        chainarray[i + 1] = 2 + superBlock.isize + i + (250 * blockcounter);

    WriteIntBlock(chainarray, 2 + superBlock.isize + (250 * blockcounter));

    for (i = 1; i <= remainingblocks; i++)
        WriteIntBlock(chainarray, 2 + superBlock.isize + 1 + i + (250 * blockcounter));


    for (i = 0; i < 256; i++)
        chainarray[i] = 0;
}

//***************************************************************************
// AllocateDataBlock: function to get a free data block. Also decrements nfree for each pass
//***************************************************************************
unsigned int AllocateDataBlock() {
    unsigned int block;

    superBlock.nfree--;

    block = superBlock.free[superBlock.nfree];
    superBlock.free[superBlock.nfree] = 0;

    if (superBlock.nfree == 0) {
        int n = 0;
        ReadIntBlock(chainarray, block);
        superBlock.nfree = chainarray[0];
        for (n = 0; n < 250; n++)
            superBlock.free[n] = chainarray[n + 1];
    }
    return block;
}

//***************************************************************************
// AllocateInode: allocate free inode
//***************************************************************************
unsigned short AllocateInode() {
    unsigned short inumber;
    unsigned int i = 0;
    superBlock.ninode--;
    inumber = superBlock.inode[superBlock.ninode];
    return inumber;
}

//***************************************************************************
//AllocateFreeBlock :free data blocks and initialize free array
//***************************************************************************
void AllocateFreeBlock(unsigned int block) {
    superBlock.free[superBlock.nfree] = block;
    ++superBlock.nfree;
}

//***************************************************************************
// AddFreeBlock: Adds a free block and update the super-block accordingly
//***************************************************************************
void AddFreeBlock(int fileDesc, long blockNumber) {
    FreeBlock copyToBlock;
    // if free array is full
    // copy content of superBlock to new block and point to it
    if (superBlock.nfree == 250) {
        //printf("\nCopy free list to block %i",blockNumber);
        copyToBlock.nfree = 250;
        int i = 0;
        for (i = 0; i < 250; i++)
            copyToBlock.free[i] = superBlock.free[i];
        lseek(fileDesc, blockNumber * BLOCK_SIZE, SEEK_SET);
        write(fileDesc, &copyToBlock, sizeof(copyToBlock));
        superBlock.nfree = 1;
        superBlock.free[0] = blockNumber;
    } else { // free array is NOT full
        superBlock.free[superBlock.nfree] = blockNumber;
        superBlock.nfree++;
    }

    // write updated superblock to filesystem
    lseek(fileDesc, BLOCK_SIZE, SEEK_SET);
    write(fileDesc, &superBlock, sizeof(superBlock));
}

//***************************************************************************
// ReadCharBlock: function to read character array from the required block
//***************************************************************************
void ReadCharBlock(char *target, unsigned int blocknum) {

    if (blocknum > superBlock.isize + superBlock.fsize + 2)

        printf(" Block number is greater than max file system block size for reading\n");
    else{
        lseek(fd, blocknum * BLOCK_SIZE, 0);
        read(fd, target, BLOCK_SIZE);
    }
}

//***************************************************************************
// ReadIntBlock: function to read integer array from the required block
//***************************************************************************
void ReadIntBlock(int *target, unsigned int blocknum) {
    if (blocknum > superBlock.isize + superBlock.fsize + 2)

        printf(" Block number is greater than max file system block size for reading\n");
    else{
        lseek(fd, blocknum * BLOCK_SIZE, 0);
        read(fd, target, BLOCK_SIZE);
    }
}

//***************************************************************************
//function to write integer array to the required block
//***************************************************************************
void WriteIntBlock(unsigned int *target, unsigned int blocknum) {
    int bytes_written;
    if (blocknum > superBlock.isize + superBlock.fsize + 2)
        printf(" Block number is greater than max file system block size for writing\n");
    else {
        lseek(fd, blocknum * BLOCK_SIZE, 0);
        if ((bytes_written = write(fd, target, BLOCK_SIZE)) < BLOCK_SIZE)
            printf("\n Error in writing block number : %d", blocknum);
    }
}

//***************************************************************************
//function to write character array to the required block
//***************************************************************************
void WriteCharBlock(char *target, unsigned int blocknum) {
    int bytes_written;
    if (blocknum > superBlock.isize + superBlock.fsize + 2)
        printf(" Block number is greater than max file system block size for writing\n");
    else {
        lseek(fd, blocknum * BLOCK_SIZE, 0);
        if ((bytes_written = write(fd, target, BLOCK_SIZE)) < BLOCK_SIZE)
            printf("\n Error in writing block number : %d", blocknum);
    }
}

//***************************************************************************
//function to write to an inode given the inode number
//***************************************************************************
void WriteInode(INode inodeinstance, unsigned int inodenumber) {
    int bytes_written;
    lseek(fd, 2 * BLOCK_SIZE + inodenumber * INODE_SIZE, 0);
    if ((bytes_written = write(fd, &inodeinstance, INODE_SIZE)) < INODE_SIZE)
        printf("\n Error in writing inode number : %d", inodenumber);

}
