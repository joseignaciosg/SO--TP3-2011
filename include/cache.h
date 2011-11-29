/********************************** 
 *
 *  cache.h
 *  	Galindo, Jose Ignacio
 *  	Homovc, Federico
 *  	Loreti, Nicolas
 *		ITBA 2011
 *
 ***********************************/

#define IS_DIRTY 1
#define NO_DIRTY 0
//#define CACHE_BLOCKS 128 /*2 Mb de cache?*/
#define CACHE_BLOCKS 256 /*2 Mb de cache?*/
#define TRUE 1
#define OK 1;
#define ERROR -1;
#define FREE_BLOCKS 32
#define MAX_TOTAL_ACCESS_COUNT 50


typedef struct{
	char data[512];
	int num_block;
	int dirty;
	int access_count;
}blockVector;

void mergesort(blockVector a[], int low, int high);
int cache_findblock(int num);
void cache_sortarray();
void cache_initarray();
int cache_isinarray(int num);
void sync();
int cache_readblock(int baseblock, char * rcv_data, int amountblocks);
int cache_insertblock(int baseblock, char * insertdata, int amountblocks);

