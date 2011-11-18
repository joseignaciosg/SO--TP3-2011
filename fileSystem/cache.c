#include<stdio.h>
#include<stdlib.h>
#include<time.h>
#include<string.h>

#define IS_DIRTY 1
#define NO_DIRTY 0
#define CACHE_BLOCKS 128

typedef struct{
	char data[512];
	int num_block;
	int dirty;
}blockVector;

void mergesort(blockVector a[], int low, int high);
int cache_findblock(int num);
char * cache_getdata(int num);
void cache_sortarray();
void cache_initarray();
void cache_removeblocks(int a, int b);
void cache_printblocks();
int cache_aprox32(int baseblock, int cantblocks);
int cache_getbase(int baseblock);
int cache_isinarray(int num);
void cache_freeall();

blockVector cache_array[CACHE_BLOCKS];
int cache_freeblocks = CACHE_BLOCKS;

int cache_findblock(int num){

	int i;

	for(i = 0; i < CACHE_BLOCKS; i++){
		if(cache_array[i].num_block == num){
			return i;
		} 
	}
	
	return -1;
}

int cache_findfreeblock(){
	int resp,i;

	for(i=0;i< CACHE_BLOCKS; i++){
		if( cache_array[i].num_block == -1 ){
			return i;
		}
	}

	return -1;
}

char * cache_getdata(int num){
	return cache_array[num].data;	
}

int cache_insertblock(int baseblock, char * insertdata, int amountblocks){
	
	int i, j,fblock;
	int cantblocks = cache_aprox32(baseblock,amountblocks);
	int numblock = cache_getbase(baseblock);
	printf("base:%d--offset:%d\n",numblock,cantblocks);
	
	if ( insertdata == NULL ){
		return -1;
	}
	if( cantblocks > CACHE_BLOCKS){
		printf("Demasiado grande para cachear\n");
		return -1;
	}
	if( (cache_freeblocks - cantblocks) < 0 ){
		//TODO:Liberarusados
		printf("Hay que liberar usados\n");
		//TODO:FlushALL.		
		cache_freeall();
		cache_insertblock(baseblock,insertdata,amountblocks);
		
	}else{
		for(j=numblock; j < (numblock+cantblocks); j+=32){
			if ( !cache_isinarray(j) ){
				for ( i = 0; i < 32; i++){
					fblock = cache_findfreeblock();
					cache_array[fblock].num_block = j + i;
					cache_array[fblock].dirty = 0;
					//memcpy(cache_array[fblock].data,(insertdata+(512*i)),512);
				}		
				cache_freeblocks = cache_freeblocks - cantblocks;
			}
		}
	}

}

void cache_freeall(){
	return	cache_removeblocks(0,CACHE_BLOCKS);

}
int cache_isinarray(int num){
	int i;

	for(i=0; i<CACHE_BLOCKS && (cache_array[i].num_block <= num);i++){
		//printf("ENTRE:array:%d-numblock:%d&i:%d\n",cache_array[i].num_block,num,i);	
		if(cache_array[i].num_block == num){
			return 1;
		}
	}
	
	return 0;
}
void cache_removeblocks(int a, int b){
	
	int i;

	for(i=0;i < CACHE_BLOCKS; i++){
		if( (cache_array[i].num_block >= a) || (cache_array[i].num_block <= b) ){
			cache_array[i].num_block = -1;
			cache_array[i].dirty = 0;	
			cache_freeblocks++;			
		}
	}
	
	return;

}

void cache_sortarray(){
	mergesort(cache_array, 0, CACHE_BLOCKS - 1);
	return;
}

void cache_initarray(){
	int i;
	for(i = 0; i < CACHE_BLOCKS; i++){
		cache_array[i].num_block = -1;
		cache_array[i].dirty = 0; 
	}
	return;
}

void cache_printblocks(){
	int i;

	for(i = 0; i < CACHE_BLOCKS; i++)
		printf(" %d", cache_array[i].num_block);
	printf("\n");
}

int cache_aprox32(int baseblock, int cantblocks){
	
	printf("APROX>>Base:%d-cantblocks:%d\n",baseblock,cantblocks);
	return ( (cache_getbase(baseblock+cantblocks) + 32) - cache_getbase(baseblock));
	
}

int cache_getbase(int baseblock){
	
	return ((int)(baseblock/32)*32)+1;
	
}

/* void main(void) {
	int i = 0;
	time_t start, stop;
	clock_t ticks; long count;

		
	cache_initarray();
	//array before mergesort
	printf("Before    :");
	cache_printblocks();

	char data[512];
	char data2[512*3];
	char data3[512*4];

	cache_insertblock(10,data,1);
	cache_insertblock(11,data,1);
	cache_insertblock(12,data,1);
	cache_insertblock(20,data2,3);
	cache_insertblock(30,data3,4);
	
	cache_insertblock(500,data3,11);
cache_insertblock(400,data3,80);
   	time(&start);
	cache_sortarray();
	time(&stop);

	// array after mergesort
	printf("Mergesort :");
	cache_printblocks();

   	printf("Used %0.3f seconds of CPU time. \n", (double)ticks/CLOCKS_PER_SEC);
   	printf("Finished in about %.0f seconds. \n", difftime(stop, start));
	printf("\n");
	return 0;
}*/

void mergesort(blockVector * a, int low, int high) {
	int i = 0;
	int length = high - low + 1;
	int pivot  = 0;
	int merge1 = 0;
	int merge2 = 0;
	blockVector working[length];

	 if(low == high)
	  return;

	 pivot  = (low + high) / 2;

	 mergesort(a, low, pivot);
	 mergesort(a, pivot + 1, high);
	 
	 for(i = 0; i < length; i++)
	  working[i] = a[low + i];

	 merge1 = 0;
	 merge2 = pivot - low + 1;

	 for(i = 0; i < length; i++) {
	 if(merge2 <= high - low)
	   if(merge1 <= pivot - low)
	    if(working[merge1].num_block > working[merge2].num_block && working[merge2].num_block != -1 )
	     a[i + low] = working[merge2++];
	    else
	     a[i + low] = working[merge1++];
	   else
	    a[i + low] = working[merge2++];
	  else
	   a[i + low] = working[merge1++];
	 }
}
