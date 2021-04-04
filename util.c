/* Matan Gazit
 * CS 4760
 * 04/04/21
 *
 * util.c
 *
 * utility functions for the oss
 * 	seperate from main.c for readability
 */

#include <libgen.h>
#include <math.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "util.h"

#define PERMITS (S_IRUSR | S_IWUSR)

static char *program_name = NULL;

static key_t shared_mem_key;
static int shared_mem_id;
static Util *shmptr = NULL;

static key_t parent_msg_q_key;
static int parent_msg_q_id;

static key_t child_msg_q_key;
static int child_msg_q_id;

// Handles the initialization of various printer boundaries and executable name
void init(int argc, char** argv)
{
	program_name = argv[0];
	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);
}

// handles program input errors
void error(char* fmt, ...)
{
	char buf[BUFF_LEN];
	va_list args;
	va_start(args, fmt);
	vsnprintf(buf, BUFF_LEN, fmt, args);
	va_end(args);

	fprintf(stderr, "%s: %s\n", program_name, buf);

	cleanup();
}

// handles program memory allocation errors
void crash(char* msg)
{
	char buf[BUFF_LEN];
	snprintf(buf, BUFF_LEN, "%s: %s", program_name, msg);
	perror(buf);
	
	cleanup();

	exit(EXIT_FAILURE);
}

// returns executable program's name
char *get_program_name()
{
	return program_name;
}

// handles the allocation of shared memory for the program
void alloc_shared_memory(bool n)
{
	if ((shared_mem_key = ftok("makefile", 1)) == -1) crash("ftok");
	if ((shared_mem_id = shmget(shared_mem_key, sizeof(Util), PERMITS | (n ? (IPC_EXCL | IPC_CREAT) : 0))) == -1) crash("shmget");
	else shmptr = (Util *)shmat(shared_mem_id, NULL, 0);
}

// handles the release of shared memory segments allocated by the program
void dealloc_shared_memory()
{
	if (shmptr != NULL && shmdt(shmptr) == -1) crash("shmdt");
	if (shared_mem_id > 0 && shmctl(shared_mem_id, IPC_RMID, NULL) == -1) crash("shmctl");
}

// gets the shared memory segment of the program
Util* get_mem_seg()
{
	return shmptr;
}

// allocates shared memory for the message queues (simulated runqueues) of the OSS
void alloc_msg_q(bool n)
{
	if ((parent_msg_q_key = ftok("makefile", 2)) == -1) crash("ftok");
	if ((parent_msg_q_id = msgget(parent_msg_q_key, PERMITS | (n ? (IPC_EXCL | IPC_CREAT) : 0))) == -1) crash("msgget");

	if ((child_msg_q_key = ftok("makefile", 3)) == -1) crash("ftok");
	if ((child_msg_q_id = msgget(child_msg_q_key, PERMITS | (n ? (IPC_EXCL | IPC_CREAT) : 0))) == -1) crash("msgget");
}

// deallocates message queues first allocated by the exxecutable
void dealloc_msg_q()
{
	if (parent_msg_q_id > 0 && msgctl(parent_msg_q_id, IPC_RMID, NULL) == -1) crash("msgctl");
	if (child_msg_q_id > 0 && msgctl(child_msg_q_id, IPC_RMID, NULL) == -1) crash("msgctl");
}

// handles the delivery of a message from one process to another
int send_msg(Msg* msg, int msg_q_id, pid_t addr, char *txt, bool wait)
{
	msg -> type = addr;
	strncpy(msg -> msg_text, txt, BUFF_LEN);
	return msgsnd(msg_q_id, msg, sizeof(Msg), wait ? 0 : IPC_NOWAIT);
}

// handles the receival of a message by one process from another
int rec_msg(Msg* msg, int msg_q_id, pid_t addr, bool wait)
{
	printf("RECEIVE MESSAGE\n");
	return msgrcv(msg_q_id, msg, sizeof(Msg), addr, wait ? 0 : IPC_NOWAIT);
}

// returns the parent queue id
int get_parent_q()
{
	return parent_msg_q_id;
}

// returns the child queue id
int get_child_q()
{
	return child_msg_q_id;
}

// handles the information loggings for a process at a given moment in the system
void logger(char *fmt, ...)
{
	FILE* fp = fopen(DEF_LOG_PATH, "a+");
	if (fp == NULL) crash("fopen");

	char buf0[BUFF_LEN];
	va_list args;
	va_start(args, fmt);
	vsnprintf(buf0, BUFF_LEN, fmt, args);
	va_end(args);

	char buf1[BUFF_LEN];
	snprintf(buf1, BUFF_LEN, "%s: [%010ld:%010ld] %s\n", basename(get_program_name()), shmptr->sys_time.seconds, shmptr->sys_time.nano, buf0); // see basename(3)

	fprintf(fp, buf1);
	fprintf(stderr, buf1);

	fclose(fp);
}

// handles the call for both deallocation methods
void cleanup()
{
	dealloc_shared_memory();
	dealloc_msg_q();
}

void set_time(Time* time, long _nano)
{
	time -> seconds = 0;
	time -> nano = _nano;
	// while loop converts nano to seconds
	while (time -> nano >= 1e9)
	{
		time -> nano -= 1e9;
		time -> seconds += 1;
	} // end while conversion is possible
}

// adds some number of nanoseconds to the clock
void add_time(Time* time, long _nano)
{
	time -> nano += _nano;
	time -> nano = time -> nano * (rand() % 1000); // line is added to add illusion of overhead
	while (time -> nano >= 1e9)
	{
		time -> nano -= 1e9;
		time -> seconds += 1;
	} // end conversion loop
}

// clears a clock
void clear_time(Time* time)
{
	time -> seconds = 0;
	time -> nano = 0;
}

// copies the state of one clock to another
void copy_time(Time* src, Time* trgt)
{
	memcpy(trgt, src, sizeof(Time));
}

// subtracts some amount of time b from time a
Time subt_time(Time* a, Time* b)
{
	Time diff = { .seconds = a -> seconds - b -> seconds,
								.nano = a -> nano - b -> nano }; 
	if (diff.nano < 0)
	{
		diff.nano += 1e9;
		diff.seconds--;
	}
	return diff;
}

// displays the current time of a given clock
void curr_time(Time* time)
{
	printf("%ld:%ld\n", time -> seconds, time -> nano);
}

// swamps one timer with another given timer
void swap_time(Time* a, Time* b)
{
	long fst = a -> seconds * 1e9 + a -> nano;
	long snd = b -> seconds * 1e9 + b -> nano;
	long diff = abs(fst - snd);
	Time temp = { 0, 0 }; // temp timer for swapping
	
	add_time(&temp, diff);
	a -> seconds = temp.seconds;
	a -> nano = temp.nano;
}

// calculates the avg time of a given clock given a count of processes
void avg_time(Time* time, int count)
{
	long moment = (time -> seconds * 1e9 + time -> nano) / count;
	Time temp = { 0, 0 };

	add_time(&temp, moment);
	time -> seconds = temp.seconds;
	time -> nano = temp.nano;
}

int get_q_quantum(int q)
{
 if (q > 0 && q < (Q_SET_COUNT - 1)) return QUANTUM_BASE * pow(2, q);
 return -1;
}

int get_usr_quantum(int q)
{
	return QUANTUM_BASE / pow(2, q);
}
