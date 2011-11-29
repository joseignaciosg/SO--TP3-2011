

#define IS_DIRTY 1
#define NO_DIRTY 0
//#define CACHE_BLOCKS 128 /*2 Mb de cache?*/
#define CACHE_BLOCKS 256 /*2 Mb de cache?*/
#define TRUE 1
#define OK 1;
#define ERROR -1;
#define FREE_BLOCKS 32


typedef struct{
	char data[512];
	int num_block;
	int dirty;
	int access_count;
}blockVector;



void mergesort(blockVector a[], int low, int high);
int cache_searchblockpackage(int block, int quantblocks);
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
int flushall();

int cache_read(int block, char * data, int size);
int cache_write(int block, char * data, int size);

int hipotetical_write(int block, char * buffer, int size);
int hipotetical_read(int block, char * buffer, int size);
