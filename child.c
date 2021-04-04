/* Matan Gazit
 * CS 4760
 * 04/04/21
 *
 * child.c
 */

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "util.h"
#include "user.h"

/* PROTOTYPES */
void sim_user();
bool msg_terminate(PCB* pcb);
bool msg_expire(PCB* pcb);
void proc_terminate();
void proc_expire();
void proc_block();

/* STRUCT */
typedef struct {
	Util *util;
	PCB *pcb;
	Msg msg;
} Global;

/* GLOBALS */
static Global* global = NULL;

int main(int argc, char** argv)
{
	global = (Global *)malloc(sizeof(Global));
	
	init(argc, argv);	
	
	int loc_PID;

	if (argc < 2)
	{
		error("Child process spawned without a PID... Terminating...\n");
		exit(EXIT_FAILURE);
	}
	else
	{
		loc_PID = atoi(argv[1]);	
	}

	alloc_shared_memory(false);
	alloc_msg_q(false);

	global -> util = get_mem_seg();
	global -> pcb = &global -> util -> proc_table[loc_PID];

	sim_user();
	
	return EXIT_SUCCESS;
}

void sim_user()
{
	printf("sim_user\n");
	srand(time(NULL) ^ (getpid() << 16));

	while (true)
	{
		rec_msg(&global -> msg, get_child_q(), global -> pcb -> PCB_pid, true);
		if (msg_terminate(global -> pcb)) proc_terminate();
		else if (msg_expire(global -> pcb)) proc_expire();
		else proc_block();
	}
}

bool msg_terminate(PCB* pcb)
{
	//return true;
	return rand() % 100 < (CHANCE_PROC_TERM * (pcb -> prio == 0 ? 2 : 1));
}

bool msg_expire(PCB* pcb)
{
	return rand() % 100 < CHANCE_PROC_EXP;
}

void proc_terminate()
{
	// sends message to the OSS
	send_msg(&global -> msg, get_parent_q(), global -> pcb -> PCB_pid, "TERMINATED", true);
	int rp = (rand() % 99) + 1;
	char buff[BUFF_LEN];
	snprintf(buff, BUFF_LEN, "%d", rp);
	send_msg(&global -> msg, get_parent_q(), global -> pcb -> PCB_pid, buff, true);
	exit(EXIT_STATUS_OFFSET + global -> pcb -> loc_pid);
}

void proc_expire()
{
	send_msg(&global -> msg, get_parent_q(), global -> pcb -> PCB_pid, "EXPIRED", true);
}

void proc_block()
{
	send_msg(&global -> msg, get_parent_q(), global -> pcb -> PCB_pid, "BLOCKED", true);

	int rp = (rand() % 99) + 1;
	char buff[BUFF_LEN];
	snprintf(buff, BUFF_LEN, "%d", rp);
	send_msg(&global -> msg, get_parent_q(), global -> pcb -> PCB_pid, buff, true);
	Time unblock = { .seconds = global -> util -> sys_time.seconds,
									 .nano = global -> util -> sys_time.nano };
	int sec = rand() % (3 + 1);
	int ns = rand() % (1000 + 1);
	add_time(&unblock, sec * 1e9 + ns);
	add_time(&global -> pcb -> block, sec * 1e9 + ns);

	while (!(global -> util -> sys_time.seconds >= unblock.seconds &&
					 global -> util -> sys_time.nano >= unblock.nano));

	send_msg(&global -> msg, get_parent_q(), global -> pcb -> PCB_pid, "UNBLOCKED", true);
}
