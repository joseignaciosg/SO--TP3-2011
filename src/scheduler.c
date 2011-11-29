/********************************** 
*
*  scheduler.c
*  	Galindo, Jose Ignacio
*  	Homovc, Federico
*  	Loreti, Nicolas
*		ITBA 2011
*
***********************************/

#include "../include/defs.h"
#include "../include/kernel.h"
#include "../include/utils.h"
#include "../include/stdio.h"
#include "../include/paging.h"
#include "../include/malloc.h"

int last100[100]={0};
int counter100 = 0;
int FirstTime = 1;
int actualKilled = 0;
int timeslot = TIMESLOT;

extern int CurrentPID;
extern int currentProcessTTY;
extern int currentTTY;
extern PROCESS idle;
extern processList ready;

void SaveESP (int);
PROCESS * GetNextProcess (void);
PROCESS * GetNextTask(void);
int LoadESP(PROCESS*);
int isTimeSlot(void);


int isTimeSlot()
{
	processNode * node;
	
	node = ((processNode*)ready);
	while(node != NULL)
	{
		if(node->process->sleep)
		{
			node->process->sleep--;
			if(!node->process->sleep)
				awake_process(node->process->pid);
		}
		node = ((processNode*)node->next);
	}
	if(--timeslot)
		return timeslot;
	timeslot = TIMESLOT;
	return 0;
}

void SaveESP (int ESP)
{
	timeslot = TIMESLOT;
	PROCESS* temp;
	if (!FirstTime && !actualKilled)
	{
		temp = GetProcessByPID(CurrentPID);
		temp->ESP = ESP;
		if(temp->state == RUNNING)
			temp->state = READY;
	}
	actualKilled = 0;
	return;
}

PROCESS * GetNextProcess(void)
{
	PROCESS * temp;
	temp = GetNextTask();
	temp->state = RUNNING;
	CurrentPID = temp->pid;
	if(temp->pid == 0)
		currentProcessTTY = currentTTY;
	else
		currentProcessTTY = temp->tty;
	last100[counter100] = CurrentPID;
	counter100 = (counter100 + 1) % 100;
	return temp;
}

PROCESS * GetNextTask()
{
	processNode * aux;
	int flag = 0, beginning = 0;

	if(ready == NULL)
		return &idle;

	aux = ((processNode *)ready);

	if(CurrentPID == 0)
	{
		while(aux != NULL)
		{
			if(aux->process->state == READY)
				return aux->process;
			aux = ((processNode*)aux->next);
		}
		return &idle;
	}

	while(aux != NULL)
	{
		if(aux->process->pid == CurrentPID)
			flag = 1;
		aux = ((processNode*)aux->next);
		if(flag && aux != NULL && aux->process->state == READY)
			return aux->process;
		if(aux == NULL && !beginning)
		{
			beginning = 1;
			aux = ((processNode*)ready);
			if(aux->process->state == READY)
				return aux->process;
		}
	}
	return &idle;

}

int LoadESP(PROCESS* proc)
{
	return proc->ESP;
}

void SetupScheduler(void)
{
	idle.pdir = create_proc_ptable();
	//printf("idle stack created. pdir = %d\n", idle.pdir);
	idle.name = (char*)malloc(8);

	//printf("after idle.name = (char*)malloc(8); \n");

	idle.pid = 0;
	idle.foreground = 0;
	idle.priority = 4;
	memcpy(idle.name, "Idle", str_len("Idle") + 1);
	idle.state = READY;
	idle.tty = 0;
	idle.stacksize = 4096;
	//printf("idle.pdir : %d\n", idle.pdir);
	idle.stackstart = get_stack_start(idle.pdir);
	//printf("idle.stackstart : %d\n", idle.stackstart);
	idle.ESP = LoadStackFrame(Idle, 0, 0, (uint32_t)((char *)idle.stackstart /*+ idle.stacksize*/), end_process);
	//printf("idle process created\n");
	idle.parent = -1;
	idle.waitingPid = 0;
	idle.sleep = 0;
	
	return;
}
