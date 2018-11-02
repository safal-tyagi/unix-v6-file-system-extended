# Unix V6 File System (Extended)
OPERATING SYSTEM PROJECT 2 PART A- Create a Modified Version of the UNIX V6 File System
===========================================================================================================
Team Members:
Safal Tyagi skt180001
Praneeth Varma vxk180026
Pallavi Pandey pxp170009
===========================================================================================================
Redesign Summary:
1. redesigned the inode structure where maximum allowed size is 32 bytes.
2. redesigned the superblock structure where maximum allowed size is 1024 bytes and the superblock used is 1018 bytes.
3. Size of free array is 250.
4. Size of inode array is 250.
5. Maximum file size allowed is 4GB
6. Size of the inode is 32bytes.
7. inode in each block is 32.
===========================================================================================================
Methods Used:
1. main
 INPUT:
a. Asks the user to enter any two of the commands InitFS or quit.
 OUTPUT:
a. If IniFS is entered by the user,it checks if the file exists.
b. Throws an error if the file doesn't exists.
c. Throws an error if the user requests more than the maximum allowed blocks.
d. If blocks requested is less than 4GB, it initializes the file system.
e. If the command entered is quit, it exits the program.
f. If the user enters any other command, it throws an error.
g. Prints the total number of commands executed.
===========================================================================================================
2. InitFS(FILE* fileSystem, long nBlocks, int nInodes)
 INPUT:
a. Path of the file that has to be created.
b. Number of blocks in the disk.
c. Number of i-nodes in the disk.
 WORKING:
a. Initializes the file system with zeroes to all the blocks using a char buffer.
b. Fills the buffer values with zeroes.
c. Calculates the number of blocks required by the inode.
d. If inodes is not perfectly divisible by 32 , uses another block to fit the remaining i-nodes.
e. Initializes the freelist array to zero.
f. Initializes the inode array to null.
g. Writes the superblock to the file system.
h. Sets the first free block as the data block.
i. Call the add free block function to add free blocks to the next free data block.
j. Initializes the flag by telling that the file is allocated and it is of the type directory.
k. Creates a root directory and sets offset to first i-node.
l. Points the first i-node to file directory.
===========================================================================================================
3. AddFreeBlock(long BlockNumber, FILE* fileSystem)

 INPUT:
a.Block Number 
b.File system

WORKING:
a. Reads the super block and if the free array is full,writes the content of super block to new block and adds the next free blocks to the free array.
b. If the free array is not full,adds the next free blocks to free array.
c. Finally, writes the updated superblock to the file system.


