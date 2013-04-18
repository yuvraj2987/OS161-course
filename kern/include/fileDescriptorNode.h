#include <limits.h>

/*ASST2*/

#define MAX_NUMBER_OF_FILES	32

/* file descriptor node structure*/
struct fileDescriptorNode
{
	const char * filePath;
	struct vnode * vn;
	struct lock * fDNLock;
	off_t offset;
	int flag;
	int refCount;
};
