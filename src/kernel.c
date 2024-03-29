/********************************** 
*
*  kernel.c
*  	Galindo, Jose Ignacio
*  	Homovc, Federico
*  	Loreti, Nicolas
*		ITBA 2011
*
***********************************/


/***	Project Includes	***/
#include "../include/kasm.h"
#include "../include/kernel.h"
#include "../include/shell.h"
#include "../include/utils.h"
#include "../include/fs.h"
#include "../include/stdio.h"
#include "../include/pisix.h"
#include "../include/paging.h"
#include "../include/malloc.h"
#include "../include/video.h"
#include "../include/cache.h"


DESCR_INT idt[0x85]; /* IDT 144 positions*/
IDTR idtr;			 /* IDTR */

int nextPID = 1;
int CurrentPID = 0;
int currentTTY = 0;
int currentProcessTTY = 0;
int logPID;
int usrID = 1;
int semCount;
int fifoCount;
PROCESS idle;
processList ready;
TTY terminals[4];
user currentUsr;
semItem * semaphoreTable;
my_fdItem * fifo_table;

extern int timeslot;
extern int logoutPID;
extern int actualKilled;
extern int last100[100];
extern int usrLoged;
extern int usrName;
extern int password;



/*for testing fifos*/
#define MAX_FIZE_SIZE 1000
#define MAX_FIFO 100

uint32_t LoadStackFrame(int(*process)(int,char**),int argc,char** argv, uint32_t bottom, void(*cleaner)());

void
initializeSemaphoreTable(){
	semaphoreTable = malloc(sizeof(semItem)*20); /* UP TO 20 semaphores */
	fifo_table = malloc(sizeof(my_fdItem)*MAX_FIFO); /* UP TO MAX_FIFO tables */
	int i, j;
	for ( i=0; i<MAX_FIFO; i++ ){
		fifo_table[i].fd = -1;
	}
	fifoCount = 0;
	for ( j=0; j<20; j++ ){
		semaphoreTable[j].key = -1;
	}
	semCount = 0;
}

int find_new_fifo_fd(){
	int i;
	if ( fifoCount == MAX_FIFO ){
		return -1;
	}
	for ( i=0; i<MAX_FIFO; i++ ){
		if (fifo_table[i].fd == -1){
			return i;
		}
	}
	
	return -1;
}

int delete_fifo_fd(int fd){
	if ( fifo_table[fd].fd ){
		return -1;
	}
	fifo_table[fd].fd = -1;
	fifo_table[fd].curr_size = 0;
	/*semrm_in_kernel(fifo_table[fd].sem_key);*/
	/*fifo_table[fd].sem_key = -1;*/
	fifoCount--;
	return 0;
}

/*for testing fifos*/
int create_fifo(char * name){
	int currfd = find_new_fifo_fd();
	if (currfd == -1 ){
		printf("No more fifos can be created.\n");
		return -1;
	}
	fifo_table[currfd].fd = currfd;/*cabeza*/
	iNode * node = insert_fifo(name,0,NULL);
	/*fifo_table[currfd].file = (char *)node->data.direct_blocks[0];*/
	fifo_table[currfd].file = malloc(MAX_FIFO_SIZE);
	fifo_table[currfd].curr_size = node->data.direct_blocks[1];
	semItem * sem = malloc(sizeof(semItem));
	sem->value = 0;
	semget_in_kernel(sem);
	fifo_table[currfd].sem_key = sem->key;
	if (fifo_table[currfd].sem_key == -1){
		printf("Not enouth space for more semaphores\n");
		return -1;
	}
	fifoCount++;
	return fifo_table[currfd].fd;
}

void write_fifo(int fd, char *buf, int n){
	/*buscar el inodo correspondiente al fd
	*aumentar el semaforo correpondiente tantas
	*unidades como bytes se escriban
	*/
	/*improve!*/
	int j;
	if ( n > (MAX_FIZE_SIZE - fifo_table[fd].curr_size) ){
		memcpy(fifo_table[fd].file,buf ,MAX_FIZE_SIZE - fifo_table[fd].curr_size);
		for(j=0; j<MAX_FIZE_SIZE - fifo_table[fd].curr_size; j++){
			up_in_kernel(fifo_table[fd].sem_key);
		}
		fifo_table[fd].curr_size = MAX_FIZE_SIZE;
	}else{
		memcpy(fifo_table[fd].file, buf ,n);
		for(j=0; j<n; j++){
			up_in_kernel(fifo_table[fd].sem_key);
		}
		fifo_table[fd].curr_size += n;
	}
}

void read_fifo(int fd, char *buf, int n){
	int j;
	char * auxbuf;
	if ( n > MAX_FIZE_SIZE ){
		n = MAX_FIZE_SIZE;
	}
	for (j=0; j<n ; j++){
		down_in_kernel(fifo_table[fd].sem_key);/*blocking*/
	}
	memcpy( buf, fifo_table[fd].file , n );
	/*delete from file all the data read*/
	auxbuf = malloc(fifo_table[fd].curr_size-n);
	memcpy( fifo_table[fd].file, buf , fifo_table[fd].curr_size-n );
	fifo_table[fd].curr_size -= n;

}


void
initializeIDT()
{
	setup_IDT_entry (&idt[0x08], 0x08, (dword)&_int_08_hand, ACS_INT, 0);
	setup_IDT_entry (&idt[0x09], 0x08, (dword)&_int_09_hand, ACS_INT, 0);
	setup_IDT_entry (&idt[0x80], 0x08, (dword)&_int_80_hand, ACS_INT, 0);
	setup_IDT_entry (&idt[0x79], 0x08, (dword)&_int_79_hand, ACS_INT, 0);/*for block_process and kill*/
	idtr.base = 0;
	idtr.base += (dword)&idt;
	idtr.limit = sizeof(idt) - 1;
	_lidt (&idtr);

	return;
}


void
unmaskPICS(){
	_mascaraPIC1(0xFC);
   	_mascaraPIC2(0xFF);

	return;
}


void
setup_IDT_entry (DESCR_INT *item, byte selector, dword offset, byte access,
			 byte cero) {
  item->selector = selector;
  item->offset_l = offset & 0xFFFF;
  item->offset_h = offset >> 16;
  item->access = access;
  item->cero = cero;
  
  return;
}


void
reboot(){
	_export( 0x64, 0xFE); /* pulse CPU reset line */
	return;
}

/**********************************************
Starting point of the whole OS
*************************************************/
int
kmain()
{
	int i, h;
	char * buffer = calloc(512 , 1);
	_Cli();
	k_clear_screen();
	printf("screen clear\n");
	//cache_initarray();
	printf("array init\n");
	//cache_sortarray();
	printf("array sort\n");

	initializeSemaphoreTable();
	printf("semaphore init\n");
	initializeIDT();
	unmaskPICS();
	initializePaging();
	_StartCR3();
	SetupScheduler();
	//printf("after SetupScheduler\n");

	for(h = 0; h < 200; h++){
		write_disk(0,h,buffer,BLOCK_SIZE,0);
	}

	fd_table = (filedescriptor *)calloc(100,1);
	masterBootRecord * mbr = (masterBootRecord *)malloc(512);
	superblock = (masterBlock*)malloc(512);		
	bitmap = (BM*)calloc(BITMAP_SIZE,1);	
	inodemap = (IM*)calloc(INODEMAP_SIZE,1);
	
	read_disk(0,0,mbr,BLOCK_SIZE,0);

	if ( mbr->existFS == 0 ){
		init_filesystem("Chinux", mbr);
	}else{
		load_filesystem();
	}
	
	ready = NULL;
	for(i = 0; i < 4; i++)
		startTerminal(i);

	//free(mbr);
	logPID = CreateProcessAt("Login", (int(*)(int, char**))logUser, 0, 0, (char**)0, PAGE_SIZE, 4, 1);
	_Sti();

	while(TRUE)
	;
	return 1;
}

PROCESS * GetProcessByPID(int pid)
{
	processNode * aux;
	if(pid == 0 || ready == NULL)
		return &idle;

	aux = ((processNode*)ready);
	while(aux != NULL && aux->process->pid != pid)
		aux = ((processNode*)aux->next);
	if(aux == NULL)
		return &idle;
	return aux->process;
}


int CreateProcessAt_in_kernel(createProcessParam * param)
{
	PROCESS * proc;
	proc = malloc(sizeof(PROCESS));
	proc->name = (char*)malloc(15);
	proc->pid = nextPID++;
	proc->pdir = create_proc_ptable();
	proc->foreground = param->isFront;
	proc->priority = param->priority;
	memcpy(proc->name, param->name,str_len(param->name) + 1);
	proc->state = READY;
	proc->tty = param->tty;
	proc->stacksize =  1024;
	proc->stackstart = get_stack_start(proc->pdir);
	proc->ESP = LoadStackFrame(param->process, param->argc, param->argv, (uint32_t)((char *)proc->stackstart /*+ proc->stacksize*/), end_process);
	proc->parent = CurrentPID;
	proc->waitingPid = 0;
	proc->sleep = 0;
	proc->acum = param->priority + 1;
	set_Process_ready(proc);

	return proc->pid;
}


uint32_t LoadStackFrame(int(*process)(int,char**),int argc,char** argv, uint32_t bottom, void(*cleaner)())
{
	STACK_FRAME * frame = (STACK_FRAME*)(bottom - sizeof(STACK_FRAME));
	frame->EBP = 0;
	frame->EIP = (int)process;
	frame->CS = 0x08;
	frame->EFLAGS = 0;
	frame->retaddr = cleaner;
	frame->argc = argc;
	frame->argv = argv;
	return (int)frame;
}

void set_Process_ready(PROCESS * proc)
{
	processNode * node;
	processNode * aux;

	node = malloc(sizeof(processNode));
	node->next = NULL;
	node->process = proc;
	if(ready == NULL)
	{
		ready = (processList)node;
		return;
	}

	aux = ((processNode*)ready);
	while(aux->next != NULL)
		aux = (processNode*)aux->next;
	aux->next = (processList)node;
	return;
}

void block_process_in_kernel(int pid)
{
	processNode * aux;

	timeslot = 1;
	if(ready == NULL)
	{
		_yield();
		return;
	}
	
	aux = ((processNode*)ready);
	if(aux->process->pid == pid)
	{
		aux->process->state = BLOCKED;
	}else{
		while(aux->next != NULL && ((processNode*)aux->next)->process->pid != pid)
			aux = ((processNode*)aux->next);
		if(aux->next == NULL)
		{
			_yield();
			return;
		}
		((processNode*)aux->next)->process->state = BLOCKED;
	}
	_yield();
	return;
}


void awake_process(int pid)
{
	PROCESS * proc;
	
	proc = GetProcessByPID(pid);
	if(proc->state == BLOCKED && !proc->waitingPid)
		proc->state = READY;

	return ;
}


int Idle(int argc, char* argv[])
{
	_Sti();
	while(1)
		;
	return 0;
}

void end_process(void)
{
	PROCESS * proc;
	PROCESS * parent;
	processNode * aux;
	int i;
	
	_Cli();
	actualKilled = 1;
	proc = GetProcessByPID(CurrentPID);
	if(!proc->foreground)
		printf("[%d]\tDone\t%s\n", proc->pid, proc->name);
	else{
		parent = GetProcessByPID(proc->parent);
		if(parent->waitingPid == proc->pid)
		{
			parent->waitingPid = 0;
			awake_process(parent->pid);
		}
	}

	aux = ((processNode *)ready);
	if(aux->process->pid == CurrentPID)
		ready = aux->next;
	else {
		while(aux->next != NULL && ((processNode *)aux->next)->process->pid != CurrentPID)
			aux = ((processNode *)aux->next);
		if(aux->next !=  NULL)
			aux->next = ((processNode*)aux->next)->next;
	}

	for(i = 0; i < 100; i++)
		if(last100[i] == proc->pid)
			last100[i] = -1;

	clear_proc_ptable(proc->pid);
	printf("Welcome\n");

	_Sti();

	return ;
}

void kill_in_kernel(int pid)
{
	PROCESS * proc;
	PROCESS * parent;
	processNode * aux;
	int i,j;

	_Cli();
	for (j = 0; j < 4; j++){
		if( pid == terminals[j].PID && usrLoged ){
			return;
		}
	}
	if(pid == 0 || pid == logoutPID || pid == logPID)
	{
		_Sti();
		return;
	}
	if(pid == CurrentPID)
		actualKilled = 1;
	proc = GetProcessByPID(pid);
	if(proc->foreground){
		parent = GetProcessByPID(proc->parent);
		if(parent->waitingPid == proc->pid)
		{
			parent->waitingPid = 0;
			awake_process(parent->pid);
		}
	}

	aux = ((processNode*)ready);
	while(aux != NULL)
	{
		if(aux->process->parent == pid)
			kill(aux->process->pid);
		aux = ((processNode*)aux->next);
	}

	aux = ((processNode *)ready);
	if(aux->process->pid == pid)
		ready = aux->next;
	else {
		while(aux->next != NULL && ((processNode *)aux->next)->process->pid != pid)
			aux = ((processNode *)aux->next);
		if(aux->next !=  NULL)
			aux->next = ((processNode*)aux->next)->next;
	}

	for(i = 0; i < 100; i++)
		if(last100[i] == proc->pid)
			last100[i] = -1;

	clear_proc_ptable(pid);

	_Sti();

	return ;
}

void clearTerminalBuffer_in_kernel( int ttyid){
	terminals[ttyid].buffer.first_char = terminals[ttyid].buffer.actual_char + 1 % BUFFER_SIZE;
	terminals[ttyid].buffer.size = 0;
}

void waitpid_in_kernel(int pid)
{
	PROCESS* proc;
	proc = GetProcessByPID(CurrentPID);
	proc->waitingPid = pid;
	block_process(CurrentPID);
}

void getTerminalSize_in_kernel(int * size){
	(*size) = terminals[currentTTY].buffer.size;
}

void getTerminalCurPos_in_kernel(int * curpos){
	(*curpos) = terminals[currentProcessTTY].curpos;
}

void getCurrentTTY_in_kernel(int * currtty ){
	(*currtty) = currentTTY;
}


void mkfifo_in_kernel(fifoStruct * param){
	/*TODO*/
	/* open 2 file
	 * create 2 semathore
	 * return param
	 * */
	int fd1, fd2;
	fd1 = create_fifo("fifoa");
	fd2 = create_fifo("fifob");
	param->fd1 = fd1;/*create files / use param->path*/
	param->fd2 = fd2;
}

void rmfifo_in_kernel(fifoStruct * param){
	rmDir("fifoa");
	rmDir("fifob");
	delete_fifo_fd(param->fd1);
	delete_fifo_fd(param->fd2);
}

int find_new_sem_key(){
	int i;
	if ( semCount == 20 ){
		return -1;
	}
	for ( i = 0; i < 20; i++ ){
		if (semaphoreTable[i].key == -1){
			return i;
		}
	}
	return -1;
}

int delete_sem_key(int key){
	if ( semaphoreTable[key].key == -1 ){
		return -1;
	}
	semaphoreTable[key].key = -1;
	semCount--;
	return 0;
}


void
semget_in_kernel(semItem * param){
	if ( semCount == 20 ){
		param->status = -1; /*failed*/
		return;
	}
	param->key = find_new_sem_key();
	param->status = 0; /*ok*/
	param->blocked_proc_pid = -1; /*ok*/
	semaphoreTable[param->key] = (*param);
	semCount++;
}

void
up_in_kernel(int key){
	/*if the process is blocked-> unblock*/
	PROCESS * proc = GetProcessByPID(semaphoreTable[key].blocked_proc_pid);
	if (proc->state == BLOCKED){
		awake_process(proc->pid);
	}
	semaphoreTable[key].value++;
}

void
down_in_kernel(int key){
	if (semaphoreTable[key].value == 0){
		semaphoreTable[key].blocked_proc_pid = CurrentPID;
		block_process_in_kernel(CurrentPID);
	}
	semaphoreTable[key].value--;
}

void semrm_in_kernel(int key){
	if (delete_sem_key(key) == -1 ){
		printf("Error deleting semaphore.\n");
	}
}


void int_79(size_t call, size_t param){
	switch(call){
	case CREATE:/* create function */
		CreateProcessAt_in_kernel((createProcessParam *)param);
		break;
	case KILL: /* kill function */
		kill_in_kernel(param);/*param == pid*/
		break;
	case BLOCK:/* block function */
		block_process_in_kernel(param);/*param == pid*/
		break;
	case CLEAR_TERM:
		clearTerminalBuffer_in_kernel(param); /*param == ttyid*/
		break;
	case WAIT_PID:
		waitpid_in_kernel(param); /*param == pid*/
		break;
	case TERM_SIZE:
		getTerminalSize_in_kernel((int *)param); /*param == *size*/
		break;
	case TERM_CURPOS:
		getTerminalCurPos_in_kernel((int *)param); /*param == *curpos*/
		break;
	case CURR_TTY:
		getCurrentTTY_in_kernel((int *)param); /*param == *currtty*/
		break;
	case MK_FIFO:
		mkfifo_in_kernel((fifoStruct *)param);
		break;
	case SEM_GET:
		semget_in_kernel((semItem *)param);
		break;
	case SEM_UP:
		up_in_kernel(param); /*param == key*/
		break;
	case SEM_DOWN:
		down_in_kernel(param);/*param == key*/
		break;
	case MK_DIR:
		makeDir((char *)param);/*param == nameDIR*/
		break;
	case LS_COM:
		ls_in_kernel((char *)param);/*param == path*/
		break;
	case RM_COM:
		rmDir((char *)param);/*param == path*/
		break;
	case TOUCH_COM:
		touch_in_kernel((char *)param);/*param == filename*/
		break;
	case CAT_COM:
		cat_in_kernel((char *)param);/*param == filename*/
		break;
	case CD_COM:
		cd_in_kernel((char *)param);/*param == path*/
		break;
	case CREAT_COM:
		creat_in_kernel((creat_param *)param);
		break;
	case RM_FIFO:
		rmfifo_in_kernel((fifoStruct *)param);
		break;
	case SEM_RM:
		semrm_in_kernel(param);
		break;
	}

}

void startTerminal(int pos)
{
	int i;
	for( i = 0; i < 80*25*2; i++)
	{
		terminals[pos].terminal[i]=' ';
		i++;
		terminals[pos].terminal[i] = WHITE_TXT;
	}
	terminals[pos].buffer.actual_char = BUFFER_SIZE-1;
	terminals[pos].buffer.first_char = 0;
	terminals[pos].buffer.size = 0;

	return;
}

void sleep(int secs)
{
	PROCESS * proc;
	proc = GetProcessByPID(CurrentPID);
	proc->sleep = 18 * secs;
	block_process(CurrentPID);
}

void logUser(void)
{
	int i, fd, usrNotFound, j;
	user * usr;
	current = superblock->root;
	currentUsr.group = ADMIN;
	fd = do_open("usersfile", 777, 777);
	usr = malloc(sizeof(user) * 100);
	do_read(fd, (char *)usr, sizeof(user) * 100);

	while(!usrLoged)
	{
		usrNotFound = 1;
		printf("username: ");
		moveCursor();
		usrName = 1;
		block_process(CurrentPID);
		scanf("%s", buffcopy);
		for(i = 0; i < 100 && usr[i].usrID != 0 && usrNotFound; i++)
		{
			if(strcmp(usr[i].name, buffcopy))
			{
				usrNotFound = 0;
			}
		}
		usrName = 0;
		printf("\n");
		clearTerminalBuffer(currentTTY);
		for(j = 0; j < BUFFER_SIZE; j++)
			buffcopy[j] = 0;
		if(!usrNotFound)
		{
			printf("password: ");
			moveCursor();
			password = 1;
			block_process(CurrentPID);
			scanf("%s", buffcopy);
			if(strcmp(usr[i - 1].password, buffcopy))
			{
				usrLoged = 1;
				currentUsr.usrID = usr[i - 1].usrID;
				currentUsr.group = usr[i - 1].group;
			}
			else
				printf("\nIncorrect password. Please try again");
			password = 0;
			printf("\n");
		} else
			printf("User not found. Please try again\n");
		clearTerminalBuffer(currentTTY);
		for(j = 0; j < BUFFER_SIZE; j++)
			buffcopy[j] = 0;
	}
	terminals[0].PID = CreateProcessAt("Shell0", (int(*)(int, char**))shell, 0, 0, (char**)0, PAGE_SIZE, 2, 1);
	//printf("terminals[0].PID \n");
	terminals[1].PID = CreateProcessAt("Shell1", (int(*)(int, char**))shell, 1, 0, (char**)0, PAGE_SIZE, 2, 1);
	//printf("terminals[1].PID \n");
	terminals[2].PID = CreateProcessAt("Shell2", (int(*)(int, char**))shell, 2, 0, (char**)0, PAGE_SIZE, 2, 1);
	//printf("terminals[2].PID \n");
	terminals[3].PID = CreateProcessAt("Shell3", (int(*)(int, char**))shell, 3, 0, (char**)0, PAGE_SIZE, 2, 1);
	//printf("terminals[3].PID \n");
	//do_close(fd);

	//free(usr);
	_Sti();
	return;
}


void logout(int argc, char * argv[])
{
	int i;
	usrLoged = 0;
	for(i = 0; i < 4; i++)
		kill(terminals[i].PID);
	logPID = CreateProcessAt("logUsr", (int(*)(int, char**))logUser, currentProcessTTY, 0, (char**)0, PAGE_SIZE, 4, 1);

	_Sti();
}

void createusr(char * name, char * password, char * group)
{
	int i, fd, length;
	user * usr;
	iNode * aux;
	aux = current;
	current = superblock->root;
	fd = do_open("usersfile", 777, 777);
	usr = malloc(sizeof(user) * 100);
	do_read(fd, (char *)usr, sizeof(user) * 100);
	for(i = 0; i < 100 && usr[i].usrID; i++)
		if(strcmp(usr[i].name, name))
		{
			printf("Error: User already exists.\n");
			return ;
		}
	if(i == 100)
	{
		printf("Error: Too many users created.\n");
		return ;
	}
	length = str_len(name);
	name[length - 1] = 0;
	memcpy(usr[i].name, name, length - 1);
	length = str_len(password);
	password[length - 1] = 0;
	memcpy(usr[i].password, password, length - 1);
	usr[i].usrID = ++usrID;
	if(strcmp(group, "admin"))
		usr[i].group = ADMIN;
	else
		usr[i].group = USR;
	do_close(fd);
	rmDir("usersfile");
	fd = do_creat("usersfile", 777);
	write(fd, (void *)usr, sizeof(user) * 100);
	do_close(fd);

	current = aux;

	//free(usr);
	return;	
}
