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

void initializePaging(void);
