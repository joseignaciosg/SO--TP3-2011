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

