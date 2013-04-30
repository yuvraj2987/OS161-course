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
#include <kern/fcntl.h>
#include <vfs.h>


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
	if(pid <= PID_MAX && pid_table[pid]!=NULL)
	{

		if(pid_table[pid]->exit_sem != NULL)
		{
			sem_destroy(pid_table[pid]->exit_sem);
		}
		kfree(pid_table[pid]);
		pid_table[pid] = NULL;
	}
	else
	{
		kprintf("Releasing invalid pid: %d", pid);
	}
	lock_release(pid_table_lock);
}

int create_runprog_pid_table_entry(void)
{
	//runprog pid = 1 always
	pid_t pid = 1;
	pid_table[pid] = (struct process*)kmalloc(sizeof(struct process));
	if(pid_table[pid] == NULL)
	{
		return ENOMEM;
	}
	pid_table[pid]->exited = 0;	//false


	pid_table[pid]->exit_code = 0;
	pid_table[pid]->exit_sem = sem_create("runprogram", 0);
	pid_table[pid]->parent_pid = 0;
	return 0;
}

void copy_file_table(struct fileDescriptorNode *src, struct fileDescriptorNode *dst)
{
	//copy all MAX_FILE_NUMBER
	for(int i =0; i<MAX_NUMBER_OF_FILES; i++)
	{
		dst[i] = src[i];
	}
}



/*
 * Fork System call
 */

int sys_fork(struct trapframe* tf_parent, int *retval)
{

	int err;
	struct child_process_para *child_para = (struct child_process_para *)
																	kmalloc(sizeof(struct child_process_para));
	if(child_para == NULL)
		return ENOMEM;
	//Copy tf and addrspace to kernel heap and semaphore to wait
	child_para->tf_child = (struct trapframe *)
																	kmalloc(sizeof(struct trapframe));
	if(child_para->tf_child == NULL)
	{
		kfree(child_para);
		return ENOMEM;
	}

	child_para->child_addrspace = NULL;
	child_para->child_status_sem =  sem_create("Child_status", 0);
	if(child_para->child_status_sem == NULL)
	{
		kfree(child_para->tf_child);
		kfree(child_para);
		return ENOMEM;
	}
	memcpy(child_para->tf_child, tf_parent, sizeof(struct trapframe));
	err = as_copy(curthread->t_addrspace, &child_para->child_addrspace);
	if(err != 0)
	{
		kfree(child_para->tf_child);
		sem_destroy(child_para->child_status_sem);
		kfree(child_para);
		return err;
	}
	//set ptr to curthead->fileDescriptor array
	child_para->fd_ptr = curthread->fileDescriptor;

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
		/*pid is not yet setup*/
		//release_pid(child_pid);
		return err;
	}
	//Overwrite default pid
	child_thread->t_pid = child_pid;
	init_pid_table_entry(child_thread);
	//wait for child to complete tf and addrspace copying
	P(child_para->child_status_sem);

	//*retval = child_thread->t_pid;
	*retval = child_pid;
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
	//copy parent process file table
	for(int i=0; i < MAX_NUMBER_OF_FILES; i++)
	{
		curthread->fileDescriptor[i] = *param->fd_ptr++;
		//Increment ref count
		if(curthread->fileDescriptor[i] != NULL)
		{
			lock_acquire(curthread->fileDescriptor[i]->fDNLock);
			curthread->fileDescriptor[i]->refCount+=1;
			lock_release(curthread->fileDescriptor[i]->fDNLock);
		}
	}
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
/**pid_t
waitpid(pid_t pid, int *status, int options);**/
int sys_waitpid(pid_t pid, userptr_t status_ptr, int options, int *ret)
{
	/*(void)pid;
	(void)status_ptr;
	(void)options;
	(void)ret;*/

	//error checking
	if((pid < 1) || (pid >= MAX_RUNNING_PROCS) )
		return EINVAL;
	/*wiating on self*/
	if(pid == curthread->t_pid)
		return EAGAIN;
	/*wait on parent - not for kernel thread i.e curthread->t_pid =0*/
	if(curthread->t_pid != 0)
	{
		if(pid_table[curthread->t_pid]->parent_pid == pid)
			return EAGAIN;
	}


	if(pid != 1)
	{


		if(status_ptr == NULL)
			return EFAULT;
		if((unsigned long)status_ptr & (sizeof(userptr_t)-1))
			return EFAULT;

		if(ret == NULL)
			return EFAULT;
		int kern_status;

		int err = copyin(status_ptr, &kern_status, sizeof(int));


		//review erro checks again
		if(err)
			return err;
		if(options != 0)
			return EINVAL;
		if(&kern_status == NULL)
			return EFAULT;
		if(pid_table[pid] == NULL)
			return ESRCH;
		if(pid_table[pid]->parent_pid != curthread->t_pid)
			return ECHILD;
		//decrement smeaphore - wait till exist
		//Set these values in sys_exit
		P(pid_table[pid]->exit_sem);
		kern_status = pid_table[pid]->exit_code;
		err = copyout(&kern_status, status_ptr, sizeof(int));
		*ret = pid;
		//cleanup
		release_pid(pid);
	}//wait is not on the first process
	else
	{
		(void)status_ptr;
		(void)options;
		(void)ret;
		if(pid_table[pid] == NULL)
			return ESRCH;
		P(pid_table[pid]->exit_sem);
		release_pid(pid);
	}

	return 0;
}


int sys_exit(int exit_code)
{
	update_childs_parent();
	pid_table[curthread->t_pid]->exit_code = (_MKWVAL(exit_code) |__WEXITED);
	pid_table[curthread->t_pid]->exited = 1;//true
	//should mark the curthread as zombie
	V(pid_table[curthread->t_pid]->exit_sem);
	//In thread exit- file_table null and sys_close on valid descriptor
	thread_exit();
	return 0;
}

void update_childs_parent()
{
	pid_t exit_process = curthread->t_pid;
	//pid == 0 nothing is defined
	if(exit_process == 0)
		return;
	pid_t new_parent =	pid_table[exit_process]->parent_pid;
	lock_acquire(pid_table_lock);
	for(int i=PID_MIN; i<MAX_RUNNING_PROCS; i++)
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


/*
int sys_exev(char *progname, char **args)
 */
int sys_execv(const_userptr_t  progname, userptr_t *args)
{
	if(progname == NULL || args==NULL)
		return EFAULT;
	int err;
	/*Copy Progname to kernel stack*/
	char k_progname[PATH_MAX];
	size_t progname_actual_len;
	err = copyinstr((userptr_t)progname, k_progname, NAME_MAX, &progname_actual_len);
	if(err)
	{
		return err;
	}

	//Incase of null
	if(progname_actual_len <= 1)
		return EINVAL;
	if(k_progname[progname_actual_len-1] == '/')
		return EISDIR;
	/*
	 * Copy args string pointer list
	 *
	 * */

	//copy args pointers
	const_userptr_t kargs_ptr[MAX_ARGS_NUMS];
	int argc_count = 0;
	do
	{
		err = copyin((userptr_t)&args[argc_count], &kargs_ptr[argc_count], sizeof(userptr_t));
		if(err)
			return EFAULT;

		argc_count++;
	}while(kargs_ptr[argc_count-1] != NULL);

	//int kargc = argc_count+1;
	int kargc = argc_count-1; //number of arguments
	if(kargc > MAX_ARGS_NUMS)
		return E2BIG;
	//copy argument string
	size_t ptr_size = sizeof(userptr_t);
	char* kargv[kargc];
	char* kargv_aligned[kargc];
	size_t aligned_len_arr[kargc];
	for(int i=0; i<kargc; i++)
	{
		size_t len;
		//release it at the end
		kargv[i] = (char *) kmalloc(NAME_MAX);
		err = copyinstr(kargs_ptr[i], kargv[i], NAME_MAX, &len);
		if(err)
			return err;
		//len includes the null terminator
		size_t not_align = (len%ptr_size);
		size_t pad_len = 0;
		if(not_align)
		{
			pad_len = ptr_size - (len%ptr_size);
		}
		size_t aligned_len = len+pad_len;
		kargv_aligned[i] = (char *) kmalloc(aligned_len);
		//memset(kargv_aligned, 0, aligned_len);
		//Init to null
		/*for(unsigned int ch=0; ch<aligned_len; ch++)
		{
			kargv_aligned[ch] = 0;
		}*/
		strcpy(kargv_aligned[i], kargv[i]);
		aligned_len_arr[i] = aligned_len;
	}


	/**
	 * Steps from runprogram
	 * */
	struct vnode *v;
	vaddr_t entrypoint, stackptr;
	int result;


	/* Open the file. */

	result = vfs_open(k_progname, O_RDONLY, 0, &v);
	if (result) {
		return result;
	}

	/* We should be a new thread.  not for execv*/
	//KASSERT(curthread->t_addrspace == NULL);

	/* Create a new address space. */
	curthread->t_addrspace = as_create();
	if (curthread->t_addrspace==NULL) {
		vfs_close(v);
		return ENOMEM;
	}

	/* Activate it. */
	as_activate(curthread->t_addrspace);

	/* Load the executable. */
	result = load_elf(v, &entrypoint);
	if (result) {
		/* thread_exit destroys curthread->t_addrspace */
		vfs_close(v);
		return result;
	}

	/* Done with the file now. */
	vfs_close(v);

	/* Define the user stack in the address space */
	result = as_define_stack(curthread->t_addrspace, &stackptr);
	if (result) {
		/* thread_exit destroys curthread->t_addrspace */
		return result;
	}

	/*
	 * create a kernel buffer with args strings and pointers
	 * **/
	//kargc = argc_count -1
	//for kargc = 1 arg_size = 8 for 0th argument and for null
	size_t arg_size = (argc_count*ptr_size);//user
	for(int i =0; i<kargc; i++)
	{
		//size_t str_len = strlen(kargv[i])+1;
		//size_t pad_len = str_len%4;//correct
		//arg_size 	  += str_len+pad_len+1;
		arg_size += aligned_len_arr[i];

	}

	//char *buffer[4*argc_count];
	//userptr_t buffer[ptr_size*argc_count];
	userptr_t buffer[argc_count];
	/*
	for(int i=0; i<4*argc_count; i++)
	{
		buffer[i] = 0;
	}
	*/

	vaddr_t buf_base_addrs = stackptr - (vaddr_t)arg_size;
	vaddr_t buf_str_addrs  = buf_base_addrs + ptr_size*argc_count;
	//vaddr_t buf_str_addrs  = buf_base_addrs + 4*argc_count+1;
	//vaddr_t buf_ptr_addrs  = buf_base_addrs;
	for(int i =0; i<kargc; i++)
	{
		/*
		buffer[i*4] = (char *)buf_str_addrs;
		size_t str_len = strlen(kargv[i])+1;
		size_t pad_len = str_len%4;//coorrect
		//buf_str_addrs += str_len+pad_len+1;
		buf_str_addrs += str_len+pad_len;
		*/
		buffer[i] = (userptr_t)buf_str_addrs;
		buf_str_addrs += aligned_len_arr[i];
	}

	buffer[kargc] = NULL;
	//copy only char pointers first
	err = copyout(buffer, (userptr_t)buf_base_addrs, 4*argc_count);
	if(err)
		return err;

	//vaddr_t stack_str_addrs  = buf_base_addrs + 4*argc_count+1;
	vaddr_t stack_str_addrs  = buf_base_addrs + ptr_size*argc_count;
	for(int i =0; i<kargc; i++)
	{
		/*
		size_t str_len = strlen(kargv[i])+1;
		size_t pad_len = str_len%4;
		size_t actual;
		err = copyoutstr(kargv[i], (userptr_t)stack_str_addrs, str_len, &actual);
		//stack_str_addrs +=str_len+pad_len+1;
		stack_str_addrs +=str_len+pad_len;
		*/
		size_t actual;
		err = copyoutstr(kargv_aligned[i], (userptr_t)stack_str_addrs, aligned_len_arr[i], &actual);
		if(err)
			return err;
		stack_str_addrs +=aligned_len_arr[i];
	}

	//release kargv
	for(int i =0; i<kargc; i++)
	{
		kfree(kargv[i]);
		kfree(kargv_aligned[i]);
	}
	/*
	 * Copied from runprogram steps
	 * */

	/* Warp to user mode. */

	enter_new_process(kargc /*argc*/, (userptr_t)buf_base_addrs /*userspace addr of argv*/,
			buf_base_addrs-16/*Margin of 16*/, entrypoint);

	/* enter_new_process does not return. */
	panic("enter_new_process returned\n");
	return EINVAL;

	return 0;
}
