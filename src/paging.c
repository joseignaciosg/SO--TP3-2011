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
#include "../include/kernel.h"
#include "../include/stdio.h"

extern DESCR_INT idt[0x90];
extern int nextPID;
extern int CurrentPID;
extern int FirstTime;

typedef uint32_t ptable_entry;
typedef uint32_t pdir_entry;
struct int_params {
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
void page_fault_handler_wrapper(struct int_params* params);

static uint32_t dirs[MAX_PROC] = { 0 };

static pdir_entry get_dir_entry(uint32_t address, uint32_t perms) {

	ptable_entry ret = address & 0xFFFFF000;
	ret |= perms;
	return ret;
}

static ptable_entry get_table_entry(uint32_t address, uint32_t perms, int flag) {

	pdir_entry ret = address & 0xFFFFF000;
	ret |= perms;
	if (flag) {
		ret |= 512; //octavo bit en 1
		//printf("\ntable entry %d\n", (ret >> 9) & 0x1);/*print bit 8*/
		//printf("ret: %d\n", ret);
	}
	return ret;
}

inline static uint32_t get_dir_entry_add(pdir_entry entry) {

	return entry & 0xFFFFF000;
}

inline static uint32_t get_table_entry_add(ptable_entry entry) {

	return entry & 0xFFFFF000;
}

void page_fault_handler_wrapper(struct int_params* params) {

	uint32_t address = 0;
	__asm__ volatile("MOVL 	%%CR2, %0" : "=r" (address) : );
	printf("err_code:%d TABLE:%d PAGE:%d\n", params->err_code & 0xF,
			address >> 22, (address >> 12) & 1023);
}

void clear_proc_ptable(uint32_t offset) {

	int i;
	for(i = 0; i < MAX_PROC; i++)
		if(dirs[i] == offset)
			dirs[i] = 0;
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
	/*se est� inicializando una una tabla de p�ginas (con todas
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
	/* se pone la tabla que se est� creando como presente en la entrada correspondiente del directorio
	 * de paginas*/

	int j;

	void *addr = (void *) ((offset + 64) * PTABLE_ENTRIES * PAGE_SIZE);
	create_user_page((void*) ((uint32_t) addr + 0 ), RWUPRESENT,1);

	for (j = 1; j < PTABLE_ENTRIES; j++) {
		create_user_page(
				(void*) ((uint32_t) addr + j * PAGE_SIZE), RWUNPRESENT,0);
	}

}

uint32_t get_stack_start(uint32_t pdir_offset) {

	return USER_VIRTUAL_MEM_START + pdir_offset * PAGE_SIZE * PTABLE_ENTRIES;
}

uint32_t create_proc_ptable(void) {

	int i = 0;
	for (i = 0; i < MAX_PROC; i++) {
		if (dirs[i] == 0) {
			set_proc_ptable(i);
			dirs[i] = nextPID - 1;
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
	*dir = get_dir_entry(P_TABLE_START + pdir_offset * PAGE_SIZE, RWUPRESENT);/*asigna donde est� el comienzo cada tablas de paginas
	 correspondiente a cada entrada*/
	/*se queda con los 20 bits m�s significativos del page frame address y pone las tablas de p�ginas como presentes*/

}

void initializePaging(void) {

	int i, j;
	/*las paginas de kernel si o si siempre tienen que estar presentes.
	 * Se eligieron tener 64 tablas de p�ginas para el kernel, por lo tanto
	 * las 64 primeras entradas del directorio de p�ginas son para tablas del
	 * kernel. En total, las paginas del kernel ocupan 64 *1024 *4096 (256 Mb),
	 * sin contar las tablas en s� mismas.*/
	for (i = 0; i < TOTAL_KERNEL_PAGE_TABLES; i++) {
		void *addr = (void *) (i * PTABLE_ENTRIES * PAGE_SIZE);/*salta de 4Mb en 4Mb, deja lugar para todas las paginas de una tabla */
		create_kernel_ptable(addr);
		for (j = 0; j < PTABLE_ENTRIES; j++) {
			create_kernel_page((void*) ((uint32_t) addr + j * PAGE_SIZE));
			/*para cada una de las tablas de p�ginas va saltando de 4Kb en 4Kb
			 * inicializando las p�ginas*/
		}
	}

	setup_IDT_entry(&idt[0x0E], 0x08, (dword) &page_fault_handler_wrapper,
			ACS_INT, 0);

	return;
}

int LoadAuxStack() {
	/*
	 * Se cambia al stack auxiliar (con LoadAuxStack)
	 *
	 */

	return 0x06000000;/* 96Mb */

}


void HopOffPages() {

	if(FirstTime)
	{
		FirstTime = 0;
		return;
	}
	
	PROCESS * p = GetProcessByPID(CurrentPID);
	int j, flag = 1;
	void * addr = (void *) ( (p->pdir + 64) * PTABLE_ENTRIES * PAGE_SIZE);
	for (j = 0; j < PTABLE_ENTRIES && flag; j++)
	{
		addr += j * PAGE_SIZE;
		uint32_t pdir_offset = ((uint32_t) addr) >> 22;
		pdir_entry *dir = (pdir_entry *) P_DIR_START + pdir_offset;
		ptable_entry *tab = (ptable_entry *) get_dir_entry_add(*dir);
		ptable_entry *entry = tab + ((((uint32_t) addr) >> 12) & 0x3FF);
		flag = (*entry) & 512;
		if(flag)
			(*entry) = (*entry) & 0xFFFFFFFE;
	}

}


void TakeUpPages() {

	PROCESS * p = GetProcessByPID(CurrentPID);
	int j, flag = 1;
	void * addr = (void *) ( (p->pdir + 64) * PTABLE_ENTRIES * PAGE_SIZE);
	for (j = 0; j < PTABLE_ENTRIES && flag; j++)
	{
		addr += j * PAGE_SIZE;
		uint32_t pdir_offset = ((uint32_t) addr) >> 22;
		pdir_entry *dir = (pdir_entry *) P_DIR_START + pdir_offset;
		ptable_entry *tab = (ptable_entry *) get_dir_entry_add(*dir);
		ptable_entry *entry = tab + ((((uint32_t) addr) >> 12) & 0x3FF);
		flag = (*entry) & 512;
		if(flag)
			(*entry) = (*entry) | 1;
	}
}

