/********************************** 
*
*  paging.h
*  	Galindo, Jose Ignacio
*  	Homovc, Federico
*  	Loreti, Nicolas
*		ITBA 2011
*
***********************************/

#define PAGE_TABLES	1024
#define PAGES_PER_TABLE	1024
#define PAGE_SIZE	4096
#define P_DIR_START	0x08000000
#define RWUPRESENT	7
#define USER_VIRTUAL_MEM_START 0x10000000
#define TOTAL_KERNEL_MEM 0x10000000
#define TOTAL_KERNEL_PAGE_TABLES (TOTAL_KERNEL_MEM >> 22)
#define P_TABLE_START (P_DIR_START + PAGE_SIZE)
#define P_TABLE_USER_START (P_TABLE_START + TOTAL_KERNEL_PAGE_TABLES * PAGE_SIZE)

void initializePaging(void);
