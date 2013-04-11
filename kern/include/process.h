/*
 * process.h
 *
 *  Created on: Apr 9, 2013
 *      Author: trinity
 */

#ifndef PROCESS_H_
#define PROCESS_H_
#include <types.h>
#include <limits.h>
#include <synch.h>

#define MAX_RUNNING_PROCS	256


struct process
{
	pid_t parent_pid;
	struct semaphore *exit_sem;		/*To protect the exit status*/
	bool exited;						/*Wheather thread is exited*/
	int exit_code;
	struct thread *self;			/*Pointer to thread structure*/
};



//Global process table
struct process* pid_table[MAX_RUNNING_PROCS];
struct lock* pid_table_lock;

/*Allocate pid from global structure*/
/*Try to get return type pid_t type*/
pid_t allocate_pid(void);
void release_pid(pid_t pid);

void pid_table_init(void);
void pid_table_release(void);
void init_pid_table_entry(struct thread* new_thread);
void child_fork_entry(void *data1, unsigned long data2);

#endif /* PROCESS_H_ */
