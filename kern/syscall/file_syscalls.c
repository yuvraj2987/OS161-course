/* ASST2*/
#include <types.h>
#include <limits.h>
#include <syscall.h>
#include <kern/errno.h>
#include <lib.h>
#include <array.h>
#include <cpu.h>
#include <current.h>
#include <synch.h>
#include <clock.h>
#include <copyinout.h>
#include <vnode.h>
#include <vfs.h>
#include <kern/fcntl.h>
#include <uio.h>
#include <kern/seek.h>
#include <kern/stat.h>


int sys_initialize_fd()
{
	int count =0;

	if(curthread->fileDescriptor[0]==NULL)
	{
		count =0;
		struct vnode * vn0;
		const char * console = "con:";
			char* kname = kstrdup(console);

		int err0 = vfs_open(kname, O_RDWR, 0, &vn0);
		if(err0==0)
		{
			curthread->fileDescriptor[count] = (struct fileDescriptorNode *)kmalloc(sizeof(struct fileDescriptorNode));
			curthread->fileDescriptor[count]->vn = vn0;
			curthread->fileDescriptor[count]->fDNLock = lock_create((char *)"con:0");
			curthread->fileDescriptor[count]->offset = 0;
			curthread->fileDescriptor[count]->refCount = 0;
			curthread->fileDescriptor[count]->flag = O_RDWR;
			curthread->fileDescriptor[count]->filePath = (char *)"con:";
		}
	}

	if(curthread->fileDescriptor[1]==NULL)
	{
		const char * console = "con:";
			char* kname = kstrdup(console);
		count = 1;
		struct vnode * vn1;
		int err1 = vfs_open(kname, O_RDWR, 0, &vn1);
		if(err1==0)
		{
			curthread->fileDescriptor[count] = (struct fileDescriptorNode *)kmalloc(sizeof(struct fileDescriptorNode));
			curthread->fileDescriptor[count]->vn = vn1;
			curthread->fileDescriptor[count]->fDNLock = lock_create((char *)"con:1");
			curthread->fileDescriptor[count]->offset = 0;
			curthread->fileDescriptor[count]->refCount = 0;
			curthread->fileDescriptor[count]->flag = O_RDWR;
			curthread->fileDescriptor[count]->filePath = (char *)"con:";
		}
	}


	if(curthread->fileDescriptor[2]==NULL)
	{
		const char * console = "con:";
		char* kname = kstrdup(console);
		count =2;
		struct vnode * vn2;
		int err2 = vfs_open(kname, O_RDWR, 0, &vn2);
		if(err2==0)
		{
			curthread->fileDescriptor[count] = (struct fileDescriptorNode *)kmalloc(sizeof(struct fileDescriptorNode));
			curthread->fileDescriptor[count]->vn = vn2;
			curthread->fileDescriptor[count]->fDNLock = lock_create((char *)"con:2");
			curthread->fileDescriptor[count]->offset = 0;
			curthread->fileDescriptor[count]->refCount = 0;
			curthread->fileDescriptor[count]->flag = O_RDWR;
			curthread->fileDescriptor[count]->filePath = (char *)"con:";
		}
	}
	/*int count = 0;
	int err = -1;
	int flag = O_WRONLY;
	//mode_t mode = 0664;
	struct lock * sys_initialize_lock = lock_create("sys_initialize_lock");

	for(count =0; count<3; count++)
	{
		if(curthread->fileDescriptor[count]==NULL)
		{
			if(count==0)
				flag = O_RDONLY;
			else
				flag = O_WRONLY;

			struct vnode * vn ;//= (struct vnode *)kmalloc(size of(struct vnode));

			lock_acquire(sys_initialize_lock);
			err = vfs_open((char *)"con:", flag, 0, &vn);
			lock_release(sys_initialize_lock);

			if(err==0)
			{
				curthread->fileDescriptor[count] = (struct fileDescriptorNode *)kmalloc(sizeof(struct fileDescriptorNode));
				curthread->fileDescriptor[count]->vn = vn;
				curthread->fileDescriptor[count]->fDNLock = lock_create((char *)"con:");
				curthread->fileDescriptor[count]->offset = 0;
				curthread->fileDescriptor[count]->refCount = 0;
				curthread->fileDescriptor[count]->flag = flag;
				curthread->fileDescriptor[count]->filePath = (char *)"con:";
			}
			else
			{
				kfree(vn);
			}
		}
	}*/
	return 0;
}

/* TTD:
 * add a check to verify that fileDescriptor[count] return by malloc is not NULL
 * what should sys_open return on success and failure
 * change offset when flag is APPEND*/
int sys_open(const_userptr_t filePath, int flag, int *retval)
{
	*retval = -1;

	sys_initialize_fd();
	mode_t mode = 0664;

	if(filePath==NULL)
		return EFAULT;

	vaddr_t filePathAddress = (vaddr_t)filePath;
	if(filePathAddress==0x40000000 || filePathAddress>=USERSPACETOP)
		return EFAULT;

	//size_t filePathLen = strlen((char *)filePath);
	char * filePathPointer = (char *)kmalloc(PATH_MAX*(sizeof(char)));
	size_t filePathGot;
	int err = copyinstr(filePath, (char *)filePathPointer, (size_t)PATH_MAX, &filePathGot);

	if (err != 0)
		return err;

	//is valid flag
	{
		int flag1 = flag;
		flag1 = flag1>>7;
		if(flag1>0)
			return EINVAL;
	}

	struct lock * sys_initialize_lock = lock_create("sys_initialize_lock");
	struct vnode * vn;// = (struct vnode *)kmalloc(sizeof(struct vnode));
	lock_acquire(sys_initialize_lock);
	err = vfs_open(filePathPointer, flag, mode, &vn);
	lock_release(sys_initialize_lock);

	int count = 0;
	if(err==0)
	{
		//Find the location is fileDescriptor  table
		for (count=3; count<MAX_NUMBER_OF_FILES; count++)
		{
			if(curthread->fileDescriptor[count] == NULL)
			{
				curthread->fileDescriptor[count] = (struct fileDescriptorNode *)kmalloc(sizeof(struct fileDescriptorNode));
				curthread->fileDescriptor[count]->vn = vn;
				curthread->fileDescriptor[count]->fDNLock = lock_create((char *)filePath);
				curthread->fileDescriptor[count]->offset = 0;
				curthread->fileDescriptor[count]->refCount = 0;
				curthread->fileDescriptor[count]->flag = flag;
				curthread->fileDescriptor[count]->filePath = (char *)filePath;
				break;
			}
		}
		if(count==MAX_NUMBER_OF_FILES)
		{
			return EMFILE;
		}
	}
	else
	{
		//kfree(vn);
		return err;
	}

	*retval = count;
	return err;
}


int sys_close(int fd, int *retval)
{
	*retval = -1;
	if(fd<0 || fd>=MAX_NUMBER_OF_FILES)
		return EBADF;

	if(curthread->fileDescriptor[fd]==NULL)
		return EBADF;

	lock_acquire(curthread->fileDescriptor[fd]->fDNLock);
	if(curthread->fileDescriptor[fd]->refCount>0)
	{
		curthread->fileDescriptor[fd]->refCount--;
		lock_release(curthread->fileDescriptor[fd]->fDNLock);
		curthread->fileDescriptor[fd] = NULL;
	}
	else
	{
		vfs_close(curthread->fileDescriptor[fd]->vn);
		//kfree(curthread->fileDescriptor[fd]->vn);
		lock_release(curthread->fileDescriptor[fd]->fDNLock);
		lock_destroy(curthread->fileDescriptor[fd]->fDNLock);
	    kfree(curthread->fileDescriptor[fd]);
	    curthread->fileDescriptor[fd] = NULL;
	}

    *retval=0;
	return 0;
}

int sys_read(int fd, const_userptr_t buffer, size_t bufferLen, int *retval)
{
	*retval = -1;
	sys_initialize_fd();

	if(fd<0 || fd>=MAX_NUMBER_OF_FILES )
		return EBADF;

	if(curthread->fileDescriptor[fd]==NULL)
		return EBADF;

	if(buffer==NULL)
		return EFAULT;

	if(bufferLen<=0)
		return EINVAL;

	size_t stoplen;
	int err1 = my_copycheck(buffer, bufferLen, &stoplen);
	if(err1!=0)
		return EFAULT;

	bufferLen = stoplen;

	int how = curthread->fileDescriptor[fd]->flag & O_ACCMODE;
	switch (how)
	{
		case O_RDWR:
	    case O_RDONLY:
	    	break;

	    case O_WRONLY:
	    	return EBADF;

	    default:
	    	return EINVAL;
	}


	int err = -1;
	//if(curthread->fileDescriptor[fd]->flag==O_RDONLY || curthread->fileDescriptor[fd]->flag==O_RDWR)
	{
		/*void * kernelBuffer = (void *)kmalloc(bufferLen);
		if(kernelBuffer==NULL)
			return ENOMEM;

		err = copyin((const_userptr_t)buffer, kernelBuffer, bufferLen);
		if(err !=0)
			return err;*/

		struct iovec iovec_read;
		struct uio uio_read;
		my_uio_kinit(&iovec_read, &uio_read, (void *)buffer, bufferLen,
				curthread->fileDescriptor[fd]->offset, UIO_READ);

		lock_acquire(curthread->fileDescriptor[fd]->fDNLock);
		err = VOP_READ(curthread->fileDescriptor[fd]->vn, &uio_read);
		lock_release(curthread->fileDescriptor[fd]->fDNLock);

		if(err !=0)
			return err;

		*retval = bufferLen - uio_read.uio_resid;
		curthread->fileDescriptor[fd]->offset = uio_read.uio_offset;
		//kfree(&iovec_read);
		//kfree(&uio_read);

	}
	//else
	{
		//return EACCES;
	}

	return 0;

}


int sys_write(int fd, const_userptr_t buffer, size_t bufferLen, int *retval)
{
	sys_initialize_fd();

	*retval = -1;
	if(fd<0 || fd>=MAX_NUMBER_OF_FILES )
		return EBADF;

	if (curthread->fileDescriptor[fd]==NULL)
			return EBADF;

	if(buffer==NULL)
		return EFAULT;

	if(bufferLen<=0)
		return EINVAL;

	size_t stoplen;
	int err1 = my_copycheck(buffer, bufferLen, &stoplen);
	if(err1!=0)
		return EFAULT;

	bufferLen = stoplen;

	int how = curthread->fileDescriptor[fd]->flag & O_ACCMODE;

	switch (how)
	{
		case O_RDWR:
		case O_WRONLY:
	    	break;

	    case O_RDONLY:
		   	return EBADF;

		default:
		  	return EINVAL;
	}


	int err = -1;
	//if(curthread->fileDescriptor[fd]->flag==O_WRONLY || curthread->fileDescriptor[fd]->flag==O_RDWR)
	{
		struct iovec iovec_write;
		struct uio uio_write;
		my_uio_kinit(&iovec_write, &uio_write, (void *)buffer, bufferLen,
						curthread->fileDescriptor[fd]->offset, UIO_WRITE);

		lock_acquire(curthread->fileDescriptor[fd]->fDNLock);
		err = VOP_WRITE(curthread->fileDescriptor[fd]->vn, &uio_write);
		lock_release(curthread->fileDescriptor[fd]->fDNLock);

		if(err !=0)
			return err;

		*retval = bufferLen - uio_write.uio_resid;
		curthread->fileDescriptor[fd]->offset = uio_write.uio_offset;
		//kfree(&iovec_write);
		//kfree(&uio_write);
	}
	//else
	{
		//return EACCES;
	}
	return 0;
}

int sys_dup2(int oldfd, int newfd, int *retval)
{
	*retval = -1;
	if(oldfd<0 || newfd<0 || oldfd>=MAX_NUMBER_OF_FILES)
	{
		return EBADF;
	}

	if(newfd>=MAX_NUMBER_OF_FILES)
		return EBADF;

	if(curthread->fileDescriptor[oldfd]==NULL)
	{
		return EBADF;
	}

	if(oldfd == newfd)
	{
		*retval = newfd;
		return 0;
	}

	if(curthread->fileDescriptor[newfd]!=NULL)
	{
		int err = sys_close(newfd, retval);
		if (err!=0)
			return EBADF;
	}

	lock_acquire(curthread->fileDescriptor[oldfd]->fDNLock);
	curthread->fileDescriptor[oldfd]->refCount++;
	curthread->fileDescriptor[newfd] = curthread->fileDescriptor[oldfd];
	lock_release(curthread->fileDescriptor[oldfd]->fDNLock);

	*retval = newfd;
	return 0;
}

int sys_lseek(int fd, off_t location, int whence, int *retval, int *retval_v1)
{
	*retval =-1;
	if(fd<0 || fd>=MAX_NUMBER_OF_FILES)
		return EBADF;

	if(curthread->fileDescriptor[fd]==NULL)
		return EBADF;

	sys_initialize_fd();
	off_t newOffest;
	int err = -1;

	switch(whence)
	{
		case SEEK_SET:

			newOffest = location;

			lock_acquire(curthread->fileDescriptor[fd]->fDNLock);
			err = VOP_TRYSEEK(curthread->fileDescriptor[fd]->vn, newOffest);
			lock_release(curthread->fileDescriptor[fd]->fDNLock);

		break;

		case SEEK_CUR:

			newOffest = curthread->fileDescriptor[fd]->offset + location;

			lock_acquire(curthread->fileDescriptor[fd]->fDNLock);
			err = VOP_TRYSEEK(curthread->fileDescriptor[fd]->vn, newOffest);
			lock_release(curthread->fileDescriptor[fd]->fDNLock);

		break;

		case SEEK_END:
		{
			struct stat fileStat;

			lock_acquire(curthread->fileDescriptor[fd]->fDNLock);
			err = VOP_STAT(curthread->fileDescriptor[fd]->vn, &fileStat);

			if(err!=0)
			{
				lock_release(curthread->fileDescriptor[fd]->fDNLock);
				return err;
			}

			newOffest = fileStat.st_size + location;
			err = VOP_TRYSEEK(curthread->fileDescriptor[fd]->vn, newOffest);
			lock_release(curthread->fileDescriptor[fd]->fDNLock);
		}
		break;

		default:
			return EINVAL;
	}

	if(newOffest<0)
		return EINVAL;

	if(err==0)
	{
		curthread->fileDescriptor[fd]->offset = newOffest;
		off_t MSB = 0;
		MSB = MSB | newOffest>>32;
		*retval = MSB;
		*retval_v1 = newOffest;
	}

	return err;
}

int sys_chdir(const_userptr_t newPath, int * retval)
{
	*retval = -1;

	if(newPath==NULL)
		return EFAULT;

	vaddr_t filePathAddress = (vaddr_t)newPath;
	if(filePathAddress==0x40000000 || filePathAddress>=USERSPACETOP)
		return EFAULT;

	int err = -1;
	//size_t newPathLen = strlen((char *)newPath);
	char * newPathPointer = (char *)kmalloc(PATH_MAX*(sizeof(char)));
	size_t newPathGot;
	err = copyinstr(newPath, (char *)newPathPointer, (size_t)PATH_MAX, &newPathGot);
	if (err!=0)
		return err;

	err = vfs_chdir(newPathPointer);

	if(err==0)
		*retval = 0;

	//kfree(newPathPointer);
	//newPathPointer = NULL;
	return err;
}

int sys__getcwd(const_userptr_t buffer, int bufferLen, int *retval)
{
	*retval = -1;

	size_t stoplen;
	int err1 = my_copycheck(buffer, bufferLen, &stoplen);
	if(err1!=0 || stoplen!=(size_t)bufferLen)
		return EFAULT;

	struct iovec iovec__getcwd;
	struct uio uio__getcwd;
	my_uio_kinit(&iovec__getcwd, &uio__getcwd, (void *)buffer, bufferLen,
			0, UIO_READ);

	int err = vfs_getcwd(&uio__getcwd);

	if(err !=0)
		return err;

	*retval = bufferLen - uio__getcwd.uio_resid;
	return err;

	/*struct iovec iovec__getcwd;
	struct uio uio__getcwd;
	void* path = (void*)kmalloc(bufferLen);
	uio_kinit(&iovec__getcwd, &uio__getcwd, (void *)path, bufferLen,
				0, UIO_READ);
	int err = vfs_getcwd(&uio__getcwd);

	if(err==0)
	{
		*retval = bufferLen - uio__getcwd.uio_resid;
		copyout(path, (userptr_t)buffer, bufferLen);
	}

	return err;*/
}

void my_uio_kinit(struct iovec *iov, struct uio *u,
	  void *ubuf, size_t len, off_t pos, enum uio_rw rw)
{
	iov->iov_ubase = ubuf;
	iov->iov_len = len;

	u->uio_iov = iov;
	u->uio_iovcnt = 1;
	u->uio_offset = pos;
	u->uio_resid = len;
	u->uio_segflg = UIO_USERSPACE;
	u->uio_rw = rw;
	u->uio_space = curthread->t_addrspace;
}

int my_copycheck(const_userptr_t userptr, size_t len, size_t *stoplen)
{
	vaddr_t bot, top;

	*stoplen = len;

	bot = (vaddr_t) userptr;
	top = bot+len-1;

	if (top < bot) {
		/* addresses wrapped around */
		return EFAULT;
	}

	if (bot >= USERSPACETOP) {
		/* region is within the kernel */
		return EFAULT;
	}

	if (top >= USERSPACETOP) {
		/* region overlaps the kernel. adjust the max length. */
		*stoplen = USERSPACETOP - bot;
	}

	return 0;
}
