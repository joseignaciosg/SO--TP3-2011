/********************************** 
*
*  paging.h
*  	Galindo, Jose Ignacio
*  	Homovc, Federico
*  	Loreti, Nicolas
*		ITBA 2011
*
***********************************/

#include "../include/defs.h"

#define KERNEL_MEM 0x10000000 //256 mega
#define TOTAL_KERNEL_MEM 0x10000000

//Start of the physical pages in memory (slots)

#define USER_MEM_START (TOTAL_KERNEL_MEM)

//Start of the all process' virtual memory

#define USER_VIRTUAL_MEM_START (TOTAL_KERNEL_MEM)

#define PAGE_SIZE 4096

#define P_DIR_START 0x08000000

#define P_TABLE_START (P_DIR_START + PAGE_SIZE)
#define PTABLE_ENTRIES 1024

#define KERNEL_PAGE_TABLES   ( KERNEL_MEM >> 22) //64

#define TOTAL_KERNEL_PAGE_TABLES (TOTAL_KERNEL_MEM >> 22)

#define P_TABLE_USER_START (P_TABLE_START + TOTAL_KERNEL_PAGE_TABLES * PAGE_SIZE)

#define RWUPRESENT 7

#define MAX_PROC 128

#define RWUNPRESENT 6

#define PAGES_ON_MEM 12

#define USER_PTABLE_OFFSET ((P_TABLE_USER_START - P_TABLE_START)/4)

void initializePaging(void);

int create_proc_ptable(void);

int get_stack_start(uint32_t pdir_offset);
