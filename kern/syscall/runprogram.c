/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Sample/test code for running a user program.  You can use this for
 * reference when implementing the execv() system call. Remember though
 * that execv() needs to do more than this function does.
 */

#include <types.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <lib.h>
#include <thread.h>
#include <current.h>
#include <addrspace.h>
#include <vm.h>
#include <vfs.h>
#include <syscall.h>
#include <test.h>
#include <process.h>
#include <copyinout.h>
/*
 * Load program "progname" and start running it in usermode.
 * Does not return except on error.
 *
 * Calls vfs_open on progname and thus may destroy it.
 */

int
runprogram(char *progname, char **args)
{
	struct vnode *v;
	vaddr_t entrypoint, stackptr;
	int result;
	size_t progname_len = strlen(progname);

	if(progname_len == 0)
			return EINVAL;
	if(progname[progname_len]== '/')
			return EISDIR;

	/* Open the file. */

	result = vfs_open(progname, O_RDONLY, 0, &v);
	if (result) {
		return result;
	}

	/*Copy args argument pointers*/
	userptr_t kargs_ptr[MAX_ARGS_NUMS];
	int argc_count = 0;
	do
	{
		//error checking
		memcpy(&kargs_ptr[argc_count], &args[argc_count], sizeof(userptr_t));
		argc_count++;
	}while(kargs_ptr[argc_count-1] != NULL);

	int kargc = argc_count-1; //number of arguments
	if(kargc > MAX_ARGS_NUMS)
		return E2BIG;

	/* Copy argument string */
	size_t ptr_size = sizeof(userptr_t);
	char* kargv[kargc];
	char* kargv_aligned[kargc];
	size_t aligned_len_arr[kargc];
	for(int i=0; i<kargc; i++)
	{
		size_t len;
		//release it at the end
		kargv[i] = (char *) kmalloc(NAME_MAX);
		//err = copyinstr(kargs_ptr[i], kargv[i], NAME_MAX, &len);
		strcpy(kargv[i], (const char*)kargs_ptr[i]);

		//len includes the null terminator
		len = strlen(kargv[i])+1;
		size_t not_align = (len%ptr_size);
		size_t pad_len = 0;
		if(not_align)
		{
			pad_len = ptr_size - (len%ptr_size);
		}
		size_t aligned_len = len+pad_len;
		kargv_aligned[i] = (char *) kmalloc(aligned_len);
		strcpy(kargv_aligned[i], kargv[i]);
		aligned_len_arr[i] = aligned_len;
	}



	/* We should be a new thread. */
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

	/*Initi pid_table for 1st user process*/
	pid_table[1]->self = curthread;
	curthread->t_pid = 1;

	/*
	 * Create kernel buffer
	 * */

	size_t arg_size = (argc_count*ptr_size);//user
	for(int i =0; i<kargc; i++)
	{
		arg_size += aligned_len_arr[i];
	}
	userptr_t buffer[argc_count];
	vaddr_t buf_base_addrs = stackptr - (vaddr_t)arg_size;
	vaddr_t buf_str_addrs  = buf_base_addrs + ptr_size*argc_count;

	for(int i =0; i<kargc; i++)
	{
		buffer[i] = (userptr_t)buf_str_addrs;
		buf_str_addrs += aligned_len_arr[i];
	}

	buffer[kargc] = NULL;
	result = copyout(buffer, (userptr_t)buf_base_addrs, ptr_size*argc_count);
	if(result)
		return result;

	vaddr_t stack_str_addrs  = buf_base_addrs + ptr_size*argc_count;

	for(int i =0; i<kargc; i++)
	{
		size_t actual;
		result = copyoutstr(kargv_aligned[i], (userptr_t)stack_str_addrs, aligned_len_arr[i], &actual);
		if(result)
			return result;
		stack_str_addrs +=aligned_len_arr[i];
	}
	//release kargv
	for(int i =0; i<kargc; i++)
	{
		kfree(kargv[i]);
		kfree(kargv_aligned[i]);
	}

	/* Warp to user mode. */
	//enter_new_process(0 /*argc*/, NULL /*userspace addr of argv*/,
	//		  stackptr, entrypoint);
	enter_new_process(kargc /*argc*/, (userptr_t)buf_base_addrs /*userspace addr of argv*/,
				buf_base_addrs-16/*Margin of 16*/, entrypoint);
	/* enter_new_process does not return. */
	panic("enter_new_process returned\n");
	return EINVAL;
}

