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
/*Do not check whther pid allocation is successful
 * i.e PID_MIN<i<=PID_MAX
 * Its responsibility of calling function*/
pid_t allocate_pid(void)
{
	int i = PID_MIN;//2
	lock_acquire(pid_table_lock);
	while(i <MAX_RUNNING_PROCS && pid_table[i] != NULL)
	{
		i++;
	}
	lock_release(pid_table_lock);
	//last element is not empty
	if(pid_table[i]!=NULL)
	{
		return MAX_RUNNING_PROCS+1;
	}

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
	lock_acquire(pid_table_lock);
	if(pid_table[pid]->exit_sem != NULL)
	{
		sem_destroy(pid_table[pid]->exit_sem);
	}
	kfree(pid_table[pid]);
	if(pid>PID_MIN && pid <= PID_MAX)
	{
		pid_table[pid] = NULL;
	}
	else
	{
		kprintf("Releasing invalid pid: %d", pid);
	}
	lock_release(pid_table_lock);
}



/*
* Fork System call
*/

int sys_fork(struct trapframe* tf_parent, int *retval)
{

	int err;
	struct child_process_para *child_para = (struct child_process_para *)
											kmalloc(sizeof(struct child_process_para));
	//Copy tf and addrspace to kernel heap and semaphore to wait
	child_para->tf_child = (struct trapframe *)
											kmalloc(sizeof(struct trapframe));
	child_para->child_addrspace = NULL;
	child_para->child_status_sem =  sem_create("Child_status", 0);
	memcpy(child_para->tf_child, tf_parent, sizeof(struct trapframe));
	err = as_copy(curthread->t_addrspace, &child_para->child_addrspace);
	if(err != 0)
	{
		kfree(child_para->tf_child);
		return err;
	}

	//child thread
	struct thread *child_thread = NULL;
	//allocate process structure for child before hand
	pid_t child_pid = allocate_pid();
	if(child_pid >MAX_RUNNING_PROCS)
	{
		sem_destroy(child_para->child_status_sem);
		kfree(child_para->tf_child);
		kfree(child_para->child_addrspace);
		kfree(child_para);
		return ENPROC;
	}

	pid_table[child_pid] = (struct process*)kmalloc(sizeof(struct process));
	if(pid_table[child_pid] == NULL)
	{
		sem_destroy(child_para->child_status_sem);
		kfree(child_para->tf_child);
		kfree(child_para->child_addrspace);
		kfree(child_para);
		return ENOMEM;
	}

	//fork
	err = thread_fork("Child_Thread",
				child_fork_entry, child_para, 0, &child_thread);
	if(err != 0)
	{
		//Failed to create child
		sem_destroy(child_para->child_status_sem);
		kfree(child_para->tf_child);
		kfree(child_para->child_addrspace);
		kfree(child_para);//free
		release_pid(child_pid);
		return err;
	}
	child_thread->t_pid = child_pid;
	init_pid_table_entry(child_thread);
	//wait for child to complete tf and addrspace copying
	P(child_para->child_status_sem);

	*retval = child_thread->t_pid;
	//on success
	sem_destroy(child_para->child_status_sem);
	kfree(child_para->tf_child);
	//kfree(child_para->child_addrspace);
	kfree(child_para);

	return 0;
}

void child_fork_entry(void *parent_param, unsigned long data2)
{
	(void)data2;//unused
	struct child_process_para *param = (struct child_process_para*)parent_param;
	//copy tf on stack and addrspace
	struct trapframe tf_child_stack;
	memcpy(&tf_child_stack, param->tf_child, sizeof(struct trapframe));
	curthread->t_addrspace = param->child_addrspace;
	as_activate(curthread->t_addrspace);
	//Mark success for child
	tf_child_stack.tf_a3 = 0;
	tf_child_stack.tf_v0 = 0;
	//Increment prog counter
	tf_child_stack.tf_epc += 4;
	//Inform parent of copying
	V(param->child_status_sem);
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

	if(status_ptr == NULL)
		return EFAULT;
	if(ret == NULL)
		return EFAULT;
	int *kern_status_ptr;

	int err = copyin(status_ptr, kern_status_ptr, sizeof(int));

	//review erro checks again
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
	//Set these values in sys_exit
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
	update_childs_parent();
	pid_table[curthread->t_pid]->exit_code = (_MKWVAL(exit_code) |__WEXITED);
	pid_table[curthread->t_pid]->exited = 1;//true
	//should mark the curthread as zombie
	V(pid_table[curthread->t_pid]->exit_sem);
	thread_exit();
	return 0;
}

void update_childs_parent()
{
	pid_t exit_process = curthread->t_pid;
	pid_t new_parent =	pid_table[exit_process]->parent_pid;
	lock_acquire(pid_table_lock);
	for(int i=PID_MIN; i<PID_MAX; i++)
	{
		if(pid_table[i] != NULL && pid_table[i]->parent_pid == exit_process)
		{
			pid_table[i]->parent_pid = new_parent;
		}
	}
	lock_release(pid_table_lock);
}


int sys_getpid(int *ret)
{
	*ret = curthread->t_pid;
	return 0;
}


int tmp_sys_write(int fd, userptr_t buf, size_t nbytes)
{
	(void)fd;

	char *kern_buffer = (char *)kmalloc(nbytes);
	int err = copyin(buf, kern_buffer, nbytes);
	kern_buffer[nbytes]= '\0';
	kprintf("%s", kern_buffer);

	return err;
}

int create_pid_table_entry(void)
{
	//curthread->t_pid = allocate_pid();
	curthread->t_pid = 1;
	pid_table[curthread->t_pid] = (struct process*)kmalloc(sizeof(struct process));
	if(pid_table[curthread->t_pid] == NULL)
	{
		return ENOMEM;
	}
	init_pid_table_entry(curthread);
	pid_table[curthread->t_pid]->parent_pid = 0;
	return 0;
}
