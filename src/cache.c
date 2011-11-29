//#include<stdio.h>
//#include<stdlib.h>
//#include<time.h>
//#include<string.h>

#include "../include/cache.h"
#include "../include/defs.h"

blockVector cache_array[CACHE_BLOCKS];
int cache_freeblocks = CACHE_BLOCKS;
int dirties = 0;
int total_access_count = 0;

/*double linked list for last used sectors*/
//blockVectorNode first = NULL;
//blockVectorNode last = NULL;

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
int cache_findfreeblock() {
	int resp, i;

	for (i = 0; i < CACHE_BLOCKS; i++) {
		if (cache_array[i].num_block == -1) {
			return i;
		}
	}

	return -1;
}

//retorna la data del bloque index en el array.
char * cache_getdata(int num) {
	return cache_array[num].data;
}

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

//inserta apartir de un bloque base la data (se le pasa la cantidad de bloques como parametro)
int cache_insertblock(int baseblock, char * insertdata, int amountblocks) {

	int i, j, fblock;
	int cantblocks = cache_aprox32(baseblock, amountblocks);
	int numblock = cache_getbase(baseblock);
	//printf("base:%d--offset:%d\n",numblock,cantblocks);

	if (insertdata == NULL) {
		return -1;
	}
	if (cantblocks > CACHE_BLOCKS) {
		printf("Demasiado grande para cachear\n");
		return -1;
	}
	if ((cache_freeblocks - cantblocks) < 0) {
		free_least_used_blocks(cantblocks - cache_freeblocks);
		//printf("Hay que liberar usados\n");
		//TODO:FlushALL.		
		cache_freeall();
		cache_insertblock(baseblock, insertdata, amountblocks);

	} else {
		for (j = numblock; j < (numblock + cantblocks); j += FREE_BLOCKS) {
			if (!cache_isinarray(j)) {
				for (i = 0; i < FREE_BLOCKS; i++) {
					fblock = cache_findfreeblock();
					cache_array[fblock].num_block = j + i;
					cache_array[fblock].dirty = 0;
					cache_array[fblock].access_count = 0;
					//memcpy(cache_array[fblock].data,(insertdata+(512*i)),512);
				}
				cache_freeblocks = cache_freeblocks - cantblocks;
			}
		}
	}

}

//setea todos los bloques en -1 desde el 0 hasta el CACHE_BLOCKS.
void cache_freeall() {
	return cache_removeblocks(0, CACHE_BLOCKS);

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

//le pone un -1 al bloque para que "desaparezca" y aumenta la cantidad de bloques libres.
void cache_removeblocks(int a, int b) {

	int i;

	for (i = 0; i < CACHE_BLOCKS; i++) {
		if ((cache_array[i].num_block >= a)
				|| (cache_array[i].num_block <= b)) {
			cache_array[i].num_block = -1;
			cache_array[i].dirty = 0;
			cache_array[i].access_count = 0;
			cache_freeblocks++;
		}
	}

	return;

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

//Imprime los bloques.
void cache_printblocks() {
	int i;

	for (i = 0; i < CACHE_BLOCKS; i++)
		printf(" %d", cache_array[i].num_block);
	printf("\n");
}

//Aproxima en base al numero que le pasan a 32 bloques
int cache_aprox32(int baseblock, int cantblocks) {

	//printf("APROX>>Base:%d-cantblocks:%d\n",baseblock,cantblocks);
	return ((cache_getbase(baseblock + cantblocks) + FREE_BLOCKS)
			- cache_getbase(baseblock));

}

//Devuelve la base del bloque para tomar 32.
int cache_getbase(int baseblock) {

	return ((int) (baseblock / FREE_BLOCKS) * FREE_BLOCKS) + 1;

}

//Seria el READ posta
int hipotetical_read(int block, char * buffer, int size) {

	int reads;
	int quantblocks = (int) (size / 512) + 1;
	//Si la cantidad de bloques supera a la de la cache entonces leo directamente de disco.	
	if (quantblocks > CACHE_BLOCKS) {
		return -1; //disk_read(block,buffer,size);
	}
	//Si no estan todos los bloques juntos, entonces leo e inserto en al cache.
	if (cache_searchblockpackage(block, quantblocks) == -1) {
		//reads = disk_read(block,buffer,size);
		cache_insertblock(block, buffer, quantblocks);
		//return reads;
	}

	return cache_read(block, buffer, size);

	//calculo la cantidad de bloques.
	//me fijo en la cache si estan todos los bloques.
	//si no esta leo y pongo todo en cache.
	//devuelvo lo que esta en la cache en el buffer
}

//Seria el Write posta.
int hipotetical_write(int block, char * buffer, int size) {

	int quantblocks = (int) (size / 512) + 1;
	if (quantblocks > CACHE_BLOCKS) {
		return -1; //disk_write(block,buffer,size);
	}
	//Si no estan todos los bloques, entonces leo y meto en la cache.
	if (cache_searchblockpackage(block, quantblocks) == -1) {
		char * buffer2 = (char *) malloc(size);
		hipotetical_read(block, buffer2, size);
	}
	return cache_write(block, buffer, size);

	//calculo la cantidad de bloques.
	//si es muy grande lo que me piden para cache leo de disco directamente.
	//me fijo si estan en la cache.
	//si no estan me los traigo.
	//escribo de la cache en el buffer y marco como dirty.

}

//Escribe en la cache
int cache_write(int block, char * data, int size) {

	int i;
	int init_block = cache_findblock(block);
	for (i = 0; i < ((int) (size / 512) + 1); i++) {
		memcpy(cache_array[init_block + i].data, data + (512 * i), 512);
		if (!cache_array[init_block + i].dirty) {
			dirties++;
		}
		cache_array[init_block + i].dirty = 1; //TRUE
		cache_array[init_block + i].access_count += 1;
	}
	total_access_count++;
	if (total_access_count == MAX_TOTAL_ACCESS_COUNT) {
		sync();
		total_access_count = 0;
	}
	return size;
}

//Leee de la cache.
int cache_read(int block, char * data, int size) {
	int i;
	int init_block = cache_findblock(block);
	for (i = 0; i < ((int) (size / 512) + 1); i++) {
		memcpy(data + (512 * i), cache_array[init_block + i].data, 512);
		cache_array[init_block + i].access_count += 1;

	}
	total_access_count++;
	if (total_access_count == MAX_TOTAL_ACCESS_COUNT) {
		sync();
		total_access_count = 0;
	}
	return size;
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

//Flush , si lee dirty escribe en disco y lo marca como no flush.
int flushall() {

	int i, size;
	int init_writeblock;

	size = 512;

	for (i = 0; i < CACHE_BLOCKS; i++) {

		if (cache_array[i].dirty == 1) {
			//disk_write(cache_array[i].num_block, cache_array[i].data,size);
			cache_array[i].dirty = 0;
			cache_array[i].access_count = 0;

			dirties--;
		}
	}

	if (dirties != 0) {
		return ERROR;
	}

	return OK;
}

//Busca si esta el paquete completo de bloques desde uno inicial hasta la cant que le pasan.
int cache_searchblockpackage(int block, int quantblocks) {

	int i, flag, init_block;
	flag = TRUE;
	init_block = cache_findblock(block);

	for (i = 0; i < quantblocks; i++) {
		if (cache_array[init_block + i].num_block != (init_block + i)) {
			flag = -1; //FALSE			
			return flag;
		}
	}

	return flag;
}

/*
 int main(void) {
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
 }
 */

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
