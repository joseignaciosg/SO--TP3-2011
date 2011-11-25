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
extern int CurrentPID;

//typedef unsigned int  size_t;
//typedef unsigned int        uint32_t;
typedef uint32_t ptable_entry;
typedef uint32_t pdir_entry;
struct int_params {
	//struct segments  segs;
	//struct registers regs;
	uint32_t int_no;
	uint32_t err_code;
	uint32_t eip;
	uint32_t cs;
	uint32_t eflags;
	uint32_t useresp;
	uint32_t ss;
};

static void create_kernel_ptable(void * addr);
static void create_kernel_page(void * addr);
static ptable_entry get_dir_entry(uint32_t address, uint32_t perms);
static ptable_entry get_table_entry(uint32_t address, uint32_t perms, int flag);
inline static uint32_t get_dir_entry_add(pdir_entry entry);
static void set_proc_ptable(uint32_t offset);
void page_fault_handler(uint32_t errcode, uint32_t address);
static void enable_paging(void);
void page_fault_handler_wrapper(struct int_params* params);

static uint32_t dirs[128] = { 0 };

static uint32_t pages[PAGES_ON_MEM] = { 0 };

static uint32_t p_off = 0;

static pdir_entry get_dir_entry(uint32_t address, uint32_t perms) {

	ptable_entry ret = address & 0xFFFFF000;
	ret |= perms;
	return ret;
}

static ptable_entry get_table_entry(uint32_t address, uint32_t perms, int flag) {

	pdir_entry ret = address & 0xFFFFF000;
	ret |= perms;
	/*proof*/
	if (flag) {
		ret |= 512; //octavo bit en 1
		printf("\ntable entry %d\n", (ret >> 9) & 0x1);/*print bit 8*/
		printf("ret: %d\n",ret);

	}
	return ret;
}

inline static uint32_t get_dir_entry_add(pdir_entry entry) {

	return entry & 0xFFFFF000;
}

inline static uint32_t get_table_entry_add(ptable_entry entry) {

	return entry & 0xFFFFF000;
}

/**
 * Constructs a virtual address associated with a certain page_table_entry
 * and returns it aligned to page.
 * @param ptable_offset 	offset of said page_table_entry from USER_PTABLE_START
 * 
 * @return 			virtual memory aligned to page
 **/

static uint32_t construct_address(uint32_t ptable_offset) {

	uint32_t val = ptable_offset + USER_PTABLE_OFFSET;
	uint32_t top = val >> 10;
	uint32_t middle = val & 0x000003FF;
	return (top << 22) + (middle << 12);
}

#define READ_PAGE true
#define WRITE_PAGE false

void page_fault_handler_wrapper(struct int_params* params) {

	uint32_t address = 0;
	__asm__ volatile("MOVL 	%%CR2, %0" : "=r" (address) : );
	printf("err_code:%d TABLE:%d PAGE:%d\n", params->err_code & 0xF,
			address >> 22, (address >> 12) & 1023);
	//page_fault_handler(params->err_code, address);

}

void clear_proc_ptable(uint32_t offset) {

	dirs[offset] = 0;
}

static void create_user_page(void* addr, int perms, int flag) {

	uint32_t pdir_offset = ((uint32_t) addr) >> 22;/*va desde 0 a 63, para ver donde comienza la tabla en cuestion*/
	pdir_entry *dir = (pdir_entry *) P_DIR_START + pdir_offset; /*busca la tabla con el offset anterior*/
	ptable_entry *tab = (ptable_entry *) get_dir_entry_add(*dir); /*se queda con los 20 bits mas significativos*/
	ptable_entry *entry = tab + ((((uint32_t) addr) >> 12) & 0x3FF); /*se queda con los 10 bits menos significativos
	 de addr*/
	*entry = get_table_entry((uint32_t) addr, perms, flag); /*setea el page frame address de la pagina, poniendola
	 como presente*/
}

static void set_proc_ptable(uint32_t offset) {
	/*se est‡ inicializando una una tabla de p‡ginas (con todas
	 * sus paginas) para el usuario*/

	pdir_entry *dir = (pdir_entry *) P_DIR_START;

	//Get the virtual address for the process
	uint32_t mem = USER_VIRTUAL_MEM_START + PAGE_SIZE * 1024 * offset;

	dir += (mem >> 22); /*entrada del directorio de paginas que apunta a la tabla*/
	//Adress of the start of the process' page table

	ptable_entry * table = (ptable_entry *) (P_TABLE_USER_START
			+ offset * PAGE_SIZE);
	/*P_TABLE_USER_START es donde empiezan las tablas del usuario, es decir, partiendo de la direccion
	 * inicial , sumo 4096 para el direcotrio de paginas y luedo sumo 64 * 4096 , ya que hay 64 tablas de
	 * paginas asignadas para el kernel*/

	//Fill the directory entry for the process' page table
	*dir = get_dir_entry((uint32_t) table, RWUPRESENT);
	/* se pone la tabla que se est‡ creando como presente en la entrada correspondiente del directorio
	 * de paginas*/

	int j;

	void *addr = (void *) ((offset + 64) * PTABLE_ENTRIES * PAGE_SIZE);
	create_user_page((void*) ((uint32_t) addr + 0 ), RWUPRESENT,1);

	for (j = 1; j < PTABLE_ENTRIES; j++) {
		create_user_page(
				(void*) ((uint32_t) addr + j * PAGE_SIZE), RWUNPRESENT,0);
	}

	/*REVISAR ESTO!*/
	//Sets all page table entries as not present, not initialized
	/*la primera pagina tendria que estar presente!*/
	/*for( i = 0; i < PTABLE_ENTRIES ; i ++ ) {
	 table[i] = 0;
	 }*/
	/*cuando se cree un proceso ademas de crear la tabla hay que crear la primera pagina y setearla como presente*/

}

uint32_t get_stack_start(uint32_t pdir_offset) {

	return USER_VIRTUAL_MEM_START + pdir_offset * PAGE_SIZE * PTABLE_ENTRIES;
}

uint32_t create_proc_ptable(void) {

	int i = 0;
	for (i = 0; i < MAX_PROC; i++) {
		if (dirs[i] == 0) {
			set_proc_ptable(i);
			dirs[i] = 1;
			printf(" number of user table used %d\n", i);
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
static void create_kernel_page(void* addr) {

	uint32_t pdir_offset = ((uint32_t) addr) >> 22;/*va desde 0 a 63, para ver donde comienza la tabla en cuestion*/
	pdir_entry *dir = (pdir_entry *) P_DIR_START + pdir_offset; /*busca la tabla con el offset anterior*/
	ptable_entry *tab = (ptable_entry *) get_dir_entry_add(*dir); /*se queda con los 20 bits mas significativos*/
	ptable_entry *entry = tab + ((((uint32_t) addr) >> 12) & 0x3FF); /*se queda con los 10 bits menos significativos
	 de addr*/
	*entry = get_table_entry((uint32_t) addr, RWUPRESENT, 0); /*setea el page frame address de la pagina, poniendola
	 como presente*/
}

/**
 * Creates a kernel page table, one to one mapping
 * Kernel priviligies.
 * 
 * @param addr		address which wants to be mapped
 * @param off		pagetable offset
 **/
static void create_kernel_ptable(void* addr) {

	uint32_t pdir_offset = ((uint32_t) addr) >> 22;/*va desde 0 a 63*/
	pdir_entry *dir = (pdir_entry *) P_DIR_START + pdir_offset; /*son las posiciones de las entradas del directorio de paginas*/
	*dir = get_dir_entry(P_TABLE_START + pdir_offset * PAGE_SIZE, RWUPRESENT);/*asigna donde est‡ el comienzo cada tablas de paginas
	 correspondiente a cada entrada*/
	/*se queda con los 20 bits m‡s significativos del page frame address y pone las tablas de p‡ginas como presentes*/

}

void initializePaging(void) {

	int i, j;
	/*las paginas de kernel si o si siempre tienen que estar presentes.
	 * Se eligieron tener 64 tablas de p‡ginas para el kernel, por lo tanto
	 * las 64 primeras entradas del directorio de p‡ginas son para tablas del
	 * kernel. En total, las paginas del kernel ocupan 64 *1024 *4096 (256 Mb),
	 * sin contar las tablas en s’ mismas.*/
	for (i = 0; i < TOTAL_KERNEL_PAGE_TABLES; i++) {
		void *addr = (void *) (i * PTABLE_ENTRIES * PAGE_SIZE);/*salta de 4Mb en 4Mb, deja lugar para todas las paginas de una tabla */
		create_kernel_ptable(addr);
		for (j = 0; j < PTABLE_ENTRIES; j++) {
			create_kernel_page((void*) ((uint32_t) addr + j * PAGE_SIZE));
			/*para cada una de las tablas de p‡ginas va saltando de 4Kb en 4Kb
			 * inicializando las p‡ginas*/
		}
	}

	setup_IDT_entry(&idt[0x0E], 0x08, (dword) &page_fault_handler_wrapper,
			ACS_INT, 0);

	for (i = 0; i < PAGES_ON_MEM; i++)
		pages[i] = -1;

	return;
}

int LoadAuxStack() {
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
	 * Se bajan todas la p‡ginas del proceso que se estaba ejecutando.
	 */

	/*char * newStack = (char*) krealloc(actual->pid);
	 unsigned int offset = (unsigned int) actual->stack - actual->esp;
	 actual->esp = (unsigned int) newStack - offset;
	 actual->stack = newStack;*/


	int page_down_attemp(int * addr){
		printf("\n addr %d:",(int)(*addr));
		return ((unsigned)addr >> 9) & 0x1;
	}

	void page_down(void * addr){
		unsigned ret = (unsigned)addr & 0xFFFFFFFE;
		printf("hop off :%d",ret);
		addr = (void*)ret;
	}

	PROCESS * p = (PROCESS *) GetProcessByPID(CurrentPID);
	//int currpage = get_stack_start(p->pdir);
	//printf("pdir %d\n", p->pdir);
	void *addr = (void *) (P_DIR_START + PAGE_SIZE + (p->pdir + 64) * PAGE_SIZE);
	int flag = 1;
	int j;
	for (j = 0; (j < PTABLE_ENTRIES) && flag; j++) {
		//printf("addr %d\n", addr + j);
		flag = page_down_attemp((void*) ((uint32_t) addr + j ));
		if (flag) {
			page_down((void*) (uint32_t) addr + j);
		}
	}

}

PROCESS* TakeUpPages(PROCESS* p) {
	/**
	 * Se suben todas las p‡ginas del proceso que se va a ejecutar.
	 */
	/*int table_num = p->pdir/1024;
	 int table_offset = p->pdir%1024;
	 int table_frame = PAGE_DIR + PAGE_SIZE + PAGE_SIZE*table_num +table_offset;
	 table_frame |= 7;*/

	return p;
}

