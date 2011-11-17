/********************************** 
*
*  malloc.c
*  	Galindo, Jose Ignacio
*  	Homovc, Federico
*  	Loreti, Nicolas
*		ITBA 2011
*
***********************************/

#include "../include/malloc.h"


typedef unsigned int  size_t;
#define NULL 0

/*
 * If MAX_MEMORY_IN_MB isn't set, then it defaults to 200 MB
 *
 * It seems that linking time is (directly?) proportional to this number,
 * so keep it low while testing, and remember to raise when releasing
 */
#ifndef MAX_MEMORY_IN_MB
#define MAX_MEMORY_IN_MB  10
#endif

/*
 * MAX_MEMORY in bytes
 */
#define MAX_MEMORY  ( (size_t)( 1024 * 1024 * MAX_MEMORY_IN_MB ) )

/*
 * number of bins in the bin table
 */
#define BIN_NUMBER  ( sizeof( bin_sizes ) / sizeof( bin_sizes[0] ) )

/*
 * bin array
 */
#define BINS  ( (struct free_header*)(memory) )

/*
 * position in memory of the i-th bin
 */
#define BIN_ABSOLUTE_POS( i ) ( (i) * sizeof( struct free_header ) )

/*
 * determines if the i-th bin is empty
 */
#define BIN_ISEMPTY( i ) ( BINS[i].next_pos == BIN_ABSOLUTE_POS( i ) )

/*
 * starting position in memory of "memory available to the user"
 */
#define MEMORY_START     ( BIN_NUMBER * sizeof( struct free_header ) )

/*
 * open (+1) ending position in memory of "memory available to the user"
 */
#define MEMORY_END       ( MEMORY_START + MAX_MEMORY )


/*
 * Bin sizes from 16 bytes to 2 GB
 *
 * Note: bin_sizes[0] is never used, since a free_header must fit in a free chunk.
 */
static const size_t bin_sizes[] = {
	     8,    16,    24,    32,    40,    48,    56,    64,    72,    80,
	    88,    96,   104,   112,   120,   128,   136,   144,   152,   160,
	   168,   176,   184,   192,   200,   208,   216,   224,   232,   240,
	   248,   256,   264,   272,   280,   288,   296,   304,   312,   320,
	   328,   336,   344,   352,   360,   368,   376,   384,   392,   400,
	   408,   416,   424,   432,   440,   448,   456,   464,   472,   480,
	   488,   496,   504,   512,   576,   640,   768,  1024,  2048,  4096,
	     0x2000,     0x4000,     0x8000,    0x10000,    0x20000,    0x40000,
	    0x80000,   0x100000,   0x200000,   0x400000,   0x800000,  0x1000000,
	  0x2000000,  0x4000000,  0x8000000, 0x10000000, 0x20000000, 0x40000000,
	 0x80000000
};


/*
 * Possible values of the status flag in a memory chunk header
 */
#define FREE_STATUS  0
#define INUSE_STATUS 1


/*
 * Header of a free memory chunk, and macros to get its size, and to get
 * the header from a specific position in memory
 */
#define GET_FREE_HEADER( pos ) ( (struct free_header*)&memory[pos] )

struct free_header {
	unsigned int status : 1;
	size_t size : 31;
	size_t prev_pos;
	size_t next_pos;
};


/*
 * Header of a memory chunk in use, and macros to get its size, and to get
 * the header from a specific position in memory
 */
#define GET_INUSE_HEADER( pos ) ( (struct inuse_header*)&memory[pos] )

struct inuse_header {
	unsigned int status : 1;
	size_t size : 31;
};


/*
 * Footer of a memory chunk, and macros to get its size, and to get
 * the footer from a specific position in memory
 */
#define GET_FOOTER( pos ) ((struct footer*)&memory[pos] )

struct footer {
	size_t size;
};


/*
 * Array of chars. Represents all "heap" memory.
 *
 * Note: memory[0..3] are not used.
 */
static char memory[ MEMORY_END ];


/*
 * Total free "memory available to the user"
 */
static size_t free_memory;


/*
 * Last chunk used to allocate. These are used to improve locallity.
 */
static size_t last_chunk;
static size_t last_chunk_size = 0;


/**
 * Performs a binary serach to find the first bin of size >= to a given value
 *
 * @param size  the size trying to be allocated (in bytes)
 *
 * @return the number of the bin, or 0 if an error ocurred
 *
 */
inline
static size_t find_bin ( size_t size ) {				/*se puede cambiar por busqueda lineal*/

	if ( size > bin_sizes[ BIN_NUMBER - 1 ] )
		return 0;

	size_t min_bin = 0, max_bin = BIN_NUMBER , curr_bin;

	while( min_bin + 1 != max_bin ) {

		curr_bin = ( min_bin + max_bin ) >> 1;

		if ( bin_sizes[curr_bin] <= size )
			min_bin = curr_bin;
		else
			max_bin = curr_bin;
	}

	return min_bin;
}


/**
 * Finds the first chunk of memory >= to a given size in a given bin
 *
 * @param bin   the bin to explore
 * @param size  the minimum size of the chunk
 *
 * @return the starting position of the chunk
 *
 */
inline
static size_t find_chunk ( size_t bin, size_t size ) {

	size_t chunk = BINS[bin].next_pos;

	while( chunk != BIN_ABSOLUTE_POS( bin ) &&
			GET_FREE_HEADER( chunk )->size < size ) {
		chunk = GET_FREE_HEADER( chunk )->next_pos;
	}

	return chunk;
}


/**
 * Finds the first chunk of memory > to a given size in a given bin.
 *
 * Note the use of > instead of >=, like in find_chunk. This version
 * is used to implement a tie breaking least-recently-used strategy,
 * which maintains (for some reason) low memory fragmentation
 *
 * @param bin   the bin to explore
 * @param size  the minimum size of the chunk
 *
 * @return the starting position of the chunk
 *
 */
inline
static size_t find_upper_chunk ( size_t bin, size_t size ) {

	size_t chunk = BINS[bin].next_pos;

	while( chunk != BIN_ABSOLUTE_POS( bin ) &&
		GET_FREE_HEADER( chunk )->size <= size ) {

		chunk = GET_FREE_HEADER( chunk )->next_pos;
	}

	return chunk;
}


/**
 * Initializes the memory and bins arrays.
 *
 * Must be called before any malloc or free.
 *
 */
void init_malloc () {

	size_t i;
	/* set bins */
	for(i = 0; i < BIN_NUMBER; i++ ) {
		BINS[i].size = sizeof( struct free_header );
		BINS[i].next_pos = BIN_ABSOLUTE_POS(i);
		BINS[i].prev_pos = BIN_ABSOLUTE_POS(i);
	}

	size_t bin = find_bin( MAX_MEMORY );
	BINS[bin].next_pos = MEMORY_START;
	BINS[bin].prev_pos = MEMORY_START;

	/* set header */
	struct free_header* main_memory_header = GET_FREE_HEADER( MEMORY_START );

	main_memory_header->status = FREE_STATUS;
	main_memory_header->size = MAX_MEMORY;
	main_memory_header->prev_pos = BIN_ABSOLUTE_POS( bin );
	main_memory_header->next_pos = BIN_ABSOLUTE_POS( bin );

	/* set footer */
	struct footer* main_memory_footer = GET_FOOTER( MEMORY_END - sizeof( struct footer ) );

	main_memory_footer->size = MAX_MEMORY;

	/* set free memory */
	free_memory = MAX_MEMORY;
}


/**
 * Splits a free chunk of memory in two chunks, the first of a given size, and
 * the second goes back to the set of free chunks
 *
 * @param chunk  the starting position of the chunk to split
 * @param size   the size of the desired chunk
 *
 * @return  a pointer to the first chunk (wich is setted "inuse")
 *
 */
static void* split ( size_t chunk, size_t size ) {

	size_t left_size = GET_FREE_HEADER( chunk )->size - size;

	if( left_size < sizeof( struct free_header ) + sizeof( struct footer ) ) {
		size += left_size;
		left_size = 0;
	}

	/* set headers and footers */
	struct inuse_header* first_header = GET_INUSE_HEADER( chunk );

	first_header->status = INUSE_STATUS;
	first_header->size = size;

	GET_FOOTER( chunk + size - sizeof( struct footer ) )->size = size;

	if ( left_size > 0 ) {
		struct free_header* second_header = GET_FREE_HEADER( chunk + size );

		second_header->status = FREE_STATUS;
		second_header->size = left_size;

		GET_FOOTER( chunk + size + left_size - sizeof( struct footer ) )->size = left_size;

		/* set free */
		size_t upper_chunk = find_upper_chunk( find_bin( left_size ), left_size );

		second_header->prev_pos = GET_FREE_HEADER( upper_chunk )->prev_pos;
		second_header->next_pos = upper_chunk;

		GET_FREE_HEADER( second_header->prev_pos )->next_pos = chunk + size;
		GET_FREE_HEADER( second_header->next_pos )->prev_pos = chunk + size;

		last_chunk = chunk + size;
		last_chunk_size = left_size;
	} else {
		last_chunk_size = 0;
	}

	free_memory -= size;
	return (char*)(first_header + 1);
}


/**
 * Allocs a chunk of memory of a given size.
 *
 * @param size  thesize trying to be allocated (in bytes)
 *
 * @return a pointer to the allocated memory, or NULL if an error ocurred
 *
 * For more info on the algorithm idea:
 * @see http://gee.cs.oswego.edu/dl/html/malloc.html
 *
 */
void* malloc (int size) {

	size += sizeof( struct inuse_header ) + sizeof( struct footer );

	if ( size < sizeof( struct free_header ) + sizeof( struct footer ) )
		size = sizeof( struct free_header ) + sizeof( struct footer );

	if ( size > free_memory )
		return NULL;

	/* find the bin to use */

	size_t bin = find_bin(size);

	if ( bin == 0 )
		return NULL;

	while ( BIN_ISEMPTY( bin ) )
		if ( ++bin >= BIN_NUMBER )
			return NULL;

	/* find the chunk to use */

	size_t chunk = find_chunk( bin, size );

	if ( chunk == BIN_ABSOLUTE_POS( bin ) ) {
		do {
			if ( ++bin >= BIN_NUMBER )
				return NULL;
		} while ( BIN_ISEMPTY( bin ) );

		chunk = BINS[bin].next_pos;
	}
	/* heuristic to improve locallity */

	if ( GET_FREE_HEADER( chunk )->size > size && last_chunk_size >= size ) {
		chunk = last_chunk;
	}

	struct free_header *header = GET_FREE_HEADER( chunk );

	/* maintain linked list */
	GET_FREE_HEADER( header->prev_pos )->next_pos = header->next_pos;
	GET_FREE_HEADER( header->next_pos )->prev_pos = header->prev_pos;

	return split( chunk, size );
}


/**
 * Frees the previously allocated space.
 *
 * @param data  a pointer to the memory to be freed
 *
 */
void free ( void* data ) {

	struct free_header* header = NULL;

	if ( data == NULL )
		return;

	size_t pos = ((char*)data) - memory - sizeof( struct inuse_header );

	//assert( pos >= MEMORY_START && pos < MEMORY_END );
	//assert( pos + GET_INUSE_HEADER(pos)->size <= MEMORY_END );
	//assert( GET_INUSE_HEADER(pos)->size ==
	//		GET_FOOTER( pos + GET_INUSE_HEADER(pos)->size - sizeof( struct footer ) )->size );
	//assert( GET_INUSE_HEADER(pos)->status == INUSE_STATUS );

	free_memory += GET_INUSE_HEADER( pos )->size;

	/* try to join with previous chunk */
	if ( pos > MEMORY_START ) {

		//assert( pos - GET_FOOTER( pos - sizeof( struct footer ) )->size >= MEMORY_START );
		//assert( pos - GET_FOOTER( pos - sizeof( struct footer ) )->size < MEMORY_END );

		header = GET_FREE_HEADER( pos - GET_FOOTER( pos - sizeof( struct footer ) )->size );

		//assert( header->size == GET_FOOTER( pos - sizeof( struct footer ) )->size );

		if ( header->status == FREE_STATUS ) {

			GET_FREE_HEADER( header->prev_pos )->next_pos = header->next_pos;
			GET_FREE_HEADER( header->next_pos )->prev_pos = header->prev_pos;

			int size = GET_INUSE_HEADER( pos )->size;
			pos -= header->size;
			header->size += size;

		} else {
			header = NULL;
		}
	}

	if ( header == NULL ) {
		header = GET_FREE_HEADER( pos );
		header->status = FREE_STATUS;
	}

	/* try to join with next chunk */
	if ( pos + header->size < MEMORY_END ) {

		struct free_header* next_header = GET_FREE_HEADER( pos + header->size );

		//assert( pos + header->size + next_header->size <= MEMORY_END );
		//assert( next_header->size ==
		//	GET_FOOTER( pos + header->size + next_header->size - sizeof( struct footer ) )->size );

		if ( next_header->status == FREE_STATUS ) {

			if ( last_chunk == pos + header->size )
				last_chunk_size = 0;

			GET_FREE_HEADER( next_header->prev_pos )->next_pos = next_header->next_pos;
			GET_FREE_HEADER( next_header->next_pos )->prev_pos = next_header->prev_pos;

			header->size += next_header->size;
		}
	}

	/* set footer */
	GET_FOOTER( pos + header->size - sizeof( struct footer ) )->size = header->size;

	/* set free */
	size_t chunk = find_upper_chunk( find_bin( header->size ), header->size );

	header->prev_pos = GET_FREE_HEADER( chunk )->prev_pos;
	header->next_pos = chunk;

	GET_FREE_HEADER( header->prev_pos )->next_pos = pos;
	GET_FREE_HEADER( header->next_pos )->prev_pos = pos;
}


void* realloc ( void* ptr, size_t new_size ) {

	if ( ptr == NULL )
		return malloc( new_size );

	size_t size = GET_INUSE_HEADER( ((char*)ptr) - memory - sizeof( struct inuse_header ) )->size;

	if ( new_size <= size )
		return ptr;

	void* new_ptr = malloc( new_size );

	memcpy( new_ptr, ptr, size );
	free( ptr );

	return new_ptr;
}


void * calloc (int size, int quant)
{
	int i;
	void * ret = malloc(size * quant);
	for(i = 0; i < size * quant; i++)
		((char *)ret)[i] = 0;

	return ret;
}
