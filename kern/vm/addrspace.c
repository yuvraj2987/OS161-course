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

#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <addrspace.h>
#include <vm.h>

/*
 * Note! If OPT_DUMBVM is set, as is the case until you start the VM
 * assignment, this file is not compiled or linked or in any way
 * used. The cheesy hack versions in dumbvm.c are used instead.
 */

struct addrspace *
as_create(void)
{
	struct addrspace *as = kmalloc(sizeof(struct addrspace));
	if (as == NULL) {
		return NULL;
	}

	/*
	 * Initialize as needed.
	 * ASST3
	 */
	as->stackTop = USERSTACK;
	as->as_heapStart = 0;
	as->as_heapEnd = 0;
	as->regionList = NULL;
	as->pageList = NULL;

	return as;
}

int
as_copy(struct addrspace *old, struct addrspace **ret)
{
	struct addrspace *newas = kmalloc(sizeof(struct addrspace));
	if (newas==NULL)
	{
			return ENOMEM;
	}
	newas->stackTop = old->stackTop;
	newas->as_heapStart = old->as_heapStart;
	newas->as_heapEnd = old->as_heapEnd;
	newas->regionList = NULL;
	newas->pageList = NULL;

	while(old->regionList!=NULL)
	{
		//struct region* oldRegion = old->regionList;

		struct region* tempRegion = (struct region*)kmalloc(sizeof(struct region));
		tempRegion->executable = old->regionList->executable;
		tempRegion->readable = old->regionList->readable;
		tempRegion->regionSize = old->regionList->regionSize;
		tempRegion->regionStart = old->regionList->regionStart;
		tempRegion->writable = old->regionList->writable;

		tempRegion->nextRegion = NULL;

	}

	//newas = as_create();


	/*
	 * Write this.
	 */

	//(void)old;
	
	*ret = newas;
	return 0;
}

void
as_destroy(struct addrspace *as)
{
	/*
	 * Clean up as needed.
	 */
	
	kfree(as);
}

void
as_activate(struct addrspace *as)
{
	/*
	 * Write this.
	 */

	(void)as;  // suppress warning until code gets written
}

/*
 * Set up a segment at virtual address VADDR of size MEMSIZE. The
 * segment in memory extends from VADDR up to (but not including)
 * VADDR+MEMSIZE.
 *
 * The READABLE, WRITEABLE, and EXECUTABLE flags are set if read,
 * write, or execute permission should be set on the segment. At the
 * moment, these are ignored. When you write the VM system, you may
 * want to implement them.
 */
int
as_define_region(struct addrspace *as, vaddr_t vaddr, size_t sz,
		 int readable, int writeable, int executable)
{
	/*
	 * Write this.
	 */
	struct region *newregion = region_getNewRegion(as);
	newregion->nextRegion = NULL;
	newregion->readable = readable;
	newregion->writable = writeable;
	newregion->executable = executable;
	newregion->regionSize = sz;
	newregion->regionStart = vaddr;
	//newregion->regionPageList = NULL;

	size_t numberOfPages = (sz + PAGE_SIZE - 1)/PAGE_SIZE;

	for(size_t count=0; count<numberOfPages; count++)
	{
		struct pageEntry* newPage = kmalloc(sizeof(struct pageEntry));
		newPage->va = vaddr;
		newPage->pa = coremap_stealmem_user(1, as, vaddr);
		newPage->valid = 1;
		newPage->readable = readable;
		newPage->writable = writeable;
		newPage->executable = executable;

		if(as->pageList==NULL)
		{
			as->pageList = newPage;
		}
		else
		{
			struct pageEntry* head = as->pageList;
			while(head->nextPageEntry!=NULL)
			{
				head = head->nextPageEntry;
			}

			head->nextPageEntry = newPage;
		}

		newPage->nextPageEntry = NULL;
		vaddr = vaddr + PAGE_SIZE;
	}//end of for loop

	return 0;

	/*(void)as;
	(void)vaddr;
	(void)sz;
	(void)readable;
	(void)writeable;
	(void)executable;
	return EUNIMP;*/
}

struct region* region_getNewRegion(struct addrspace *as)
{
	struct region *head = as->regionList;
	struct region *newRegion = (struct region*)kmalloc(sizeof(struct region));

	if(head==NULL)
	{
		as->regionList = newRegion;
		return newRegion;
	}
	else
	{
		while(head->nextRegion!=NULL)
		{
				head = head->nextRegion;
		}
		head->nextRegion = newRegion;
		return newRegion;
	}
}

int
as_prepare_load(struct addrspace *as)
{
	/*
	 * Write this.
	 */

	(void)as;
	return 0;
}

int
as_complete_load(struct addrspace *as)
{
	/*
	 * Write this.
	 */

	(void)as;
	return 0;
}

int
as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{
	/*
	 * Write this.
	 */

		(void)as;

	/* Initial user-level stack pointer */
	*stackptr = USERSTACK;
	
	return 0;
}

