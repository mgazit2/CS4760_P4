/* Matan Gazit
 * CS 4760
 * 04/04/21
 * oss.c
 */

#include <getopt.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/msg.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "oss.h"
#include "queue.h"
#include "util.h"

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define BIT_SET(a, b) ((a) |= (1ULL << (b)))
#define BIT_CLEAR(a, b) ((a) &= ~(1ULL << (b)))
#define BIT_FLIP(a, b) ((a) ^= (1ULL << (b)))
#define BIT_CHECK(a, b) (!!((a) & (1ULL << (b))))
#define MAX_TIME_NEW_PROCS_SEC 1
#define MAX_TIME_NEW_PROCS_NANO 15000
#define CHANCE_PROCS_RT 5

/* GLOBALS */
typedef unsigned long int bv_t; /* Type-defined bit-vector */
static volatile bool quit = false;

/* STRUCT */
typedef struct {
	Util *util; 
	Queue *q_set0[Q_SET_COUNT];
	Queue *q_set1[Q_SET_COUNT];
	Queue **active;
	Queue **expired;
	Queue *blocked;
	Msg *msg;
	PCB *run;
	bv_t vec;
	Time idle;
	Time next_spawn_try;
	unsigned int spawned_proc_count;
	unsigned int exited_proc_count;
	Time total_cpu;
	Time total_block;
	Time total_wait;
	int proc_count_realtime;
	int proc_count_normal;
} Global;

/* PROTOTYPES */
void print_usage();
void timeout(int sig);
void kill_all(int sig);
void init_q();
void sched_proc();
void try_spawn();
bool proc_can_spawn();
void proc_running();
void try_q_swap();
void blocked_procs();
void try_schedule();
void spawn();
int find_a_pid();
PCB* get_pcb(unsigned int loc_PID);
void init_pcb(PCB* pcb, unsigned int loc_PID, pid_t pcb_PID);
void proc_created(PCB* pcb);
bool is_running();
void on_proc_terminated(PCB* pcb);
void on_proc_exp(PCB* pcb);
void on_proc_block(PCB* pcb);
Queue** get_active_q();
Queue** get_exp_q();
Queue* get_block_q();
void q_swap();
void unblock_proc(PCB* pcb);
void schedule(PCB* pcb);
void proc_scheduled(PCB* pcb);
void on_proc_exit(PCB* pcb);
void dealloc_pcb(PCB* pcb);

/* MORE GLOBALS */
static Global *global = NULL;
static char* log_path = DEF_LOG_PATH;
int runtime = 100;
clock_t spawn_timer;

int main(int argc, char** argv)
{
	global = (Global *)malloc(sizeof(Global));
	signal(SIGINT, kill_all);
	signal(SIGALRM, timeout);
	
	if (argc == 1)
	{
		//printf("%d\n", global -> exited_proc_count);
		printf("Too few arguments given, run again with [-h] for usage options\n");
		return EXIT_FAILURE;
	}	

	while (true)
	{
		int c = getopt(argc, argv, "hs:l:");
		if (c == -1) break;
		switch (c)
		{
			case 'h':
				print_usage();
				return EXIT_SUCCESS;
				break;
			case 's':
				runtime = atoi(optarg);
				printf("Using custom max program runtime: %d\n", runtime);
				break;
			case 'l':
				log_path = optarg;
				printf("Using custom filename for [logfile]: %s\n", log_path);
				break;
			default:
				printf("Unrecognizable command line input encountered... Terminating...\n");
				return EXIT_FAILURE;
		}
	}
	spawn_timer = clock();
	alarm(runtime); // set alarm
	alloc_shared_memory(true);
	alloc_msg_q(true);

	set_filename(log_path);

	global -> util = get_mem_seg();
	global -> msg = (Msg *)malloc(sizeof(Msg));

	FILE* fp;
	if ((fp = fopen(log_path, "w")) == NULL) crash("fopen");
	if (fclose(fp) == EOF) crash("fclose");
	
	init_q(); // initialize all queues in the Global struct
	set_time(&global -> util -> sys_time, 0); // sets clock
	set_time(&global -> next_spawn_try, 0); // sets clock
	srand(time(NULL));

	while (true)
	{
		printf("EXITED: %d\n SPAWNED: %d\n", global -> exited_proc_count, global -> spawned_proc_count);
		add_time(&global -> util -> sys_time, DELTA);
		add_time(&global -> idle, DELTA);

		sched_proc(); // handle process scheduling for iteration
		printf("after sched_proc\n");

		int state;
		pid_t pid = waitpid(-1, &state, WNOHANG);
		if (pid > 0)
		{
			if (WIFEXITED(state))
			{
				if (WEXITSTATUS(state) > EXIT_STATUS_OFFSET)
				{
					int loc_pid = WEXITSTATUS(state) - EXIT_STATUS_OFFSET;
					PCB* pcb = get_pcb(loc_pid);
					on_proc_exit(pcb);					
				}
			}
		}
		//if(!can_sched()) break;
		if(global -> exited_proc_count == global -> spawned_proc_count) break;
	}

	if (quit) printf("We're out of time folks...\n");

	printf("What have we done?\n");
	printf("\t CPU-Bound Processes: %d\n", global -> proc_count_realtime);
	printf("\t IO-Bound Processes: %d\n", global -> proc_count_normal);

	printf("Our totals...!\n");
	printf("\tCPU:			%ld:%ld\n", global -> total_cpu.seconds, global -> total_cpu.nano);
	printf("\tBlock:    %ld:%ld\n", global -> total_block.seconds, global -> total_cpu.nano);
	printf("\tWait:    	%ld:%ld\n", global -> total_wait.seconds, global -> total_cpu.nano);
	printf("\tSystem:   %ld:%ld\n", global -> util -> sys_time.seconds, global -> util -> sys_time.nano);
	printf("\tIdle:    	%ld:%ld\n", global -> idle.seconds, global -> idle.nano);

	avg_time(&global -> total_cpu, global -> exited_proc_count);
	avg_time(&global -> total_block, global -> exited_proc_count);
	avg_time(&global -> total_wait, global -> exited_proc_count);
	avg_time(&global -> util -> sys_time, global -> exited_proc_count);
	avg_time(&global -> idle, global -> exited_proc_count);

	printf("Our average times...!\n");
	printf("\tCPU:      %ld:%ld\n", global -> total_cpu.seconds, global -> total_cpu.nano);
	printf("\tBlock:    %ld:%ld\n", global -> total_block.seconds, global -> total_cpu.nano);
	printf("\tWait:     %ld:%ld\n", global -> total_wait.seconds, global -> total_cpu.nano);
	printf("\tSystem:   %ld:%ld\n", global -> util -> sys_time.seconds, global -> util -> sys_time.nano);
	printf("\tIdle:     %ld:%ld\n", global -> idle.seconds, global -> idle.nano);

	cleanup();

	return EXIT_SUCCESS;
}

void print_usage()
{
	printf("USAGE\n\n");
	printf("oss should take in several command line options as follows: ");
	printf("./oss [-h] [-s t] [-l f]\n");
	printf("\n[-h] Describe how the project should be run and then, terminate.\n");
	printf("[-s t] Indicate how many maximum seconds before the system terminates.\n");
	printf("[-l f] Specify a particular name for the log file.\n");
}

void timeout(int sig)
{
	perror("timeout()");
	int i = 0;
	printf("We're out of time, folks...\n");
	//killpg((*group_pid), SIGTERM);
	for (; i < 20; i++) wait(NULL);
	printf("Exited all active processes...\n");
	cleanup();
	exit(1);
}

void kill_all(int sig)
{
	// terminate all process with ^C
	perror("kill_all()");
	int i = 0;
	printf("Received termination signal, exiting...\n");
	//killpg((*group_pid), SIGTERM);
	for (; i < 20; i++) wait(NULL);
	printf("Exited all active processes... \n");
	cleanup();
	exit(1);
}

// handles the initialization of queues used by the OSS
void init_q()
{
	int i; // iterator
	for (i = 0; i < Q_SET_COUNT; i++)
	{
		global -> q_set0[i] = new_queue(Q_SET_SIZE);
		global -> q_set1[i] = new_queue(Q_SET_SIZE);
	} // end for fill queue sets
	global -> active = global -> q_set0; // set the active queue to be q_set0
	global -> expired = global -> q_set1; // set the expired queue to be q_set1
	global -> blocked = new_queue(Q_BLOCK_SIZE);
}

// handles the calling of methods necessary for scheduling a process
// sched_proc() first trys to spawn a process if room is available in the system
// handles running processes in the system
// tries to swap the queues if necessary and possible
// handles blocked processes in the system
// tries to schedule the processes for time on CPU
void sched_proc()
{
	printf("in sched_proc\n");
	try_spawn();
	proc_running();
	try_q_swap();
	blocked_procs();
	try_schedule();
}

// handles the check for whether or not a process can spawn in the system
void try_spawn()
{
	clock_t spawn_timer_check;
	spawn_timer_check = clock();
	if ((double)(spawn_timer_check - spawn_timer) / CLOCKS_PER_SEC > TIMEOUT) return;
	printf("try_spawn()\n");
	if (proc_can_spawn()) spawn();
}

// returns true if there is room for a new process to spawn in the system
bool proc_can_spawn()
{
	printf("proc_can_spawn()\n");
	return !quit && global -> spawned_proc_count < PROC_MAX;
}

// handles the spawning of a new process into the system
void spawn()
{
	sleep(1); // sleeps about a second between spawning
	printf("spawn()\n");
	int loc_PID = find_a_pid();
	if (loc_PID > -1)
	{
		BIT_SET(global -> vec, loc_PID - 1);

		pid_t pid = fork();
		if (pid == -1) crash("fork");
		else if (pid == 0)
		{
			char buff[BUFF_LEN];
			snprintf(buff, BUFF_LEN, "%d", loc_PID);
			execl("./child", "child", buff, NULL);
			crash("execl");
		}

		PCB *pcb = get_pcb(loc_PID);
		init_pcb(pcb, loc_PID, pid);
		proc_created(pcb);
	}
}

// searches for and finds an availble PID for a new process before spawning
int find_a_pid()
{
	printf("find_a_pid()\n");
	unsigned int i = 1;
	unsigned int pos = 1;

	while ((i & global -> vec) && pos <= PROC_CONC_MAX)
	{
		i <<= 1;
		pos++;
	}

	return pos <= PROC_CONC_MAX ? pos : -1;
}

// returns pcb from location in the proc table of util struct
PCB* get_pcb(unsigned int loc_PID)
{
	printf("get_pcb()");
	return &global -> util -> proc_table[loc_PID];
}

// handles the initialization of new process PCB
void init_pcb(PCB* pcb, unsigned int loc_PID, pid_t pcb_PID)
{
	printf("init_pcb\n");
	pcb -> loc_pid = loc_PID;
	pcb -> PCB_pid = pcb_PID;
	pcb -> prio = rand() % 100 < CHANCE_PROCS_RT ? 0 : 1;

	if (pcb -> prio == 0) global -> proc_count_realtime++;
	else global -> proc_count_normal++;

	clear_time(&pcb -> arrival);
	clear_time(&pcb -> proc_exit);
	clear_time(&pcb -> cpu);
	clear_time(&pcb -> q);
	clear_time(&pcb -> block);
	clear_time(&pcb -> waiting);
	clear_time(&pcb -> in_system);

	copy_time(&global -> util -> sys_time, &pcb -> arrival);
	copy_time(&global -> util -> sys_time, &pcb -> in_system);
}

void proc_created(PCB* pcb)
{
	printf("proc_created()\n");
	printf("in proc_created\n");
	global -> spawned_proc_count++;
	q_push(get_active_q()[pcb -> prio], pcb -> loc_pid);
	global -> next_spawn_try.seconds = global -> util -> sys_time.seconds;
	global -> next_spawn_try.nano = global -> util -> sys_time.nano;
	int sec = abs(rand() * rand()) % (MAX_TIME_NEW_PROCS_SEC + 1);
	int ns = abs(rand() * rand()) % (MAX_TIME_NEW_PROCS_NANO + 1);
	add_time(&global -> next_spawn_try, sec * 1e9 * ns);
	logger("%-6s PID: %2d, Priority: %d", "CCCCCC", pcb -> loc_pid, pcb -> prio);
}

/* PROC_RUNNING BLOCK START */

// checks to see if the process is running,
// 	if the process is running, have it receive some message
// 	a handler is called to handle the received message appropriately
void proc_running()
{
	printf("proc_running()\n");
	if (is_running())
	{
		PCB *pcb = global -> run;
		rec_msg(global -> msg, get_parent_q(), pcb -> PCB_pid, true);
		if (strcmp(global -> msg -> msg_text, "TERMINATED") == 0) on_proc_terminated(pcb);
		else if (strcmp(global -> msg -> msg_text, "EXPIRED") == 0) on_proc_exp(pcb);
		else if (strcmp(global -> msg -> msg_text, "BLOCKED") == 0) on_proc_block(pcb);
	}
}

// returns true is a process is running
bool is_running()
{
	printf("is_running()\n");
	return global -> run != NULL;
}

// handles proc termination
void on_proc_terminated(PCB* pcb)
{
	printf("on_proc_terminated()\n");
	rec_msg(global -> msg, get_parent_q(), pcb -> PCB_pid, true);
	copy_time(&global -> util -> sys_time, &pcb -> proc_exit);
	int cent = atoi(global -> msg -> msg_text);
	int cost = get_usr_quantum(pcb -> prio);
	int times = (int)((double)cost * ((double)cent / (double)100));
	add_time(&pcb -> cpu, times);
	add_time(&pcb -> q, times);
	add_time(&global -> util -> sys_time, times);

	subt_time(&pcb -> waiting, &pcb -> cpu);
	subt_time(&pcb -> waiting, &pcb -> block);

	global -> run = NULL;
	//global -> exited_proc_count++;

	logger("%-6s PID: %2d, Priority: %d", "TTTTTT", pcb -> loc_pid, pcb -> prio);
	printf("\nPROCESS TERMINATED\n");
	printf("\tActual PID: %d\n", pcb -> PCB_pid);
	printf("\tLocal PID: %d\n", pcb -> loc_pid);
	printf("\tPriority: %d\n", pcb -> prio);
	printf("\tArrival:  %ld:%ld\n", pcb -> arrival.seconds, pcb -> arrival.nano);
	printf("\tExit:  %ld:%ld\n", pcb -> proc_exit.seconds, pcb -> proc_exit.nano);
	printf("\tCPU:  %ld:%ld\n", pcb -> cpu.seconds, pcb -> cpu.nano);
	printf("\tWaiting:  %ld:%ld\n", pcb -> waiting.seconds, pcb -> waiting.nano);
	printf("\tBlock:  %ld:%ld\n", pcb -> block.seconds, pcb -> block.nano);
}

// handler for processes ecpired
void on_proc_exp(PCB* pcb)
{
	printf("on_proc_exp()\n");
	//int prev_prio = pcb -> prio;
	int next_prio = pcb -> prio;

	int cent = 100;
	int cost = get_usr_quantum(pcb -> prio);
	int times = (int)((double)cost * ((double)cent / (double)100));
	add_time(&pcb -> cpu, times);
	add_time(&pcb -> q, times);
	add_time(&global -> util -> sys_time, times);

	if (pcb -> prio == 0)
	{
		q_push(get_active_q()[next_prio], pcb -> loc_pid);
		logger("%-6s PID: %2d, Priority: %d", "EEEEEE", pcb -> loc_pid, pcb -> prio);
	}
	else
	{
		int n = get_q_quantum(pcb -> prio);
		if (n != -1 && pcb -> q.nano >= n)
		{
			next_prio++;
			if(next_prio > Q_SET_COUNT - 1) next_prio = Q_SET_COUNT - 1;
			pcb -> prio = next_prio;
			clear_time(&pcb -> q);
			logger("%-6s PID: %2d, Priority: %d", "EEEEEE", pcb -> loc_pid, pcb -> prio);
		}
		else
		{
			logger("%-6s PID: %2d, Priority: %d", "EEEEEE", pcb -> loc_pid, pcb -> prio);
		}
		q_push(get_exp_q()[next_prio], pcb -> loc_pid);
	}
	global -> run = NULL;
}

// handler for processes blocked
void on_proc_block(PCB* pcb)
{
	printf("on_proc_block()\n");
	rec_msg(global -> msg, get_parent_q(), pcb -> PCB_pid, true);

	int cent = atoi(global -> msg -> msg_text);
	int cost = get_usr_quantum(pcb -> prio);
	int times = (int)((double)cost * ((double)cent / (double)100));
	add_time(&pcb -> cpu, times);
	add_time(&pcb -> q, times);
	add_time(&global -> util -> sys_time, times);

	logger("%-6s PID: %2d, Priority: %d", "BBBBBB", pcb -> loc_pid, pcb -> prio);
	q_push(get_block_q(), pcb -> loc_pid);
	global -> run = NULL;
}

// handler for process exiting
void on_proc_exit(PCB* pcb)
{
	printf("on_proc_exit()\n");
	global -> exited_proc_count++;
	add_time(&global -> total_cpu, pcb -> cpu.seconds + pcb -> cpu.nano);
	add_time(&global -> total_block, pcb -> block.seconds + pcb -> block.nano);
	add_time(&global -> total_wait, pcb -> waiting.seconds + pcb -> waiting.nano);
	dealloc_pcb(pcb);
}

// free PCB memory for a given proc
void dealloc_pcb(PCB* pcb)
{
	printf("dealloc_pcb()\n");
	BIT_CLEAR(global -> vec, pcb -> loc_pid - 1);
	memset(&global -> util -> proc_table[pcb -> loc_pid], 0, sizeof(PCB));
}

// check to see if a queue swap is necessary and needed
void try_q_swap()
{
	printf("try_q_swap()\n");
	if (is_running()) return;
	int i;
	for (i = 0; i < Q_SET_COUNT; i++)
	{
		if(!q_empty(get_active_q()[i])) return;
	}

	bool flag = false;
	for (i = 0; i < Q_SET_COUNT; i++)
	{
		if(!q_empty(get_exp_q()[i])) flag = true;
	}
	if (!flag) return;

	q_swap();
}

// swaps the active and expired queues
void q_swap()
{
	printf("q_swap()\n");
	Queue **temp = global -> active;
	global -> active = global -> expired;
	global -> expired = temp;
}

// handler for blocked processes
// cycles through the blocked queue and unblocks possible processes
void blocked_procs()
{
	printf("blocked_procs()\n");
	Queue* blocked = get_block_q();
	if(!q_empty(blocked))
	{
		if (!is_running())
		{
			add_time(&global -> util -> sys_time, 5e6);
			add_time(&global -> idle, 5e6);
		}

		int i;
		for (i = 0; i < blocked -> num_occupied; i++)
		{
			printf("in loop in blocked_procs()\n");
			PCB* pcb = get_pcb(q_pop(blocked));
			if (msgrcv(get_parent_q(), global -> msg, sizeof(Msg), pcb -> PCB_pid, IPC_NOWAIT) > 1 && strcmp(global -> msg -> msg_text, "UNBLOCKED") == 0)
			{
				unblock_proc(pcb);
			}
			else
			{
				q_push(get_block_q(), pcb -> loc_pid);
			}
		}
	}
}

// unblock a given process
void unblock_proc(PCB* pcb)
{
	printf("unblock_procs()\n");
	q_push(get_active_q()[pcb -> prio], pcb -> loc_pid);
	logger("%-6s PID: %2d, Priority: %d", "UUUUUU", pcb -> loc_pid, pcb -> prio);
}

// checks to see if it is possible to currently schedule a process
// this is based on processes currently in active queue
void try_schedule()
{
	printf("try_schedule\n");
	if (is_running()) return;

	int i;
	for (i = 0; i < Q_SET_COUNT; i++)
	{
		if(!q_empty(get_active_q()[i]))
		{
			int loc_PID = q_pop(get_active_q()[i]);
			PCB* pcb = &global -> util -> proc_table[loc_PID];
			schedule(pcb);
			break;
		}
	}
}

// sends message to process notifying it of scheduling
void schedule(PCB* pcb)
{
	printf("schedule()\n");
	global -> run = pcb;
	send_msg(global -> msg, get_child_q(), pcb -> PCB_pid, "", false);
	proc_scheduled(pcb);
}

// logs scheduling of a process
void proc_scheduled(PCB *pcb)
{
	printf("proc_scheduled\n");
	logger("%-6s PID: %2d, Priority: %d", "SSSSSS", pcb -> loc_pid, pcb -> prio);
}

// get function for active q
Queue** get_active_q()
{
	return global -> active;
}

// get function for expired q
Queue** get_exp_q()
{
	return global -> expired;
}

// get function for block q
Queue* get_block_q()
{
	return global -> blocked;
}
