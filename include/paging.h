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

#define PAGE_TABLES	10
#define PAGES_PER_TABLE	1024
#define PAGE_SIZE	4096
#define PAGE_DIR	0x08000000
#define PAGE_USER_START	(PAGE_DIR + PAGE_SIZE + PAGE_TABLES * PAGE_SIZE)

void initializePaging(void);

int create_proc_table(void);

uint32_t get_stack_start(uint32_t pdir_offset);
