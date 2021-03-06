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

#include <uio.h>

#ifndef _SYSCALL_H_
#define _SYSCALL_H_


struct trapframe; /* from <machine/trapframe.h> */

/*
 * The system call dispatcher.
 */

void syscall(struct trapframe *tf);

/*
 * Support functions.
 */

/* Helper for fork(). You write this. */
void enter_forked_process(struct trapframe *tf);

/* Enter user mode. Does not return. */
void enter_new_process(int argc, userptr_t argv, vaddr_t stackptr,
		       vaddr_t entrypoint);


/*
 * Prototypes for IN-KERNEL entry points for system call implementations.
 */

int sys_reboot(int code);
int sys___time(userptr_t user_seconds, userptr_t user_nanoseconds);
/*ASST2*/
int sys_initialize_fd(void);
int sys_open(const_userptr_t filePath, int flag, int * retval);
int sys_close(int fd, int *retval);
int sys_read(int fd, const_userptr_t buffer, size_t bufferLen, int *retval);
int sys_write(int fd, const_userptr_t buffer, size_t bufferLen, int *retval);
int sys_dup2(int oldfd, int newfd, int *retval);
int sys_lseek(int fd, off_t location, int whence, int *retval, int *retval_v1);
int sys_chdir(const_userptr_t newPath, int * retval);
int sys__getcwd(const_userptr_t buffer, int bufferLen, int *retval);
void my_uio_kinit(struct iovec *iov, struct uio *u,
		void *kbuf, size_t len, off_t pos, enum uio_rw rw);
int my_copycheck(const_userptr_t userptr, size_t len, size_t *stoplen);

//Process system calls
int sys_fork(struct trapframe *tf, int *ret);
int sys_waitpid(pid_t pid, userptr_t status_ptr, int options ,int *ret);
int sys_exit(int exit_code);
int sys_getpid(int *ret);
//int sys_execv(char *progname, char **args);
int sys_execv(const_userptr_t progname, userptr_t *args);
int tmp_sys_write(int fd, userptr_t buf, size_t nbytes);

#endif /* _SYSCALL_H_ */
