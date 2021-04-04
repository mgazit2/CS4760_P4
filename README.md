Matan Gazit
Assignment 4
CS 4760
04/04/21

Make:

	To make this project, type 'make' into the command line while in the project directory
	To clean up the project directory after execution, simply type 'make clean' into the command line

Purpose:

	The goal of this homework is to learn about process scheduling inside an operating system. You will work on the specified scheduling algorithm and simulate its performance.

Task:

	In this project, you will simulate the process scheduling part of an operating system.
	You will implement time-based scheduling, ignoring almost every other aspect of the os.
	In particular, this project will involve a main executable managing the execution of concurrent user processes just like a scheduler/dispatcher does in an os. 
	You may use message queues or semephores for synchronization, depending on your choice.

Invoking the solution:

	oss [-h] [-s t] [-l f]

	Run ./oss -h for usage options and help

References:

	The following repositories were used to as references when costructing this project:

		https://github.com/Gaurav-Gilalkar/process-scheduling-algorithms
		https://github.com/JasonYao/Operating-Systems-Process-Scheduler
		https://gist.github.com/pallabpain/2cee67fba040b442e929
		https://github.com/aroques/process-scheduling
		https://github.com/jaredible/CS4760-Project-4/blob/master/oss.c

Issues:

	I was unable to make it such that the OSS stops spawning processes after 3 real time seconds, because of this, the proegram will only spawn 20 processes before proving that it can terminate
	This was done to show that though this part of the program does not function as needed, the program itself can execute and terminate cleanly.
