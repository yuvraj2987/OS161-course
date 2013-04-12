/*

 * process_syscall.c
 *
 *  Created on: Apr 9, 2013
 *      Author: trinity
 */


#include <process.h>
#include <syscall.h>
#include <limits.h>
#include <types.h>
#include <kern/errno.h>
#include <kern/syscall.h>
#include <lib.h>
#include <mips/trapframe.h>
#include <thread.h>
#include <current.h>
#include <syscall.h>
#include <addrspace.h>
#include <spl.h>
#include <copyinout.h>
#include <kern/wait.h>

/*Process table init and release called only once main->boot*/
void pid_table_init(void)
{
	pid_table_lock = lock_create("pid_table");
	int i;

	for(i=PID_MIN/*2*/; i< MAX_RUNNING_PROCS; i++)
	{
		pid_table[i] = NULL;
	}

}
//main->shutdown
void pid_table_release(void)
{
	lock_destroy(pid_table_lock);
}


/*allocate pid - thread_create*/
pid_t allocate_pid(void)
{
	int i = PID_MIN;//2
	lock_acquire(pid_table_lock);
	while(pid_table[i] != NULL && i <MAX_RUNNING_PROCS)
	{
		i++;
	}
	lock_release(pid_table_lock);

	return i;
}

void init_pid_table_entry(struct thread* new_thread)
{
	pid_t pid = new_thread->t_pid;
	pid_table[pid]->self = new_thread;
	pid_table[pid]->exited = 0;	//false
	//check
	pid_table[pid]->parent_pid = curthread->t_pid;
	pid_table[pid]->exit_code = 0;
	pid_table[pid]->exit_sem = sem_create(new_thread->t_name, 0);
}

//when to release? - in sys_waitpid
void release_pid(pid_t pid)
{
	sem_destroy(pid_table[pid]->exit_sem);
	kfree(pid_table[pid]);
	lock_acquire(pid_table_lock);
	if(pid>PID_MIN && pid <= PID_MAX)
	{
		pid_table[pid] = NULL;
	}
	lock_release(pid_table_lock);
}

/*
*
*/



int sys_fork(struct trapframe* tf_parent, int *retval)
{

	int err;
	//disable interrupts
	int s = splhigh();
	struct trapframe *tf_child = (struct trapframe *)
									kmalloc(sizeof(struct trapframe));
	struct addrspace *addrs_child = NULL;

	memcpy(tf_child, tf_parent, sizeof(struct trapframe));
	err = as_copy(curthread->t_addrspace, &addrs_child);
	if(err != 0)
	{
		kfree(tf_child);
		return err;
	}

	//decide on thread name
	struct thread *child_thread = NULL;
	err = thread_fork("Child_Thread",
			child_fork_entry, tf_child, (unsigned long)addrs_child, &child_thread);

	if(err != 0)
	{
		kfree(tf_child);
		kfree(addrs_child);
		return err;
	}
	//copy parents file table into child - Remaining
	//other book kipping stuff??
	if(child_thread->t_pid == MAX_RUNNING_PROCS)
	{
		kfree(tf_child);
		kfree(addrs_child);
		return ENPROC;
	}

	*retval = child_thread->t_pid;
	splx(s);
	return 0;
}


void child_fork_entry(void *tf, unsigned long addrs_space)
{
	/*(void)tf_child;
		(void)addrs_child;*/

	struct trapframe* tf_child = (struct trapframe *)tf;
	struct addrspace *addrs_child = (struct addrspace *)addrs_space;
	/*address space is on kernel heap
	 * mips_usermode function checks it on kernel stack
	 * therefore copy from kernel heap to stack
	 */
	struct trapframe tf_child_stack;
	memcpy(&tf_child_stack, tf_child, sizeof(struct trapframe));

	//Mark success for child
	tf_child->tf_a3 = 0;
	tf_child->tf_v0 = 0;
	//Increment prog counter
	tf_child-> tf_epc += 4;
	//load addrs_space into childs curthread ->addrspace
	curthread->t_addrspace = addrs_child;
	as_activate(curthread->t_addrspace);
	/*free the kernel heap memory??*/
	/*kfree(tf_child);
	kfree(addrs_child);*/
	mips_usermode(&tf_child_stack);

}

/*wait pid*/
int sys_waitpid(pid_t pid, userptr_t status_ptr, int options, int *ret)
{
	/*(void)pid;
	(void)status_ptr;
	(void)options;
	(void)ret;*/

	//error checking
	int *kern_status_ptr;
	int err = copyin(status_ptr, kern_status_ptr, sizeof(int));

	if(err)
		return err;
	if(options != 0)
		return EINVAL;
	if(kern_status_ptr == NULL)
		return EFAULT;
	if(pid_table[pid] == NULL)
		return ESRCH;
	if(pid_table[pid]->parent_pid != curthread->t_pid)
		return ECHILD;
	//decrement smeaphore - wait till exist
	//Set these values in thread_exit
	P(pid_table[pid]->exit_sem);
	*kern_status_ptr = pid_table[pid]->exit_code;
	err = copyout(kern_status_ptr, status_ptr, sizeof(int));
	*ret = pid;
	//cleanup
	release_pid(pid);

	return 0;
}


int sys_exit(int exit_code)
{
	//disable interupts
	int s = splhigh();
	//should mark the curthread as zombie
	thread_exit();
	//overwrite exit_code
	pid_table[curthread->t_pid]->exit_code = (_MKWVAL(exit_code) |__WEXITED);
	//To do- reset parent_pid of child threads

	//enable interrupts
	splx(s);
	return 0;
}
