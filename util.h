/* Matan Gazit
 * CS 4760
 * 04/04/21
 *
 * util.h
 *
 * header file for util.c
 */

#ifndef UTIL_H
#define UTIL_H

#include <stdbool.h>
#include <sys/types.h>

#define BUFF_LEN 1024
#define PROC_CONC_MAX 18 // concurrent max processes allowed to run in the system at a time
#define PROC_MAX 100 // maximum amount of processes allowed in the system at a time
#define TIMEOUT 3
#define DEF_LOG_PATH "./logfile"
#define QUANTUM_BASE 1e7
#define Q_SET_COUNT 4
#define Q_SET_SIZE PROC_CONC_MAX
#define Q_BLOCK_SIZE PROC_CONC_MAX
#define EXIT_STATUS_OFFSET 20
#define DELTA 1e4

/* STRUCTURE DEFINTIIONS */
// Time
typedef struct
{
	long seconds;
	long nano; // nanoseconds0
} Time;
// PCB
typedef struct
{
	pid_t  PCB_pid; // actual pid of this PCB's process
	unsigned int loc_pid; // simulated process pid
	unsigned int prio; // process priority in the runqueue
	Time arrival; // when process arrived in the system
	Time proc_exit; // when process exited the system
	Time cpu; // Time of the CPU used up by process
	Time q; // Time process spent in runqueue
	Time block; // Time process spent blocked
	Time waiting; // Time process spent waiting on some signal
	Time in_system; // total time process spent in system
} PCB;
// Util
typedef struct
{
	Time sys_time;
	PCB proc_table[PROC_CONC_MAX];
} Util;
// Msg
typedef struct
{
	long type;
	char msg_text[BUFF_LEN];
} Msg;
/* PROTOTYPES */
void init(int, char**);
void error(char *fmt, ...);
void crash(char*);
char *get_program_name();

void alloc_shared_memory(bool);
void dealloc_shared_memory();
Util *get_mem_seg();

// messaging
void alloc_msg_q(bool);
void dealloc_msg_q();
int send_msg(Msg*, int, pid_t, char*, bool);
int rec_msg(Msg*, int, pid_t, bool);
int get_parent_q();
int get_child_q();

void logger(char*, ...);
void cleanup();

void set_time(Time*, long);
void add_time(Time*, long);
void clear_time(Time*);
void copy_time(Time*, Time*);
Time subt_time(Time*, Time*);
void curr_time(Time*);
void swap_time(Time*, Time*);
void avg_time(Time*, int);

int get_q_quantum(int);
int get_usr_quantum(int);

#endif
