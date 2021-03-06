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
#include <thread.h>

#define MAX_RUNNING_PROCS	256
#define MAX_ARGS_NUMS		 32
#define ARGS_STR_LEN		 10

struct process
{
	pid_t parent_pid;
	struct semaphore *exit_sem;		/*To protect the exit status*/
	bool exited;						/*Wheather thread is exited*/
	int exit_code;
	struct thread *self;			/*Pointer to thread structure*/
};

/*
 * sharing trapframe and semaphore with child
 * */
struct child_process_para
{
	struct trapframe *tf_child;
	struct semaphore *child_status_sem;
	struct addrspace *child_addrspace;
	struct fileDescriptorNode** fd_ptr;
};

//Global process table
struct process* pid_table[MAX_RUNNING_PROCS];
struct lock* pid_table_lock;

/*Allocate pid from global structure*/

pid_t allocate_pid(void);
void release_pid(pid_t pid);

void pid_table_init(void);
void pid_table_release(void);
void init_pid_table_entry(struct thread* new_thread);
void child_fork_entry(void *data1, unsigned long data2);

/*Upadte the parent_pid of childs of exiting process*/
void update_childs_parent(void);
/*Init first pid_table entry*/
int create_runprog_pid_table_entry(void);

void copy_file_table(struct fileDescriptorNode *src, struct fileDescriptorNode *dst);
#endif /* PROCESS_H_ */
