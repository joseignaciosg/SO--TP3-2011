#include "../include/paging.h"

#include "../include/defs.h"


#define KERNEL_MEM 0x10000000
#define TOTAL_KERNEL_MEM 0x10000000

//Start of the physical pages in memory (slots)

#define USER_MEM_START (TOTAL_KERNEL_MEM)

//Start of the all process' virtual memory

#define USER_VIRTUAL_MEM_START (TOTAL_KERNEL_MEM)

#define PAGE_SIZE 4096

#define P_DIR_START 0x08000000

#define P_TABLE_START (P_DIR_START + PAGE_SIZE)
#define PTABLE_ENTRIES 1024

#define KERNEL_PAGE_TABLES   ( KERNEL_MEM >> 22)

#define TOTAL_KERNEL_PAGE_TABLES (TOTAL_KERNEL_MEM >> 22)

#define P_TABLE_USER_START (P_TABLE_START + TOTAL_KERNEL_PAGE_TABLES * PAGE_SIZE)

#define RWUPRESENT 7

#define MAX_PROC 128

#define RWUNPRESENT 6

#define PAGES_ON_MEM 12

#define USER_PTABLE_OFFSET ((P_TABLE_USER_START - P_TABLE_START)/4)



typedef uint32_t ptable_entry;

typedef uint32_t pdir_entry;

extern DESCR_INT idt[0x90];

extern int CurrentPID;




//Simple Vector which tells us which process page table is used or free
//0 free, 1 used

static uint32_t dirs[128] = {0};

//Vector which tells which page slots are used and by whom
// -1 unused
//on page[k] we save the offset of the entry from P_TABLE_USER_START
//which is using de kth page slot. Since all processes table are continous in memory
//when a process gets a page fault and wants the kth slot, the page table entry of the process
//which used this slot can be easily retreived (it's just (ptable_entry)P_TABLE_USER_START + pages[k])

static uint32_t pages[PAGES_ON_MEM] = {0};

//Round Robin Offset

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


#define READ_PAGE 1
#define WRITE_PAGE 0



void page_fault_handler_wrapper( int errorcode){
	printf("tetanga\n");
  
}

void clear_proc_ptable( uint32_t offset) {
  
    dirs[offset] = 0;
}

static void set_proc_ptable( uint32_t offset ) {
	/*se est� inicializando una una tabla de p�ginas (con todas
	 * sus paginas) para el usuario*/

	pdir_entry *dir = (pdir_entry *)P_DIR_START;
	
	//Get the virtual address for the process
	uint32_t mem = USER_VIRTUAL_MEM_START + PAGE_SIZE * 1024 * offset;

	dir += (mem >> 22); /*entrada del directorio de paginas que apunta a la tabla*/
	//Adress of the start of the process' page table

	ptable_entry * table = (ptable_entry  *) (P_TABLE_USER_START + offset * PAGE_SIZE);
	/*P_TABLE_USER_START es donde empiezan las tablas del usuario, es decir, partiendo de la direccion
	 * inicial , sumo 4096 para el direcotrio de paginas y luedo sumo 64 * 4096 , ya que hay 64 tablas de
	 * paginas asignadas para el kernel*/

	
	//Fill the directory entry for the process' page table
	*dir = get_dir_entry( (uint32_t) table, RWUPRESENT);
	/* se pone la tabla que se est� creando como presente en la entrada correspondiente del directorio
	 * de paginas*/

	int i;

	/*REVISAR ESTO!*/
	//Sets all page table entries as not present, not initialized
	/*la primera pagina tendria que estar presente!*/
	for( i =0; i < PTABLE_ENTRIES ; i ++ ) {
		table[i] = 0;
	}
	/*cuando se cree un proceso ademas de crear la tabla hay que crear la primera pagina y setearla como presente*/


}

uint32_t get_stack_start( uint32_t pdir_offset ) {

    return USER_VIRTUAL_MEM_START + pdir_offset * PAGE_SIZE * PTABLE_ENTRIES;
    /*le estoy pasando el comienzo de la primera pagina de la tabla asignada al proceso*/
}

int create_proc_table( void ) {
  
    int i = 0;
    for ( i = 0 ; i < MAX_PROC ; i ++ ) {
		if(dirs[i] == 0) {
			set_proc_ptable( i );
			dirs[i] = 1;
			printf("\n number of user table used %d\n",i);
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
  
      uint32_t pdir_offset = ((uint32_t) addr) >> 22;/*va desde 0 a 63, para ver donde comienza la tabla en cuestion*/
      pdir_entry *dir = (pdir_entry *)P_DIR_START + pdir_offset; /*busca la tabla con el offset anterior*/
      ptable_entry *tab = (ptable_entry * )get_dir_entry_add( *dir ); /*se queda con los 20 bits mas significativos*/
      ptable_entry *entry = tab + ((((uint32_t) addr)>> 12) & 0x3FF); /*se queda con los 10 bits menos significativos
       	   	   	   	   	   	   	   	   	   	   	   	   	   	   	   	   de addr*/
      *entry = get_table_entry( (uint32_t ) addr, RWUPRESENT ); /*setea el page frame address de la pagina, poniendola
      	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  como presente*/
}

/**
 * Creates a kernel page table, one to one mapping
 * Kernel priviligies.
 * 
 * @param addr		address which wants to be mapped
 * @param off		pagetable offset
 **/
static void create_kernel_ptable(  void* addr ) {
  
      uint32_t pdir_offset = ((uint32_t) addr) >> 22;/*va desde 0 a 63*/
      pdir_entry *dir = (pdir_entry *)P_DIR_START + pdir_offset; /*son las posiciones de las entradas del directorio de paginas*/
      *dir = get_dir_entry(P_TABLE_START + pdir_offset * PAGE_SIZE, RWUPRESENT );/*asigna donde est� el comienzo cada tablas de paginas
      	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  correspondiente a cada entrada*/
      /*se queda con los 20 bits m�s significativos del page frame address y pone las tablas de p�ginas como presentes*/

}

/**
 * Initializes kernel pages.
 * One to one mapping
 **/
void initializeKernelPages( void ){
	int i,j;
	/*las paginas de kernel si o si siempre tienen que estar presentes.
	 * Se eligieron tener 64 tablas de p�ginas para el kernel, por lo tanto
	 * las 64 primeras entradas del direcotirio de p�ginas son para tablas del
	 * kernel. En total, las paginas del kernel ocupan 64 *1024 *4096 (256 Mb),
	 * sin contar las tablas en s� mismas.*/
    for ( i = 0 ; i < TOTAL_KERNEL_PAGE_TABLES ; i ++ ) {
		void *addr = (void *)(i * PTABLE_ENTRIES * PAGE_SIZE);/*salta de 4Mb en 4Mb, deja lugar para todas las paginas de una tabla */
		create_kernel_ptable(addr);
		for(j = 0; j < PTABLE_ENTRIES ; j ++) {
			create_kernel_page((void*)((uint32_t)addr + j * PAGE_SIZE));
			/*para cada una de las tablas de p�ginas va saltando de 4Kb en 4Kb
			 * inicializando las p�ginas*/
		}
    }
    printf("leaving initializeKernelPages\n");
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

void init_paging( void ) {
  
	int i;
	initializeKernelPages();

    //register_interrupt_handler ( 14, page_fault_handler_wrapper );
	printf("after initializeKernelPages\n");
	setup_IDT_entry(&idt[0x0E], 0x08, (dword) &page_fault_handler_wrapper, ACS_INT, 0);/*for block_process and kill*/

    for(i = 0 ; i < PAGES_ON_MEM ; i ++ ) {
		pages[i] = -1;
    }
    //enable_paging();
}

int LoadAuxStack()
{
	/*
	 * Se cambia al stack auxiliar (con LoadAuxStack)
	 *
	 */

	/*STACK_FRAME * frame = (STACK_FRAME*) (bottom - sizeof(STACK_FRAME));
	frame->EBP = 0;
	frame->EIP = (int) process;
	frame->CS = 0x08;
	frame->EFLAGS = 0;
	frame->retaddr = cleaner;
	frame->argc = argc;
	frame->argv = argv;
	return (int) frame;
	return LoadStackFrame(
			(int(*)(int, char**)) shell,
			param->argc,
			param->argv,
			(uint32_t) ((char *) proc->stackstart + proc->stacksize), end_process);
			*/

	return 0x06000000;/* 96Mb */

}

void HopOffPages() {
	/*
	 * Se bajan todas la p�ginas del proceso que se estaba ejecutando.
	 */

	/*char * newStack = (char*) krealloc(actual->pid);
	unsigned int offset = (unsigned int) actual->stack - actual->esp;
	actual->esp = (unsigned int) newStack - offset;
	actual->stack = newStack;*/

	PROCESS * p = (PROCESS *)GetProcessByPID(CurrentPID);
	//int currpage = get_stack_start(p->pdir);

	int table_num = p->pdir/1024;
	int table_offset = p->pdir%1024;
	int table_frame = PAGE_DIR + PAGE_SIZE + PAGE_SIZE*table_num +table_offset;
	table_frame = table_frame & 0xFFF0;

}

PROCESS* TakeUpPages(PROCESS* p){
	/**
	 * Se suben todas las p�ginas del proceso que se va a ejecutar.
	 */
	int table_num = p->pdir/1024;
	int table_offset = p->pdir%1024;
	int table_frame = PAGE_DIR + PAGE_SIZE + PAGE_SIZE*table_num +table_offset;
	table_frame |= 7;

	return p;
}


