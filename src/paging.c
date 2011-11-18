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

typedef unsigned int  size_t;
typedef unsigned int        uint32_t;
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
	for(i = 0; i < PAGE_TABLES; i++) 
	{
		void * addr = (void *)(i * PAGES_PER_TABLE * PAGE_SIZE);
		create_table(addr);
		for(j = 0; j < PAGES_PER_TABLE; j++)
			create_page( (void*)((uint32_t)addr + j * PAGE_SIZE) );
	}

	return;
}


static void create_table(void * addr)
{
  
	uint32_t pdir_offset = ((uint32_t) addr) >> 22;
	pdir_entry * dir = (pdir_entry *)P_DIR_START + pdir_offset;
	* dir = get_dir_entry(P_DIR_START + PAGE_SIZE + pdir_offset * PAGE_SIZE, RWUPRESENT );

	return;
}


static void create_page(void * addr)
{ 
	uint32_t pdir_offset = ((uint32_t) addr) >> 22;
	pdir_entry *dir = (pdir_entry *)P_DIR_START + pdir_offset;
	ptable_entry *tab = (ptable_entry * )get_dir_entry_add( *dir );
	ptable_entry *entry = tab + ((((uint32_t) addr)>> 12) & 0x3FF);
	* entry = get_table_entry( (uint32_t ) addr, RWUPRESENT );

	return;
}


static ptable_entry get_dir_entry(int address, int perms)
{
	ptable_entry ret = address & 0xFFFFF000;
	ret |= perms;
	return ret;
}


static ptable_entry get_table_entry(int address, int perms)
{  
	ptable_entry ret = address & 0xFFFFF000;
	ret |= perms;
	return ret;
}

inline static uint32_t get_dir_entry_add(int entry)
{
	return entry & 0xFFFFF000;
}

int create_proc_table( void ) {

    int i;
    for ( i = 0 ; i < 1024 * 1024; i ++ ) {
		if(dirs[i] == 0) {
			set_proc_ptable( i );
			dirs[i] = 1;
			return i;
		}
	}
    return -1;
}

uint32_t get_stack_start( uint32_t pdir_offset ) {
  
    return USER_VIRTUAL_MEM_START + pdir_offset * PAGE_SIZE * PAGES_PER_TABLE;
}

static void set_proc_ptable( uint32_t offset ) {

	pdir_entry *dir = (pdir_entry *)P_DIR_START;
	
	//Get the virtual address for the process
	uint32_t mem = USER_VIRTUAL_MEM_START + PAGE_SIZE * 1024 * offset;
	dir += (mem >> 22);
	//Adress of the start of the process' page table
	ptable_entry * table = (ptable_entry  *) (P_TABLE_USER_START + offset * PAGE_SIZE);
	
	//Fill the directory entry for the process' page table
	*dir = get_dir_entry( (uint32_t) table, RWUPRESENT);
	int i = 0;

	//Sets all page table entries as not present, not initialized
	for( i = 0 ; i < PAGES_PER_TABLE ; i ++ ) {
		table[i] = 0;
	}
}

