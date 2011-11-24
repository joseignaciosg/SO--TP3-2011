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

extern DESCR_INT idt[0x90];

//typedef unsigned int  size_t;
//typedef unsigned int        uint32_t;
typedef uint32_t ptable_entry;
typedef uint32_t pdir_entry;
struct int_params  {
	//struct segments  segs;
	//struct registers regs;
	uint32_t         int_no;
	uint32_t         err_code;
	uint32_t         eip;
	uint32_t         cs;
	uint32_t         eflags;
	uint32_t         useresp;
	uint32_t         ss;
};

static void create_kernel_ptable(void * addr);
static void create_kernel_page(void * addr);
static ptable_entry get_dir_entry(uint32_t address, uint32_t perms);
static ptable_entry get_table_entry(uint32_t address, uint32_t perms);
inline static uint32_t get_dir_entry_add(pdir_entry entry);
static void set_proc_ptable( uint32_t offset );
void page_fault_handler( uint32_t errcode, uint32_t address);
static void enable_paging( void );
void page_fault_handler_wrapper( struct int_params* params);


static uint32_t dirs[128] = {0};

static uint32_t pages[PAGES_ON_MEM] = {0};

static uint32_t p_off = 0;

static pdir_entry get_dir_entry(  uint32_t address, uint32_t perms){
  
     ptable_entry ret = address & 0xFFFFF000;
     ret |= perms;
     return ret;
}

static ptable_entry get_table_entry(  uint32_t address, uint32_t perms){
  
     pdir_entry ret = address & 0xFFFFF000;
     ret |= perms;
     return ret;
}

inline static uint32_t get_dir_entry_add( pdir_entry entry){
  
    return entry & 0xFFFFF000;
}

inline static uint32_t get_table_entry_add( ptable_entry entry){
  
    return entry & 0xFFFFF000;
}

/**
 * Constructs a virtual address associated with a certain page_table_entry
 * and returns it aligned to page.
 * @param ptable_offset 	offset of said page_table_entry from USER_PTABLE_START
 * 
 * @return 			virtual memory aligned to page
 **/

static uint32_t construct_address(uint32_t ptable_offset){
  
    uint32_t val  = ptable_offset + USER_PTABLE_OFFSET;
    uint32_t top = val >> 10;
    uint32_t middle = val & 0x000003FF;
    return (top << 22) + (middle << 12);
}

#define READ_PAGE true
#define WRITE_PAGE false

void page_fault_handler( uint32_t errcode, uint32_t address) {

	pdir_entry * dir = ((pdir_entry *)P_DIR_START) + (address >> 22);
	uint32_t ptable_off = address >> 12;
	ptable_off = ptable_off & 0x000003FF;

	
	ptable_entry *ent = (ptable_entry*)get_dir_entry_add(*dir);
	ent += ptable_off;

	if ((*dir & 0x1) == 0) {
		//kernel panic... page tables are always in memory!!
		while (1);
	}
	uint32_t add = USER_VIRTUAL_MEM_START + PAGE_SIZE * p_off;

	if (*ent == 0){

		//page_down(pages[p_off]);
		*ent = get_table_entry( add, RWUPRESENT);

		// not necessary. However the page is another process' RAM, and
		// processes should have other process' looking at their ram
		//memset((void *)(address&(~0xfff)), 0, PAGE_SIZE);
	} /*else {

		//search in disk for initialized page
		page_down(pages[p_off]);
		page_up( p_off, ent );
	}*/
	
	//Round Robin
	
	uint32_t off = ent - (ptable_entry *) P_TABLE_USER_START;
	pages[p_off] = off; 
	p_off++;
	if (p_off == PAGES_ON_MEM) {
		p_off = 0;
	}

	return;
}


void page_fault_handler_wrapper( struct int_params* params){
	
	uint32_t address = 0;
	__asm__ volatile("MOVL 	%%CR2, %0" : "=r" (address) : );
	printf("err_code:%d DIR:%d TABLE:%d\n", params->err_code & 0xF, address >> 22, (address >> 12) & 1023);
	page_fault_handler(params->err_code, address);
  
}

void clear_proc_ptable( uint32_t offset) {
  
    dirs[offset] = 0;
}

static void set_proc_ptable( uint32_t offset ) {

	pdir_entry *dir = (pdir_entry *)P_DIR_START;
	
	//Get the virtual address for the process
	uint32_t mem = USER_VIRTUAL_MEM_START + PAGE_SIZE * 1024 * offset;
	dir += (mem >> 22);
	//Adress of the start of the process' page table
	ptable_entry * table = (ptable_entry*) (P_TABLE_USER_START + offset * PAGE_SIZE);
	
	//Fill the directory entry for the process' page table
	*dir = get_dir_entry( (uint32_t) table, RWUPRESENT);
	int i;

	//Sets all page table entries as not present, not initialized
	for( i = 1 ; i < PTABLE_ENTRIES ; i ++ ) {
		table[i] = 0;
	}
}

uint32_t get_stack_start( uint32_t pdir_offset ) {

	//printf("USER_VIRTUAL_MEM_START:%d pdir_offset:%d PAGE_SIZE:%d PTABLE_ENTRIES:%d\n", USER_VIRTUAL_MEM_START, pdir_offset, PAGE_SIZE, PTABLE_ENTRIES);
	return USER_VIRTUAL_MEM_START + pdir_offset * PAGE_SIZE * PTABLE_ENTRIES;
}

uint32_t create_proc_ptable( void ) {
  
    int i = 0;
    for ( i = 0 ; i < MAX_PROC ; i ++ ) {
		if(dirs[i] == 0) {
			set_proc_ptable( i );
			dirs[i] = 1;
			return i;
		}
	}
    return -1;
}

/**
 * Creates a kernel page, one to one mapping.
 * Kernel priviligies.
 * Assumes corresponding page table is created
 * 
 * @param addr		address which wants to be mapped
 **/
static void create_kernel_page( void* addr) {
  
      uint32_t pdir_offset = ((uint32_t) addr) >> 22;
      pdir_entry *dir = (pdir_entry *)P_DIR_START + pdir_offset;
      ptable_entry *tab = (ptable_entry * )get_dir_entry_add( *dir );
      ptable_entry *entry = tab + ((((uint32_t) addr)>> 12) & 0x3FF);
      *entry = get_table_entry( (uint32_t ) addr, RWUPRESENT );
}

/**
 * Creates a kernel page table, one to one mapping
 * Kernel priviligies.
 * 
 * @param addr		address which wants to be mapped
 * @param off		pagetable offset
 **/
static void create_kernel_ptable(  void* addr ) {
  
      uint32_t pdir_offset = ((uint32_t) addr) >> 22;
      pdir_entry *dir = (pdir_entry *)P_DIR_START + pdir_offset;
      *dir = get_dir_entry(P_TABLE_START + pdir_offset * PAGE_SIZE, RWUPRESENT );
}

void initializePaging(void)
{
	int i, j;
	for (i = 0; i < /*TOTAL_KERNEL_PAGE_TABLES*/ 1024; i++ )
	{
		void * addr = (void *)(i * PTABLE_ENTRIES * PAGE_SIZE);
		create_kernel_ptable(addr);
		for( j = 0; j < PTABLE_ENTRIES; j++)
		{
			create_kernel_page((void*)((uint32_t)addr + j * PAGE_SIZE));
		}
	}

	setup_IDT_entry (&idt[0x0E], 0x08, (dword)&page_fault_handler_wrapper, ACS_INT, 0);
	
	for(i = 0; i < PAGES_ON_MEM; i++)
		pages[i] = -1;

	//enable_paging();

	return;
}

/**
 * Enables hardware paging.
 * Kernel pages MUST have been initialized beforehand
 **/
static void enable_paging( void ) {
  
	uint32_t directory = P_DIR_START;
      
	//Set the page_directory address in CR3
	__asm__ volatile("MOVL 	%0, %%CR3" : : "b" (directory) : "%eax");
	__asm__ volatile("MOVL	%CR0, %EAX"); 			// Get the value of CR0.
	__asm__ volatile("OR	$0x80000000, %EAX");	// Set PG bit.
	__asm__ volatile("MOVL 	%EAX, %CR0");			// Set CR0 value.
}
