/********************************** 
*
*  paging.c
*  	Galindo, Jose Ignacio
*  	Homovc, Federico
*  	Loreti, Nicolas
*		ITBA 2011
*
***********************************/

#include "../include/paging.h"
#include "../include/defs.h"

//typedef unsigned int  size_t;
//typedef unsigned int        uint32_t;
typedef uint32_t ptable_entry;
typedef uint32_t pdir_entry;

static void create_table(void * addr);
static void create_page(void * addr);
static ptable_entry get_dir_entry(int address, int perms);
static ptable_entry get_table_entry(int address, int perms);
inline static uint32_t get_dir_entry_add(int entry);
static void set_proc_ptable( uint32_t offset );

static uint32_t dirs[1024 * 1024] = {0};

void initializePaging(void)
{
	int i, j;
	unsigned * table, * dir;
	for(i = 0; i < PAGE_TABLES; i++) 
	{
		dir = (unsigned *)PAGE_DIR + i;
		* dir = PAGE_DIR + PAGE_SIZE * i + 1;
		table = (unsigned *)PAGE_DIR + i;
		for(j = 0; j < PAGES_PER_TABLE; j++)
			*(table + j) = PAGE_USER_START + j * PAGE_SIZE;
	}

	return;
}

int create_proc_table( void ) {

	int i;
	for (i = 0; i < 1024 * 1024; i++){
		if(dirs[i] == 0) {
			set_proc_ptable(i);
			dirs[i] = 1;
			return i;
		}
	}
	return -1;
}

uint32_t get_stack_start(uint32_t offset){

	return PAGE_USER_START + offset * PAGE_SIZE;
}

static void set_proc_ptable(uint32_t offset) {

	int i;
	unsigned * table = (unsigned *) (PAGE_USER_START + offset * PAGE_SIZE);
	
	for(i = 0; i < PAGE_SIZE; i++) {
		table[i] = 0;
	}
}

