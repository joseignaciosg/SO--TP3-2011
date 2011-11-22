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

//typedef unsigned int  size_t;
//typedef unsigned int        uint32_t;
typedef uint32_t ptable_entry;
typedef uint32_t pdir_entry;

extern int nextPID;
extern int CurrentPID;


static void create_table(void * addr);
static void create_page(void * addr);
static ptable_entry get_dir_entry(int address, int perms);
static ptable_entry get_table_entry(int address, int perms);
inline static uint32_t get_dir_entry_add(int entry);
static void set_proc_ptable(uint32_t offset);

static uint32_t dirs[1024 * 1024] = {0};

void initializePaging(void) {
	int i, j;
	unsigned * table, *dir;
	for (i = 0; i < PAGE_TABLES; i++) {
		dir = (unsigned *) PAGE_DIR + i;
		*dir = PAGE_DIR + PAGE_SIZE * i + 1;
		/*assigns directories entries*/
		//table = (unsigned *) PAGE_DIR + i;
		table = (unsigned *)PAGE_DIR + PAGE_SIZE * (i + 1);
		for (j = 0; j < PAGES_PER_TABLE; j++)
			/*assigns tables entries*/
			*(table + j) = PAGE_USER_START + j * PAGE_SIZE;
	}

	return;
}

int create_proc_table(void) {

	int i;
	for (i = 0; i < 1024 * 1024; i++) {
		if (dirs[i] == 0) {
			set_proc_ptable(i);
			//dirs[i] = 1;
			dirs[i] = nextPID+1;
			return i;
		}
	}
	return -1;
}

uint32_t get_stack_start(uint32_t offset) {

	return PAGE_USER_START + offset * PAGE_SIZE;
}

static void set_proc_ptable(uint32_t offset) {

	int i;
	unsigned * page = (unsigned *) (PAGE_USER_START + offset * PAGE_SIZE);

	for (i = 0; i < PAGE_SIZE; i++) {
		page[i] = 0;
	}
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
	 * Se bajan todas la p‡ginas del proceso que se estaba ejecutando.
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
	 * Se suben todas las p‡ginas del proceso que se va a ejecutar.
	 */
	int table_num = p->pdir/1024;
	int table_offset = p->pdir%1024;
	int table_frame = PAGE_DIR + PAGE_SIZE + PAGE_SIZE*table_num +table_offset;
	table_frame |= 7;

	return p;
}

