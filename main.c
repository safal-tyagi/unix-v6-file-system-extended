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

#define LAST(k,n) ((k) & ((1<<(n))-1))
#define MID(k,m,n) LAST((k)>>(m),((n)-(m)))

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
    unsigned short size0;
    unsigned short size1;
    unsigned short addr[23];
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

typedef struct one_bit
{
    unsigned x:1;
} one_bit;

typedef struct seven_bits
{
    unsigned x:7;
} seven_bits;

typedef union
{
    struct {
    	seven_bits    bit25_31;
        one_bit       bit24;
        unsigned char bit16_23;
        unsigned short bit0_15;
    } bits;
    unsigned int dword;
} File_Size;

int InitializeFS(int nBlocks, int nInodes,FILE* fileDesc);
int CopyIN(const char* extFile,const char* fsFile,FILE* fileDesc);
int CopyOUT(const char* fsFile,const char* extFile,FILE* fileDesc);
int Rm(const char* filename,FILE* fileDesc);
int MakeDirectory(const char* dirName,FILE* fileDesc);
int add_file_to_dir(const char* fsFile,FILE* fileDesc);
int get_inode_by_file_name(const char* fsFile,FILE* fileDesc);
INode read_inode(int to_file_inode_num, FILE* fileDesc);
INode init_file_inode(int to_file_inode_num, unsigned int file_size, FILE* fileDesc);
int write_inode(int inode_num, INode inode, FILE* fileDesc);
void AddFreeBlock(int blockNumber, FILE* fileDesc);
int get_free_block(FILE* fileDesc);
void copy_int_array(unsigned short *from_array, unsigned short *to_array, int buf_len);
void add_block_to_inode(int block_order_num, int blockNumber, int to_file_inode_num, FILE* fileDesc);
unsigned short get_block_for_big_file(int file_node_num,int block_number_order,FILE* fileDesc);
unsigned int get_inode_file_size(int to_file_inode_num, FILE* fileDesc);
void add_block_to_free_list(int next_block_number, FILE* fileDesc);
void remove_file_from_directory(int file_node_num, FILE*  fileDesc);
void add_block_to_inode_small_file(int block_order_num, int blockNumber, int to_file_inode_num, FILE* fileDesc);
unsigned short get_block_for_small_file(int file_node_num,int block_number_order, FILE* fileDesc);

int BLOCK_SIZE=1024;
int inode_size=64;
int main(int argc, char *argv[])
{
	int status,file_size;
	FILE* fileDesc = NULL;
	const char *filename = argv[1];
	int max = 200;
	char* command = (char*)malloc(max); /* allocate buffer for user command */
	int cmd_counter = 0;

	if (argc !=2){
		printf("\nWrong number of parameters. Only one parameter is allowed: file_system name \n");
		exit(0);
	  }

	//printf("\nBefore open file.File name: {%s}\n", filename);

	if( access( filename, F_OK ) != -1 ) {
		// file exists
		printf("\nFile system %s exists. Trying to open...\n",filename);
		fileDesc = fopen(filename, "r+");
		fseek(fileDesc, 0L, SEEK_END); // Move to the end of the file
		file_size = ftell(fileDesc);
		if (file_size == 0){
			printf("\nFile system %s doesn't exists. You need to run initfs command\n",filename);
			}
		else{
			printf("\nFile size is %i\n",file_size);
			rewind(fileDesc);
			}
		}
	else {
		// file doesn't exist
		fileDesc = fopen(filename, "w+");
		printf("\nFile system %s doesn't exists. You need to run initfs command\n",filename);
		}


	if(NULL == fileDesc){
	printf("\n fopen() Error!!!\n");
	return 1;
	}

	while(1){
			//While loop, wait for user input
		printf("MyFileSystem>>");
		cmd_counter++;

		int i = 0;
		while (1) {
			// Read user input and process it:
			//  - place zero in last byte
			//
			int c = getchar();
			if (c=='\n'){ /* at end, add terminating zero */
				command[i] = 0;
				break;
			}
			command[i] = c;
			if (i == max - 1) { /* buffer full */
				printf("Command is too long\n");
				exit(1);
			}
			i++;
		}
		char * cmd;
		char * arg;
		// In case user typed command with argument, split to command and argument
		cmd = strtok (command," ,\n");
		if (cmd != NULL){
			arg = strtok (NULL, "\n");
			}

		if (strcmp(command,"q")==0){
			printf("Number of executed commands is..%i\n",cmd_counter);
			fclose(fileDesc);
			exit(0);
		}
		if (strcmp(command,"initfs")==0){
			printf("Init file_system was requested: %s\n",filename);
			char *  p    = strtok (arg, " ");
			long int nBlocks = atoi(p);
			p = strtok (NULL, " ");
			int nInodes = atoi(p);
			if (nBlocks>4194304){
				printf("\nFailed. Too many blocks requested, maximum is 4194304. Please try again.....\n");
				}
			else{
				status = InitializeFS(nBlocks,nInodes,fileDesc);
				if (status ==0)
					printf("\nFile system successfully initialized\n");
				else
					printf("\nFile system initialization failed\n");
				}
		}

		else if (strcmp(command,"cpin")==0){
			printf("cpin was requested\n");
			char *  p    = strtok (arg, " ");
			const char *extFile = p;
			p = strtok (NULL, " ");
			const char *fsFile = p;
			if (strlen(fsFile)>14){
				printf("Target file name  %s is too long:%i, maximum length is 14",fsFile,strlen(fsFile));
				status=1;
				}
			else
				status = CopyIN(extFile,fsFile,fileDesc);
			if (status ==0)
				printf("\nFile  successfully copied\n");
			else
				printf("\nFile copy failed\n");

		}

		else if (strcmp(command,"cpout")==0){
			char *  p    = strtok (arg, " ");
			const char *fsFile = p;
			p = strtok (NULL, " ");
			const char *extFile = p;
			status = CopyOUT(fsFile,extFile,fileDesc);
			if (status ==0)
				printf("\nFile %s successfully copied\n",fsFile);
			else
				printf("\nFile copy failed\n");
		}

		else if (strcmp(command,"Rm")==0){
			printf("Rm was requested\n");
			char *  p    = strtok (arg, " ");
			const char *filename = p;
			status = Rm(filename,fileDesc);
			if (status ==0)
				printf("\nFile  successfully removed\n");
			else
				printf("\nFile removal failed\n");
		}

		else if (strcmp(command,"mkdir")==0){
			printf("mkdir was requested\n");
			char *  p    = strtok (arg, " ");
			const char *dirName = p;
			status = MakeDirectory(dirName,fileDesc);
			if (status ==0)
				printf("\nDirectory %s successfully created\n",dirName);
			else
				printf("\nDirectory creation failed\n");
		}

		else
			printf("Not valid command. Available commands: initfs, cpin, cpout, Rm, q, mkdir. Please try again\n");

	}
}

int InitializeFS(int nBlocks, int nInodes, FILE* fileDesc)
{

	long i = 0;

    // SUPER-BLOCK: max size 1024
    SuperBlock superBlock;

	rewind(fileDesc);
	// TEST ONLY: fill blocks with 0s to confirm size
    char charbuffer[BLOCK_SIZE];  // char buffer to read/write the blocks to/from file
    memset(charbuffer, 0, BLOCK_SIZE);
    for (i = 0; i < nBlocks; i++ ) {
        lseek(fileDesc, i * BLOCK_SIZE, SEEK_SET);
        write(fileDesc, charbuffer, BLOCK_SIZE);
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
    lseek(fileDesc, BLOCK_SIZE, SEEK_SET);
    write(fileDesc, &superBlock, sizeof(superBlock)); // Note: sizeof(superBlock) <= BLOCK_SIZE

    // SUPER-BLOCK: FREE LIST
    // first free block = bootBlock + superBlock + inodeBlocks
    int firstFreeBlock = 2 + number_inode_blocks + 1; // +1 for the root directory
	int next_free_block;

	// Initialize free blocks
	for (next_free_block=0;next_free_block<nBlocks; next_free_block++ ){
			AddFreeBlock(next_free_block, fileDesc);
	}

	//Initialize free i-node list
	superBlock.ninode=249;
	int next_free_inode=1; //First i-node is reserved for Root Directory
	for (i=0;i<=249;i++){
		superBlock.inode[i]=next_free_inode;
		next_free_inode++;
	}
	//Go to beginning of the second block
	lseek(fileDesc,BLOCK_SIZE,SEEK_SET);
	write(fileDesc,&superBlock,BLOCK_SIZE);

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
	lseek(fileDesc,allocBlock*BLOCK_SIZE,SEEK_SET); //move to the beginning of first available block
	write(fileDesc,&fileName,16);
	strcpy(fileName.fileName, "..");
	write(fileDesc,&fileName,16);

	//Point first inode to file directory
	printf("\nDirectory in the block %i",allocBlock);
	rootInode.addr[0]=allocBlock; // Allocate block to file directory
	write_inode(1,rootInode,fileDesc);

	return 0;
}

int CopyIN(const char* extFile,const char* fsFile,FILE* fileDesc){
	printf("\nInside cpin, copy from %s to %s \n",extFile, fsFile);
	INode iNode;
	int newblocknum;
	unsigned long file_size;
	FILE* srcfd;
	unsigned char reader[BLOCK_SIZE];

	int file_node_num = get_inode_by_file_name(fsFile,fileDesc);
	if (file_node_num !=-1){
		printf("\nFile %s already exists. Choose different name",fsFile);
		return -1;
	}

	if( access( extFile, F_OK ) != -1 ) {
		// file exists
		printf("\nCopy From File %s exists. Trying to open...\n",extFile);
		srcfd = fopen(extFile, "rb");
		fseek(srcfd, 0L, SEEK_END); //Move to the end of the file
		file_size = ftell(srcfd);
		if (file_size == 0){
			printf("\nCopy from File %s doesn't exists. Type correct file name\n",extFile);
			return -1;
			}
		else{
			printf("\nCopy from File size is %i\n",file_size);
			rewind(fileDesc);
			}
		}
	else {
		// file doesn't exist
		printf("\nCopy from File %s doesn't exists. Type correct file name\n",extFile);
		return -1;
		}

	if (file_size > pow(2,32)){
		printf("\nThe file is too big %s, maximum supported size is 4GB",file_size);
		return 0;
	}
	//Add file name to directory
	int to_file_inode_num = add_file_to_dir(fsFile,fileDesc);
	//Initialize and load inode of new file
	iNode = init_file_inode(to_file_inode_num,file_size, fileDesc);
	write_inode(to_file_inode_num, iNode, fileDesc);

	//Copy content of extFile into to_file_name
	int num_blocks_read=1;
	int total_num_blocks=0;
	fseek(srcfd, 0L, SEEK_SET);	 // Move to beginning of the input file
	int block_order=0;
	while(num_blocks_read == 1){
			// Read one block at a time from source file
			num_blocks_read = fread(&reader,BLOCK_SIZE,1,srcfd);
			total_num_blocks+= num_blocks_read;
			newblocknum = get_free_block(fileDesc);

			if (newblocknum == -1){
				printf("\nNo free blocks left. Total blocks read so far:%i",total_num_blocks);
				return -1;
			}
			if (file_size > BLOCK_SIZE*23)
				add_block_to_inode(block_order,newblocknum, to_file_inode_num, fileDesc);
			else
				add_block_to_inode_small_file(block_order,newblocknum, to_file_inode_num, fileDesc);
			// Write one block at a time into target file
			fseek(fileDesc, newblocknum*BLOCK_SIZE, SEEK_SET);
			fwrite(&reader, sizeof(reader), 1, fileDesc);
			block_order++;
	}
	return 0;
}

int CopyOUT(const char* fsFile,const char* extFile,FILE* fileDesc){
	int file_node_num;
	file_node_num = get_inode_by_file_name(fsFile,fileDesc);
	if (file_node_num ==-1){
		printf("\nFile %s not found",fsFile);
		return -1;
	}
	FILE* write_to_file;
	unsigned char buffer[BLOCK_SIZE];
	int next_block_number, block_number_order, number_of_blocks,file_size;
	write_to_file = fopen(extFile,"w");
	block_number_order=0;
	file_size = get_inode_file_size(file_node_num, fileDesc);

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
			next_block_number = get_block_for_big_file(file_node_num,block_number_order,fileDesc);
		else
			next_block_number = get_block_for_small_file(file_node_num,block_number_order,fileDesc);

		if(next_block_number == 0)
			return 0;

		fseek(fileDesc, next_block_number*BLOCK_SIZE, SEEK_SET);
		if ((block_number_order <(number_of_blocks-1)) || (number_of_bytes_last_block ==0)){
			fread(buffer,sizeof(buffer),1,fileDesc);
			fwrite(buffer,sizeof(buffer),1,write_to_file);
			}
		else {
			fread(last_buffer,sizeof(last_buffer),1,fileDesc);
			fwrite(last_buffer,sizeof(last_buffer),1,write_to_file);
			}
		block_number_order++;
	}

	fclose(write_to_file);
	return 0;
}

int MakeDirectory(const char* dirName,FILE* fileDesc){
	INode directory_inode, free_node;
	FileName fileName;
	int to_file_inode_num, flag, file_node_num;

	file_node_num = get_inode_by_file_name(dirName,fileDesc);
	if (file_node_num !=-1){
		printf("\nDirectory %s already exists. Choose different name",dirName);
		return -1;
	}

	int found = 0;
	to_file_inode_num=1;
	while(found==0){
		to_file_inode_num++;
		free_node = read_inode(to_file_inode_num,fileDesc);
		flag = MID(free_node.flags,15,16);
		if (flag == 0){
			found = 1;
		}
	}

	directory_inode = read_inode(1,fileDesc);

	// Move to the end of directory file
	printf("\nDirectory node block number is %i",directory_inode.addr[0]);
	fseek(fileDesc,(BLOCK_SIZE*directory_inode.addr[0]+directory_inode.size1),SEEK_SET);
	// Add record to file directory
	fileName.inodeOffset = to_file_inode_num;
	strcpy(fileName.fileName, dirName);
	fwrite(&fileName,16,1,fileDesc);

	//Update Directory file inode to increment size by one record
	directory_inode.size1+=16;
	write_inode(1,directory_inode,fileDesc);

	//Initialize new directory file-node
	free_node.flags=0;       //Initialize
	free_node.flags |=1 <<15; //Set first bit to 1 - i-node is allocated
	free_node.flags |=1 <<14; // Set 2-3 bits to 10 - i-node is directory
	free_node.flags |=0 <<13;

	write_inode(to_file_inode_num, free_node, fileDesc);

	return 0;
}

int Rm(const char* filename,FILE* fileDesc){
	printf("\nInside Rm, remove file %s",filename);
	int file_node_num, file_size, block_number_order, next_block_number,i;
	INode file_inode;
	unsigned char bit_14; //Plain file or Directory bit
	file_node_num = get_inode_by_file_name(filename,fileDesc);
	if (file_node_num ==-1){
		printf("\nFile %s not found",filename);
		return -1;
	}
	file_inode = read_inode(file_node_num, fileDesc);
	bit_14 = MID(file_inode.flags,14,15);
	if (bit_14 == 0){ //Plain file
		printf("\nRemove Plain file");
		file_size = get_inode_file_size(file_node_num, fileDesc);
		block_number_order = file_size/BLOCK_SIZE;
		if (file_size%BLOCK_SIZE !=0)
				block_number_order++;  //Take care of truncation of integer devision

		block_number_order--; //Order starts from zero
		while(block_number_order>0){
			if (file_size > BLOCK_SIZE*23)
				next_block_number = get_block_for_big_file(file_node_num,block_number_order,fileDesc);
			else
				next_block_number = file_inode.addr[block_number_order];

			add_block_to_free_list(next_block_number, fileDesc);
			block_number_order--;
		}
	}
	file_inode.flags=0; //Initialize inode flag and make it unallocated
	for (i = 0;i<23;i++)
		file_inode.addr[i] = 0;
	write_inode(file_node_num, file_inode, fileDesc);
	remove_file_from_directory(file_node_num, fileDesc);
	fflush(fileDesc);
	return 0;
}

//***************************************************************************
// AddFreeBlock: Adds a free block and update the super-block accordingly
//***************************************************************************
void AddFreeBlock(int blockNumber, FILE* fileDesc){
	SuperBlock superBlock;
	FreeBlock copyToBlock;

	// read and update existing superblock
	lseek(fileDesc, BLOCK_SIZE, SEEK_SET);
	read(fileDesc,&superBlock,sizeof(superBlock));

	// if free array is full
	// copy content of superblock to new block and point to it
	if (superBlock.nfree == 250){
		copyToBlock.nfree=250;
		copy_int_array(superBlock.free, copyToBlock.free, 250);
		lseek(fileDesc,blockNumber*BLOCK_SIZE,SEEK_SET);
		write(fileDesc,&copyToBlock,sizeof(copyToBlock));
		superBlock.nfree = 1;
		superBlock.free[0] = blockNumber;
	}
	else {  // free array is NOT full
		superBlock.free[superBlock.nfree] = blockNumber;
		superBlock.nfree++;
	}

	// write updated superblock to filesystem
	fseek(fileDesc, BLOCK_SIZE, SEEK_SET);
	fwrite(&superBlock,sizeof(superBlock),1,fileDesc);
}

void copy_int_array(unsigned short *from_array, unsigned short *to_array, int buf_len){
	int i;
	for (i=0;i<buf_len;i++){
		to_array[i] = from_array[i];
	}
}

void add_block_to_inode(int block_order_num, int blockNumber, int to_file_inode_num, FILE* fileDesc){  //Assume large file
	INode file_inode;
	unsigned short block_num_tow = blockNumber;
	unsigned short sec_in;
	unsigned short third_in;
	unsigned short first_in;
	file_inode = read_inode(to_file_inode_num, fileDesc);


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
			third_in = get_free_block(fileDesc);
			file_inode.addr[block_order_num] = third_in;
		}
		if(prev_block2 < logical_block2){
			sec_in = get_free_block(fileDesc);
			fseek(fileDesc, file_inode.addr[22]*BLOCK_SIZE+(logical_block2)*2, SEEK_SET);
			fwrite(&sec_in,sizeof(sec_in),1,fileDesc);
			fflush(fileDesc);
		}

		if(prev_block < logical_block){
			first_in = get_free_block(fileDesc);

			fseek(fileDesc, file_inode.addr[22]*BLOCK_SIZE+(logical_block2)*2, SEEK_SET);
			fread(&sec_in,sizeof(sec_in),1,fileDesc);
			fseek(fileDesc, sec_in*BLOCK_SIZE+(logical_block)*2, SEEK_SET);
			fwrite(&first_in,sizeof(first_in),1,fileDesc);
			fflush(fileDesc);
		}

		fseek(fileDesc, sec_in*BLOCK_SIZE+(logical_block)*2, SEEK_SET);
		fread(&first_in,sizeof(first_in),1,fileDesc);

		fseek(fileDesc, first_in*BLOCK_SIZE+(word_in_block2)*2, SEEK_SET);
		fwrite(&block_num_tow,sizeof(block_num_tow),1,fileDesc);

		}

	write_inode(to_file_inode_num, file_inode, fileDesc);

}

void add_block_to_inode_small_file(int block_order_num, int blockNumber, int to_file_inode_num, FILE* fileDesc){
	//Assume small file
	INode file_inode;
	unsigned short block_num_tow = blockNumber;
	file_inode = read_inode(to_file_inode_num, fileDesc);
	file_inode.addr[block_order_num] = block_num_tow;
	write_inode(to_file_inode_num, file_inode, fileDesc);
	return;
}

int get_free_block(FILE* fileDesc){
	SuperBlock superBlock;
	FreeBlock copy_from_block;
	int free_block;

	fseek(fileDesc, BLOCK_SIZE, SEEK_SET);
	fread(&superBlock,sizeof(superBlock),1,fileDesc);
	superBlock.nfree--;
	free_block = superBlock.free[superBlock.nfree];
	if (free_block ==0){ 											// No more free blocks left
		printf("(\nNo free blocks left");
		fseek(fileDesc, BLOCK_SIZE, SEEK_SET);
		fwrite(&superBlock,sizeof(superBlock),1,fileDesc);
		fflush(fileDesc);
		return -1;
	}

	// Check if need to copy free blocks from linked list
	if (superBlock.nfree == 0) {
		fseek(fileDesc, BLOCK_SIZE*superBlock.free[superBlock.nfree], SEEK_SET);
		fread(&copy_from_block,sizeof(copy_from_block),1,fileDesc);
		superBlock.nfree = copy_from_block.nfree;
		copy_int_array(copy_from_block.free, superBlock.free, 250);
		superBlock.nfree--;
		free_block = superBlock.free[superBlock.nfree];
	}

	fseek(fileDesc, BLOCK_SIZE, SEEK_SET);
	fwrite(&superBlock,sizeof(superBlock),1,fileDesc);
	fflush(fileDesc);
	return free_block;
}

void add_block_to_free_list(int freed_block_number, FILE* fileDesc){
	SuperBlock superBlock;
	FreeBlock copyToBlock;

	fseek(fileDesc, BLOCK_SIZE, SEEK_SET);
	fread(&superBlock,sizeof(superBlock),1,fileDesc);

	if (superBlock.nfree < 250){
		superBlock.free[superBlock.nfree] = freed_block_number;
		superBlock.nfree++;
	}
	else{
		copy_int_array(superBlock.free, copyToBlock.free, 250);
		copyToBlock.nfree = 250;
		fseek(fileDesc, BLOCK_SIZE*freed_block_number, SEEK_SET);
		fwrite(&copyToBlock,sizeof(copyToBlock),1,fileDesc);
		superBlock.nfree =1;
		superBlock.free[0] = freed_block_number;
		}
	fseek(fileDesc, BLOCK_SIZE, SEEK_SET);
	fwrite(&superBlock,sizeof(superBlock),1,fileDesc);
	fflush(fileDesc);

	return;
}

int add_file_to_dir(const char* fsFile,FILE* fileDesc){
	INode directory_inode, free_node;
	FileName fileName;
	int to_file_inode_num, flag;
	int found = 0;
	to_file_inode_num=1;
	while(found==0){
		to_file_inode_num++;
		free_node = read_inode(to_file_inode_num,fileDesc);
		flag = MID(free_node.flags,15,16);
		if (flag == 0){
			found = 1;
		}
	}

	directory_inode = read_inode(1,fileDesc);

	// Move to the end of directory file
	fseek(fileDesc,(BLOCK_SIZE*directory_inode.addr[0]+directory_inode.size1),SEEK_SET);
	// Add record to file directory
	fileName.inodeOffset = to_file_inode_num;
	strcpy(fileName.fileName, fsFile);
	fwrite(&fileName,16,1,fileDesc);

	//Update Directory file inode to increment size by one record
	directory_inode.size1+=16;
	write_inode(1,directory_inode,fileDesc);

	return to_file_inode_num;

}

INode init_file_inode(int to_file_inode_num, unsigned int file_size, FILE* fileDesc){
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

INode read_inode(int to_file_inode_num, FILE* fileDesc){
	INode to_file_inode;
	fseek(fileDesc,(BLOCK_SIZE*2+inode_size*(to_file_inode_num-1)),SEEK_SET); //move to the beginning of inode number to_file_inode_num
	fread(&to_file_inode,inode_size,1,fileDesc);
	return to_file_inode;
}

int write_inode(int inode_num, INode inode, FILE* fileDesc){
	fseek(fileDesc,(BLOCK_SIZE*2+inode_size*(inode_num-1)),SEEK_SET); //move to the beginning of inode with inode_num
	fwrite(&inode,inode_size,1,fileDesc);
	return 0;
}


unsigned short get_block_for_big_file(int file_node_num,int block_number_order, FILE* fileDesc){
	INode file_inode;
	unsigned short block_num_tow;
	unsigned short sec_ind_block;
	unsigned short third_ind_block;

	file_inode = read_inode(file_node_num,fileDesc);
    int logical_block = (block_number_order-22)/512;
	int logical_block2 = logical_block/512;
	int word_in_block = (block_number_order-22) % (512*512);
	if (block_number_order <22){
		return (file_inode.addr[block_number_order]);
	}
	else{
		// Read block number of second indirect block
		fseek(fileDesc, file_inode.addr[22]*BLOCK_SIZE+(logical_block2)*2, SEEK_SET);
		fread(&sec_ind_block,sizeof(sec_ind_block),1,fileDesc);

		// Read third indirect block number from second indirect block
		fseek(fileDesc, sec_ind_block*BLOCK_SIZE+logical_block*2, SEEK_SET);
		fread(&third_ind_block,sizeof(block_num_tow),1,fileDesc);

		// Read target block number from third indirect block
		fseek(fileDesc, third_ind_block*BLOCK_SIZE+word_in_block*2, SEEK_SET);
		fread(&block_num_tow,sizeof(block_num_tow),1,fileDesc);
	}

	return block_num_tow;

}

unsigned short get_block_for_small_file(int file_node_num,int block_number_order, FILE* fileDesc){
	INode file_inode;
	file_inode = read_inode(file_node_num,fileDesc);
	return (file_inode.addr[block_number_order]);
}

unsigned int get_inode_file_size(int to_file_inode_num, FILE* fileDesc){
	INode to_file_inode;
	unsigned int file_size;

	unsigned short bit0_15;
	unsigned char bit16_23;
	unsigned short bit24;

	to_file_inode = read_inode(to_file_inode_num, fileDesc);

	bit24 = LAST(to_file_inode.flags,1);
	bit16_23 = to_file_inode.size0;
	bit0_15 = to_file_inode.size1;

	file_size = (bit24<<24) | ( bit16_23 << 16) | bit0_15;
	return file_size;
}

int get_inode_by_file_name(const char* filename,FILE* fileDesc){
	INode directory_inode;
	FileName fileName;

	directory_inode = read_inode(1,fileDesc);

	// Move to the end of directory file
	fseek(fileDesc,(BLOCK_SIZE*directory_inode.addr[0]),SEEK_SET);
	int records=(BLOCK_SIZE-2)/sizeof(fileName);
	int i;
	for(i=0;i<records; i++){
		fread(&fileName,sizeof(fileName),1,fileDesc);
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

void remove_file_from_directory(int file_node_num, FILE*  fileDesc){
	INode directory_inode;
	FileName fileName;

	directory_inode = read_inode(1,fileDesc);

	// Move to the beginning of directory file (3rd record)
	fseek(fileDesc,(BLOCK_SIZE*directory_inode.addr[0]),SEEK_SET); //Move to third record in directory
	int records=(BLOCK_SIZE-2)/sizeof(fileName);
	int i;
	fread(&fileName,sizeof(fileName),1,fileDesc);

	for(i=0;i<records; i++){

		if (fileName.inodeOffset == file_node_num){
			printf("File found! removing from the directory.");
			fseek(fileDesc, (-1)*sizeof(fileName), SEEK_CUR); //Go one record back
			fileName.inodeOffset = 0;
			memset(fileName.fileName,0,sizeof(fileName.fileName));
			fwrite(&fileName,sizeof(fileName),1,fileDesc);
			return;
			}

			fread(&fileName,sizeof(fileName),1,fileDesc);
	}
	printf("not able to delete from directory");
	return;
}
