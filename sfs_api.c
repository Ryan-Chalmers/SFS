#include "sfs_api.h"


block_t *file_system;
superblock_t sblock;

f_desc *fd_table;



void mkssfs(int fresh){
	
	if(fresh){

		init_fresh_disk("DISK", __BLOCK_SIZE__, __NUM_BLOCKS__);
		// array containing all the block for the file system
		file_system = calloc(__NUM_BLOCKS__, __BLOCK_SIZE__);

		//assign a magic number to identify the file system
		//unsigned char magic[] = "2222";

		//create a root inode
		inode_t root;
		root.size = 16*sizeof(int);

		//initialize the root pointers to point to the blocks containing the
		//inodes we are about to create
		for(int k = 0; k < 16; k++){
			root.direct[k] = 1024 * k;
		}

		//populate the superblock
		sblock.magic[0] = 0xff;
		sblock.magic[1] = 0xff;
		sblock.magic[2] = 0xff;
		sblock.magic[3] = 0xff;
		sblock.b_size = __BLOCK_SIZE__;
		sblock.num_blocks =__NUM_BLOCKS__;
		sblock.num_inodes = __NUM_INODES__;
		sblock.root = root;
	

		//create a block in memory to hold the super block
		block_t *super = calloc(1, __BLOCK_SIZE__);
		//copy the super block into that memory block
		memcpy(super, &sblock, sizeof(sblock));

		//file_system[0] = *super;

		int r = write_blocks(0,1, super);
		//set thr first block in that file system to that superblock

		
		unsigned char *buffer = calloc(5, __BLOCK_SIZE__);
		dnode_t *direct = (dnode_t *) buffer;

		for(int i = 0; i < 224; i++){
			direct[i].node = -1;
		}

		
		r = write_blocks(1, 5, buffer);

		int block_num = 20;

		//INITIALIZING INODES
		for(int i = 0; i < 14; i++){ 
	        //alloc enough space for one block
	        block_t *iblock = calloc(1, __BLOCK_SIZE__);
	        //allocate an array of 16 inodes
	        inode_t *inodes = calloc(16, sizeof(inode_t));

	        //iterate through the block adding inodes
	        for(int j = 0; j < 16 ; j++){
	        	//create a pointer to an inode array the size of a block
	           	memset(&inodes[j], 0, sizeof(inode_t));
	            //update the inode data for each inode in the array
	            inodes[j].size = -1;
	            for(int l = 0; l < 14; l++){
	            	inodes[j].direct[l] = block_num;
	            	block_num++;
	            }
	        }
	        //copy the inode array into the block
	        memcpy(iblock, inodes, sizeof(inode_t)*16);
	        //add the block to the file system
	        file_system[i] = *iblock;
	        free(inodes);
	        free(iblock);
	    }

	    fd_table = calloc(__NUM_INODES__, sizeof(f_desc));
	    //initialize fd_table
	    for(int z = 0; z < __NUM_INODES__;z++){
			fd_table[z].free_bit = 1;
			fd_table[z].inode = -1;
			fd_table[z].read_ptr = -1;
			fd_table[z].write_ptr = -1;
		}
		
		write_blocks(6, 14, file_system);
	    //free(file_system);
		free(super);
	} else {

		init_disk("DISK", __BLOCK_SIZE__, __NUM_BLOCKS__);

		fd_table = calloc(__NUM_INODES__, sizeof(f_desc));
		for(int z = 0; z < __NUM_INODES__;z++){
			fd_table[z].free_bit = 1;
			fd_table[z].inode = -1;
			fd_table[z].read_ptr = -1;
			fd_table[z].write_ptr = -1;
		}
	}
	//free(fd_table);
}

int ssfs_fopen(char *name){

	//assume this is the inode for the file
	int file_inode = -1;
	//allocate space for a filesystem(array of blocks)

	unsigned char *inode_blocks = calloc(14, __BLOCK_SIZE__);

	inode_t *inodes = (inode_t *) inode_blocks;

	int result = read_blocks(6,14, inodes);

	unsigned char *dnode_blocks = calloc(5, (__BLOCK_SIZE__));
	
	dnode_t *dnodes =  (dnode_t *) dnode_blocks;

	result = read_blocks(1, 5, dnodes);

	int rd_ptr = 0;
	int wt_ptr = 0;

	//look for the file with that name
	for(int i = 0; i < 224; i++){
		//if we find it set the file_node to that node
		if(!strcmp(dnodes[i].name, name)){
			//printf("Found File at: \n");
			file_inode = dnodes[i].node;
			wt_ptr = inodes[file_inode].size;
			break;
		} 
	}

	//no file was found
	if(file_inode == -1){
		//look for the first empty inode
		for(int i = 0; i < 224; i++){
			if(inodes[i].size == -1){ 
				//allocate empty node for the new file
				file_inode = i;
				inodes[i].size = 0;
				//find an empty spot in the directory
				for(int j = 0; j < 224; j++){
					if(dnodes[j].node == -1){
						dnodes[j].node = file_inode;
						strcpy(dnodes[j].name,name);
						break;
					}
				}
				break;
			}
		}
	} 

	int flag = 0;

	for(int i = 0; i < 224; i++){
		if(fd_table[i].inode == file_inode){
			flag = 1;
			break;
		}
	}

	int loc = -1;

	if(!flag){
		for(int i = 0; i < 224; i++){
			if(fd_table[i].free_bit == 1) {
				fd_table[i].inode = file_inode;
				fd_table[i].read_ptr = rd_ptr;
				fd_table[i].write_ptr = wt_ptr;
				fd_table[i].free_bit = 0;
				loc = i;
				break;

			}
		}
	}

	write_blocks(1,5, dnodes);
	write_blocks(6,14, inodes);

	free(dnodes);
	free(inodes);

	return loc;
}	
int ssfs_fclose(int fileID){

	if(fileID < 0){
		return -1;
	}
	if(fd_table[fileID].free_bit == 0){
		fd_table[fileID].inode = -1;
		fd_table[fileID].read_ptr = 0;
		fd_table[fileID].write_ptr = 0;
		fd_table[fileID].free_bit = 1;
		return 0;
	}
	
	//printf("FILE NOT FOUND\n");
	return -1;
}

int ssfs_frseek(int fileID, int loc){

	if(fileID < 0){
		return -1;
	}

	//read in the inodes
	unsigned char *inode_blocks = calloc(14, __BLOCK_SIZE__);

	inode_t *inodes = (inode_t *) inode_blocks;

	read_blocks(6,14, inodes);

	if(loc < 0){
		//printf("NEGATIVE SEEK ATTEMPTED\n");
		return -1;
	}

	//check the fd table for the file in question
	if(fd_table[fileID].free_bit == 1){
		//printf("FILE NOT FOUND\n");
		return -1;
	}

	//find the node we are seeking 
	inode_t node = inodes[fd_table[fileID].inode];

	//check to see if we are trying to read outside of the file
	if((fd_table[fileID].read_ptr + loc) > node.size){
		//printf("CANT READ PAST THE END OF THE FILE\n");
		return -1;
	}

	fd_table[fileID].read_ptr = loc;

	return 0;
}

int ssfs_fwseek(int fileID, int loc){

	if(fileID < 0){
		return -1;
	}
	//read in the inodes
	unsigned char *inode_blocks = calloc(14, __BLOCK_SIZE__);

	inode_t *inodes = (inode_t *) inode_blocks;

	read_blocks(6,14, inodes);

	if(loc < 0){
		//printf("NEGATIVE SEEK ATTEMPTED\n");
		return -1;
	}

	//check the fd table for the file in question
	if(fd_table[fileID].free_bit == 1){
		//printf("FILE NOT FOUND\n");
		return -1;
	}

	//find the node we are seeking 
	inode_t node = inodes[fd_table[fileID].inode];

	//check to see if we are trying to read outside of the file
	if((fd_table[fileID].read_ptr + loc) > node.size){
		//printf("CANT READ PAST THE END OF THE FILE\n");
		return -1;
	}

	fd_table[fileID].write_ptr = loc;

	return 0;
}

int ssfs_fwrite(int fileID, char *buf, int length){

	if(fileID < 0){
		return -1;
	}

	unsigned char *inode_blocks = calloc(14, __BLOCK_SIZE__);

	inode_t *inodes = (inode_t *) inode_blocks;

	read_blocks(6,14, inodes);

	unsigned char *file = calloc(14, __BLOCK_SIZE__);

	if(fd_table[fileID].free_bit == 1){
		//printf("FILE IS NOT OPEN\n");
		return -1;
	}

	

	// for(int i = 0; i < 14; i++){
	// 	//populate an array with all the blocks that make up the file
	// 	unsigned char *file_block = calloc(1, __BLOCK_SIZE__);
	// 	read_blocks(inodes[fileID].direct[i], 1, file_block);
	// 	file[i] = file_block[0];
	//}

	read_blocks(inodes[fileID].direct[0], 14, file);

	//unsigned char b = file[0];

	//this will give the file as one long char array
	//unsigned char *file_string = calloc(14*1024, sizeof(char));

	//printf("Buf: %s", file_string);
	//memcpy(file_string, file, sizeof(file_string));

	//copy into the buf starting at the read pointer
	for(int i = 0; i < length; i++){
		 file[i + fd_table[fileID].write_ptr] = buf[i];
	}

	//unsigned char block = (char *) w_blocks[0];

	//printf("%d\n",inodes[fileID].direct[0]);

	// for(int i = 0; i < 1024; i++){
	// 	printf("%x", file[i]);
	// }

	block_t *w_blocks = (block_t *) file;

	write_blocks(inodes[fileID].direct[0], 14, w_blocks);
	
	// for(int i = 0; i < 14; i++){
	// 	//repopulate an array with all the blocks that make up the file
	// 	block_t *w_block = calloc(1, __BLOCK_SIZE__);
	// 	w_block[0] = w_blocks[i];
	// 	write_blocks(inodes[fileID].direct[i], 1, w_block);
	// }

	free(inode_blocks);

	return length;
}

int ssfs_fread(int fileID, char *buf, int length){

	if(fileID < 0){
		return -1;
	}

	unsigned char *inode_blocks = calloc(14, __BLOCK_SIZE__);

	inode_t *inodes = (inode_t *) inode_blocks;

	read_blocks(6,14, inodes);

	block_t *file_blocks = calloc(14, __BLOCK_SIZE__);

	if(fd_table[fileID].free_bit == 1){
		//printf("FILE IS NOT OPEN\n");
		return -1;
	}



	
	read_blocks(inodes[fileID].direct[0], 14, file_blocks);


	unsigned char *file = (unsigned char * ) file_blocks;
	// for(int i = 0; i < 14; i++){
	// 	//populate an array with all the blocks that make up the file
	// 	unsigned char *file_block = calloc(1, __BLOCK_SIZE__);
	// 	read_blocks(inodes[fileID].direct[i], 1, file_block);
	// 	file[i] = file_block[0];
	// }

	//this will give the file as one long char array
	//unsigned char *file_string = calloc(14*1024, sizeof(char));

	// for(int i = 0; i < 14*1024; i++){
	// 	printf("%x", file_string[i]);
	// }
	//memcpy(file_string, file, sizeof(file_string));

	//copy into the buf starting at the read pointer

	

	for(int i = 0; i < length; i++){
		 buf[i] = file[i + fd_table[fileID].write_ptr];
	}

	free(inode_blocks);

	return length;
} 

int ssfs_remove(char *file){
	unsigned char *dnode_blocks = calloc(5, (__BLOCK_SIZE__));
	
	dnode_t *dnodes =  (dnode_t *) dnode_blocks;

	int result = read_blocks(1, 5, dnodes);

	unsigned char *inode_blocks = calloc(14, __BLOCK_SIZE__);

	inode_t *inodes = (inode_t *) inode_blocks;

	result = read_blocks(6,14, inodes);

	//remove the file from the directory
	for(int i = 0; i < 224; i++){
		if(!strcmp(dnodes[i].name, file)){
			ssfs_fclose(dnodes[i].node);
			//clear I node and all blocks associated with the file
			for(int j = 0; j < 14; j++){
				//find each block the inode points to
				block_t *empty_block = calloc(1, (__BLOCK_SIZE__));

				int block_loc = inodes[dnodes[i].node].direct[j];
				//write an empty block to that location
				write_blocks(block_loc, 1, empty_block);

			}
			//set the node to be -1 and the name to be all zeroes in the directory
			memset(dnodes[i].name, 0, 16);
			dnodes[i].node = -1;
			write_blocks(1,5, dnodes);
			return 0;
			break;
		}
	}


	return -1;
}
