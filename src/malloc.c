/********************************** 
 *
 *  malloc.c
 *  	Galindo, Jose Ignacio
 *  	Homovc, Federico
 *  	Loreti, Nicolas
 *		ITBA 2011
 *
 ***********************************/


#define MALLOC 1
#define CALLOC 2

#define BLOCK_SIZE1 128
#define BLOCK_SIZE2 1024    // 1Kb
#define BLOCK_SIZE3 1048576 // 1MB

/*
 * from address 0x00300000
 * to address   0x01000000
 * block of size BLOCK_SIZE1
 * */
#define FIRST_BLOCK_SPACE_SIZE (0x01000000 - 0x00300000)
#define FIRST_BLOCK_SPACE_START 0x00300000

/*
 * from address 0x01000000
 * to address   0x04000000
 * block of size BLOCK_SIZE2
 * */
#define SECOND_BLOCK_SPACE_SIZE (0x04000000 - 0x01000000)
#define SECOND_BLOCK_SPACE_START 0x01000000

/*
 * from address 0x04000000
 * to address   0x07000000
 * block of size BLOCK_SIZE3
 * */
#define THIRD_BLOCK_SPACE_SIZE (0x07000000 - 0x04000000)
#define THIRD_BLOCK_SPACE_START 0x04000000

#define COUNT_BLOCKS1 (FIRST_BLOCK_SPACE_SIZE) / BLOCK_SIZE1
#define COUNT_BLOCKS2 (SECOND_BLOCK_SPACE_SIZE) / BLOCK_SIZE2
#define COUNT_BLOCKS3 (THIRD_BLOCK_SPACE_SIZE) / BLOCK_SIZE3

static char bitmapBlocks1[COUNT_BLOCKS1] = { 0 };
static char bitmapBlocks2[COUNT_BLOCKS2] = { 0 };
static char bitmapBlocks3[COUNT_BLOCKS3] = { 0 };

void * getBlock(int type_block, int type_call);
void * getBlocks(int size, int type_call);

/*void * malloc(int size) {
	void * temp;
	//printf("inside malloc\n");
	if (size <= BLOCK_SIZE1) {
		//printf("malloc here \n");
		temp = getBlock(1, MALLOC);
	} else if (size <= BLOCK_SIZE2) {
		temp = getBlock(2, MALLOC);
	} else if (size <= BLOCK_SIZE3) {
		temp = getBlock(3, MALLOC);
	} else {
		temp = getBlocks(size, MALLOC);
	}
	return temp;
}

void * calloc(int size) {
	void * temp;
	//printf("inside calloc\n");
	if (size <= BLOCK_SIZE1) {
		temp = getBlock(1, CALLOC);
	} else if (size <= BLOCK_SIZE2) {
		temp = getBlock(2, CALLOC);
	} else if (size <= BLOCK_SIZE3) {
		temp = getBlock(3, CALLOC);
	} else {
		temp = getBlocks(size, CALLOC);
	}
	return temp;
}*/



void * getBlock(int type_block, int type_call) {
	void * ret;
	char * aux;
	int i, j;
	switch (type_block) {
	case 1:
		for (i = 0; i < COUNT_BLOCKS1; i++) {
			if (!bitmapBlocks1[i]) { //is not used
				ret = (void*) (FIRST_BLOCK_SPACE_START + i * BLOCK_SIZE1);
				bitmapBlocks1[i] = 1;
				if (type_call == CALLOC) {
					aux = (char *) ret;
					for (j = 0; j < BLOCK_SIZE1; j++) {
						aux[j] = '\0';
					}
				}
				break;
			}
		}
		break;
	case 2:
		for (i = 0; i < COUNT_BLOCKS2; i++) {
			if (!bitmapBlocks2[i]) { //is not used
				ret = (void*) (SECOND_BLOCK_SPACE_START + i * BLOCK_SIZE2);
				bitmapBlocks2[i] = 1;
				if (type_call == CALLOC) {
					aux = (char *) ret;
					for (j = 0; j < BLOCK_SIZE2; j++) {
						aux[j] = '\0';
					}
				}
				break;
			}
		}
		break;
	case 3:
		for (i = 0; i < COUNT_BLOCKS3; i++) {
			if (!bitmapBlocks3[i]) { //is not used
				ret = (void*) (THIRD_BLOCK_SPACE_START + i * BLOCK_SIZE3);
				bitmapBlocks3[i] = 1;
				if (type_call == CALLOC) {
					aux = (char *) ret;
					for (j = 0; j < BLOCK_SIZE3; j++) {
						aux[j] = '\0';
					}
				}
				//writeScreen("last initialized third block : %d\n", 33 );
				//enter();
				//printf("last initialized third block : %d\n", i);
				break;
			}
		}
		break;
	}
	return ret;

}

//TODO
void * getBlocks(int size, int type_call) {
	void * ret;
	char * aux;
	int flag = 1,flag2 = 1;
	int i, j, k;
	int bocks_needed = (size / BLOCK_SIZE3) + 1;
	for (i = 0; (i < COUNT_BLOCKS3) && flag; i++) {
		flag2=1;
		if (!bitmapBlocks3[i]) { //is not used
			/*checks if the number of needed bocks is available*/
			for (j = i; j < bocks_needed ; j++) {
				if (bitmapBlocks3[j]) { //is used
					flag2=0;
					i = j - i + 1; // more efficient
					break;
				}
			}
			if (flag2){
				ret = (void*) (THIRD_BLOCK_SPACE_START + i * BLOCK_SIZE3);
				//bitmapBlocks1[i] = 1;
				if (type_call == CALLOC) {
					aux = (char *) ret;
					for (k = 0; k < BLOCK_SIZE3 * bocks_needed; k++) {
						aux[j] = '\0';
					}
				}
				/*actualizes the bitmap*/
				for (j = i; j < size / BLOCK_SIZE3; j++) {
					//bitmapBlocks3[j] = 1;
					//writeScreen("last initialized third block : %d\n", 33 );
					enter();
				}
				flag = 0; /*stops outer for*/
			}
		}
	}
	return ret;

}

/*void free(void * p){
	int curr_size;
	int count;
	int pos;
	if ( (int)p < SECOND_BLOCK_SPACE_START){
		curr_size = SECOND_BLOCK_SPACE_START - (int)p;
		count = curr_size / BLOCK_SIZE1;
		pos = COUNT_BLOCKS1 - count;
		bitmapBlocks1[pos] = 0;
//		printf("\npos: %d \n",pos);
//		printf("\np: %d \n",(int)p);
		//is necessary to clean it up?
	}else if( (int)p < THIRD_BLOCK_SPACE_START ){
		curr_size = THIRD_BLOCK_SPACE_START - (int)p;
		count = curr_size / BLOCK_SIZE2;
		pos = COUNT_BLOCKS2 - count;
		bitmapBlocks2[pos]=0;
//		printf("\npos: %d \n",pos);
//		printf("\npos: %d \n",(int)p);
	}else{
		curr_size = THIRD_BLOCK_SPACE_START + (THIRD_BLOCK_SPACE_SIZE) - (int)p;
		count = curr_size / BLOCK_SIZE3;
		//printf("\ncount: %d ",count);
		pos = COUNT_BLOCKS3 - count;
		//printf("\n bitmapBlocks3[pos] :%d",bitmapBlocks3[pos]);
		//printf("\n bitmapBlocks3[pos+1]  :%d",bitmapBlocks3[pos+1]);
		//printf("\n bitmapBlocks3[pos+2]  :%d",bitmapBlocks3[pos+2]);
		bitmapBlocks3[pos]=0;
		//printf("\npos: %d ",pos);
		//printf("\np: %d \n",(int)p);

	}

}*/

int nextfree = 0x300000;


 void * malloc (int size)
 {
 void * temp = (void*)nextfree;
 nextfree = nextfree + size;
 return temp;
 }

 void * calloc (int size, int quant)
 {
 int i;
 char * temp = malloc(size*quant);
 for(i=0;i<(size*quant);i++){
 temp[i] = '\0';
 }
 return (void*)temp;
 }
