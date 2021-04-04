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

typedef struct {
	Util *util;
	PCB *pcb;
	Msg msg;
} Global;

static Global* global = NULL;

int main(int argc, char** argv)
{
	global = (Global *)malloc(sizeof(Global));
	return EXIT_SUCCESS;
}

