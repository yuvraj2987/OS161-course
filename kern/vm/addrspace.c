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
#include <spl.h>
#include <mips/tlb.h>
/*
 * Note! If OPT_DUMBVM is set, as is the case until you start the VM
 * assignment, this file is not compiled or linked or in any way
 * used. The cheesy hack versions in dumbvm.c are used instead.
 */

struct addrspace *
as_create(void)
{
	struct addrspace *as;

	as = kmalloc(sizeof(struct addrspace));
	if (as == NULL) {
		return NULL;
	}

	/*
	 * Initialize as needed.
	 */
	as->heap_start = 0;
	as->heap_end = 0;
	as->stacktop = 0;
	/*No need to allocate page here since we are
	 * using linked list as a page table*/

	return as;
}

int
as_copy(struct addrspace *old, struct addrspace **ret)
{
	struct addrspace *newas;

	newas = as_create();
	if (newas==NULL) {
		return ENOMEM;
	}

	/*
	 * Write this.
	 */

	old->heap_start = newas->heap_start;
	old->heap_end   = newas->heap_end;
	old->stacktop = newas->stacktop;
	newas->region_list = copy_region_list(old->region_list);
	/*Validate*/
	KASSERT(newas->heap_start != 0);
	KASSERT(newas->heap_end != 0);
	KASSERT(newas->stacktop != 0);
	KASSERT(newas->region_list != NULL);
	//newas->page_table_list = copy_page_table(old->page_table_list);
	copy_page_table(old, newas);
	//error checks
	*ret = newas;

	return 0;
}

void
as_destroy(struct addrspace *as)
{
	/*
	 * Clean up as needed.
	 * 1. Loop through page_table_list
	 * 2. if page is allocated free it
	 * 3. else kfree page_table_entry
	 * 4. Free the page_table_list
	 */
	struct page_table_entry *cur_page = as->page_table_list;
	struct page_table_entry *next_page = NULL;
	while(cur_page != NULL)
	{
		next_page = cur_page->next_page_entry;
		if(cur_page->allocated)
		{
			free_user_pages(cur_page->as_physical, 1);
		}
		kfree(cur_page);
		cur_page = next_page;
	}//end of while

	/*
	 * 5. loop through regions
	 * 6. Free each region
	 * 7. destroy addrspace
	 */
	struct region *cur_reg = as->region_list;
	struct region *next_reg = NULL;
	while(cur_reg != NULL)
	{
		next_reg = cur_reg->next_region;
		kfree(cur_reg);
		cur_reg = next_reg;
	}//end of while

	kfree(as);
}

void
as_activate(struct addrspace *as)
{
	/*
	 * Write this.
	 */

	(void)as;  // suppress warning until code gets written
	int i, spl;
	/* Disable interrupts on this CPU while frobbing the TLB. */
	spl = splhigh();

	for (i=0; i<NUM_TLB; i++) {
		tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
	}

	splx(spl);

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
	size_t npages;
	vaddr_t cur_page_addr;
	/* Align the region. First, the base... */
	sz += vaddr & ~(vaddr_t)PAGE_FRAME;
	vaddr &= PAGE_FRAME;

	/* ...and now the length. */
	sz = (sz + PAGE_SIZE - 1) & PAGE_FRAME;

	npages = sz / PAGE_SIZE;
	//alloate new region
	struct region *new_region = (struct region*)kmalloc(sizeof(struct region));

	if(new_region == NULL)
		return ENOMEM;

	new_region->as_region_start = vaddr;
	new_region->region_size = sz;
	new_region->execute = executable;
	new_region->read = readable;
	new_region->write = writeable;
	append_region(&as->region_list, new_region);
	cur_page_addr = vaddr;
	for(unsigned int i = 0; i<npages; i++)
	{
		struct page_table_entry *new_pte = (struct page_table_entry*)kmalloc(sizeof(struct page_table_entry));

		if(new_pte == NULL)
			return ENOMEM;

		new_pte->as_virtual = cur_page_addr;
		new_pte->allocated = 0;	//false
		new_pte->as_physical = 0;//allocated in vm_fault
		new_pte->execute = executable;
		new_pte->read = readable;
		new_pte->write = writeable;
		append_page_table_entry(&as->page_table_list, new_pte);
		cur_page_addr += PAGE_SIZE;
		cur_page_addr &= PAGE_FRAME;
	}
	//heap allocation
	if(as->heap_start <= vaddr+sz)
	{
		as->heap_start = vaddr+sz;
		as->heap_start &= PAGE_FRAME;//Page alignment
		as->heap_end  = as->heap_start;
	}

	return 0;
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



	/* Initial user-level stack pointer */
	*stackptr = USERSTACK;
	/*Allocate stack PTE beforehand*/
	as->stacktop = USERSTACK - (VM_STACKPAGES*PAGE_SIZE);
	vaddr_t stack_page = as->stacktop;
	for(int i = 0; i <= VM_STACKPAGES; i++)
	{
		struct page_table_entry *new_page = (struct page_table_entry *)kmalloc(sizeof(struct page_table_entry));

		if(new_page == NULL)
			return ENOMEM;
		new_page->allocated = 0;
		new_page->as_physical = 0; /*Unintitalized*/
		new_page->as_virtual = stack_page;
		new_page->execute = 0;
		new_page->read 	  = 1;
		new_page->write   = 1;
		append_page_table_entry(&as->page_table_list, new_page);
		//increment address
		stack_page = stack_page - PAGE_SIZE;
	}

	return 0;
}

/*ASST3*/
struct region* copy_region_list(struct region *list_head)
{
	struct region *current = list_head;
	struct region *new_list_head = NULL;
	struct region *new_list_tail = NULL;
	while(current != NULL)
	{
		if(new_list_head == NULL)
		{
			new_list_head = (struct region *)kmalloc(sizeof(struct region));
			new_list_tail = new_list_head;
			//DATA
			new_list_head->as_region_start = current->as_region_start;
			new_list_head->region_size = current->region_size;
			new_list_head->execute = current->execute;
			new_list_head->read = current->read;
			new_list_head->write = current->write;
		}
		else
		{
			new_list_tail->next_region = (struct region *)kmalloc(sizeof(struct region));
			new_list_tail = new_list_tail->next_region;
			new_list_tail->next_region = NULL;
			//Data
			new_list_tail->as_region_start = current->as_region_start;
			new_list_tail->region_size = current->region_size;
			new_list_tail->execute = current->execute;
			new_list_tail->read = current->read;
			new_list_tail->write = current->write;

		}//end of if else
		current = current->next_region;
	}//end of while loop

	return new_list_head;
}


//struct page_table_entry* copy_page_table(struct page_table_entry *pte_head)
void copy_page_table(struct addrspace *old, struct addrspace *new)
{
	struct page_table_entry *cur = old->page_table_list;
	struct page_table_entry *new_pte_head = NULL;
	struct page_table_entry *new_pte_tail = NULL;
	while(cur != NULL)
	{
		if(new_pte_head == NULL)
		{
			new_pte_head = (struct page_table_entry*)kmalloc(sizeof(struct page_table_entry));

			if(new_pte_head == NULL)
				return;

			new_pte_tail = new_pte_head;
			//Data
			cur->as_virtual = new_pte_head->as_virtual;
			cur->execute = new_pte_head->execute;
			cur->read = new_pte_head->read;
			cur->write = new_pte_head->write;
			cur->allocated = new_pte_head->allocated;
			if(cur->allocated)
			{
				/*
				 * Allocate a new physical page for new addrspace page table
				 * Copy content of new page
				 * */
				//get_user_pages(unsigned long npages, struct addrspace* as, vaddr_t va)
				new_pte_head->as_physical = get_user_pages( 1, new, new_pte_head->as_virtual);

				if(new_pte_head->as_physical == 0)
					panic("Error: Cannot allocate get_user_pages in copy_page_table\n");
				/*
				memmove((void *)PADDR_TO_KVADDR(new->as_pbase1),
						(const void *)PADDR_TO_KVADDR(old->as_pbase1),
						old->as_npages1*PAGE_SIZE);
						*/
				memmove((void *)PADDR_TO_KVADDR(cur->as_physical),
						(const void *)PADDR_TO_KVADDR(new_pte_head->as_physical),PAGE_SIZE);
			}

		}
		else
		{
			new_pte_tail->next_page_entry = (struct page_table_entry *)kmalloc(sizeof(struct page_table_entry));
			new_pte_tail = new_pte_tail->next_page_entry;
			new_pte_tail->next_page_entry = NULL;
			//Data
			cur->as_virtual = new_pte_tail->as_virtual;
			cur->execute = new_pte_tail->execute;
			cur->read = new_pte_tail->read;
			cur->write = new_pte_tail->write;
			cur->allocated = new_pte_tail->allocated;
			if(cur->allocated)
			{
				/*
				 * Allocate a new physical page for new addrspace page table
				 * Copy content of new page
				 * */
				//get_user_pages(unsigned long npages, struct addrspace* as, vaddr_t va)

				new_pte_tail->as_physical = get_user_pages( 1, new, new_pte_head->as_virtual);
				if(new_pte_tail->as_physical == 0)
					panic("Error: Cannot allocate get_user_pages in copy_page_table\n");
				/*
							memmove((void *)PADDR_TO_KVADDR(new->as_pbase1),
									(const void *)PADDR_TO_KVADDR(old->as_pbase1),
									old->as_npages1*PAGE_SIZE);
				 */
				memmove((void *)PADDR_TO_KVADDR(cur->as_physical),
						(const void *)PADDR_TO_KVADDR(new_pte_tail->as_physical),PAGE_SIZE);
			}


		}//end of ie else

		cur = cur->next_page_entry;
	}//end of while

}



void append_region(struct region **reg_head_ref, struct region *new_region)
{
	struct region *cur = *reg_head_ref;
	if(cur == NULL)
	{
		*reg_head_ref = new_region;
	}
	else
	{
		while(cur->next_region != NULL)
		{
			cur = cur->next_region;
		}

		cur->next_region = new_region;
		new_region->next_region = NULL;
	}
}

void append_page_table_entry(struct page_table_entry **pgtbl_head_ref, struct page_table_entry *new_page)
{
	struct page_table_entry *cur = *pgtbl_head_ref;
	if(cur == NULL)
	{
		new_page->next_page_entry = NULL;
		*pgtbl_head_ref = new_page;
	}
	else
	{
		while(cur->next_page_entry != NULL)
		{
			cur = cur->next_page_entry;
		}

		cur->next_page_entry = new_page;
		new_page->next_page_entry = NULL;
	}
}
