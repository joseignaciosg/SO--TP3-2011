/********************************** 
 *
 *  cache.c
 *  	Galindo, Jose Ignacio
 *  	Homovc, Federico
 *  	Loreti, Nicolas
 *		ITBA 2011
 *
 ***********************************/

#include "../include/cache.h"
#include "../include/defs.h"
#include "../include/stdio.h"
#include "../include/utils.h"
#include "../include/atadisk.h"

blockVector cache_array[CACHE_BLOCKS];
int cache_freeblocks = CACHE_BLOCKS;
int dirties = 0;
int total_access_count = 0;


void remove_block_from_cache(int index) {
	blockVector b = cache_array[index];
	int j;
	if (b.dirty) {
		/*sincronize*/
		_disk_write(0x1f0, (char *) b.data, 1, b.num_block);
	}
	b.dirty = 0;
	b.access_count = 0;
	for (j = 0; j < 512; j++) {
		b.data[j] = '\0';
	}
	b.num_block = -1;
}

void free_least_used_blocks(int blocks) {
	int i, j;
	int min = 0;

	for (i = 0; i < blocks; i++) {
		min = 0;
		for (j = 0; j < CACHE_BLOCKS; j++) {
			if (cache_array[j].access_count < cache_array[min].access_count) {
				min = j;
			}
		}
		remove_block_from_cache(min);
	}
}

//Busca un index en el array correspondiente al numero de bloque.
int cache_findblock(int num) {

	int i;

	for (i = 0; i < CACHE_BLOCKS; i++) {
		if (cache_array[i].num_block == num) {
			return i;
		}
	}

	return -1;
}

//Busca un bloque libre en el array.
int cache_findfreeblock(){
	int i;

	for (i = 0; i < CACHE_BLOCKS; i++) {
		if (cache_array[i].num_block == -1) {
			return i;
		}
	}

	return -1;
}


//inserta apartir de un bloque base la data (se le pasa la cantidad de bloques como parametro)
int cache_insertblock(int baseblock, char * insertdata, int amountblocks){
	
	int i, fblock, count = 0;
	
	if ( insertdata == NULL ){
		return -1;
	}
	if( amountblocks > CACHE_BLOCKS){
		printf("Demasiado grande para cachear\n");
		return -1;
	}
	if( (cache_freeblocks - amountblocks) < 0 ){
		printf("Hay que liberar usados\n");
		free_least_used_blocks(amountblocks - cache_freeblocks);
	}else{
		for(i = 0; i < amountblocks; i++){
			if ( !cache_isinarray(baseblock + i) ){
				fblock = cache_findfreeblock();
				cache_array[fblock].num_block = baseblock + i;
				cache_array[fblock].dirty = 0;
				cache_array[fblock].access_count = 0;
				memcpy(cache_array[fblock].data,(insertdata+(512*i)),512);
				count++;
			}
		}
	}
	total_access_count++;
	if (total_access_count == MAX_TOTAL_ACCESS_COUNT) {
		sync();
		total_access_count = 0;
	}
	cache_freeblocks -= count;
	return count;
}

int cache_readblock(int baseblock, char * rcv_data, int amountblocks){
	
	int i, fblock, count = 0;
	
	if ( rcv_data == NULL ){
		return -1;
	}
	if( amountblocks > CACHE_BLOCKS){
		printf("Demasiado grande para cachear\n");
		return -1;
	}
	if( (cache_freeblocks - amountblocks) < 0 ){
		printf("Hay que liberar usados\n");
		free_least_used_blocks(amountblocks - cache_freeblocks);		
	}else{
		for(i = 0; i < amountblocks; i++){
			if ( !cache_isinarray(baseblock + i) ){
				fblock = cache_findfreeblock();
				cache_array[fblock].num_block = baseblock + i;
				cache_array[fblock].dirty = 0;
				cache_array[fblock].access_count = 0;
				_disk_read(0x1f0, cache_array[fblock].data, 1, baseblock + i + 1);
				memcpy(rcv_data + i * 512, cache_array[fblock].data, 512);
				count++;
			}else{
				memcpy(rcv_data + i * 512, cache_array[cache_findblock(baseblock + i)].data, 512);
			}
		}
	}
	total_access_count++;
	if (total_access_count == MAX_TOTAL_ACCESS_COUNT) {
		sync();
		total_access_count = 0;
	}
	cache_freeblocks -= count;
	return count;
}


//Basicamente pregunta si el bloque esta en el array o no...
int cache_isinarray(int num) {
	int i;

	for (i = 0; i < CACHE_BLOCKS && (cache_array[i].num_block <= num); i++) {
		//printf("ENTRE:array:%d-numblock:%d&i:%d\n",cache_array[i].num_block,num,i);	
		if (cache_array[i].num_block == num) {
			return 1;
		}
	}

	return 0;
}


//ordena el arreglo de bloques de manera ascendente (Creo ja)
void cache_sortarray() {
	mergesort(cache_array, 0, CACHE_BLOCKS - 1);
	return;
}

//Inicializa el array
void cache_initarray() {
	int i;
	for (i = 0; i < CACHE_BLOCKS; i++) {
		cache_array[i].num_block = -1;
		cache_array[i].dirty = 0;
		cache_array[i].access_count = 0;
	}
	return;
}


//Mergesort.
void mergesort(blockVector * a, int low, int high) {
	int i = 0;
	int length = high - low + 1;
	int pivot = 0;
	int merge1 = 0;
	int merge2 = 0;
	blockVector working[length];

	if (low == high)
		return;

	pivot = (low + high) / 2;

	mergesort(a, low, pivot);
	mergesort(a, pivot + 1, high);

	for (i = 0; i < length; i++)
		working[i] = a[low + i];

	merge1 = 0;
	merge2 = pivot - low + 1;

	for (i = 0; i < length; i++) {
		if (merge2 <= high - low)
			if (merge1 <= pivot - low)
				if (working[merge1].num_block > working[merge2].num_block
						&& working[merge2].num_block != -1)
					a[i + low] = working[merge2++];
				else
					a[i + low] = working[merge1++];
			else
				a[i + low] = working[merge2++];
		else
			a[i + low] = working[merge1++];
	}
}

void sync() {
	int i;
	for (i = 0; i < CACHE_BLOCKS; i++) {
		if (cache_array[i].dirty == 1) {
			/*sincronize*/
			_disk_write(0x1f0, (char *) cache_array[i].data, 1,
			cache_array[i].num_block);
		}
		cache_array[i].dirty = 0;
	}
}
