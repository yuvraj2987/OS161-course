/*
 * vm.c

 *
 *  Created on: Apr 30, 2013
 *      Author: trinity
 */
#include <vm.h>
#include <types.h>


void
vm_bootstrap(void)
{
	paddr_t firstaddr, lastaddr, freeaddr;
	coremap_lock = lock_create("coremap_lock");

	/*Do all init*/
	ram_getsize(&firstaddr, &lastaddr);

	/* pages should be a kernel virtual address !!  */
	struct page* pages = (struct page*)PADDR_TO_KVADDR(firstaddr);
	int page_num = lastaddr/PAGE_SIZE;
	int kern_page_num = firstaddr/PAGE_SIZE;
	freeaddr = firstaddr + page_num * sizeof(struct page);
	/*Init kern pages*/
	for(int i =0; i<kern_page_num; i++)
	{
		pages[i]->as   = NULL;
		pages[i].state = FIXED;
		pages[i].va    = PADDR_TO_KVADDR(i*PAGE_SIZE);
	}
	/*Init user pages*/
	for(int i =kern_page_num; i<page_num; i++)
	{
		pages[i].state = FREE;
		pages[i]->as   = NULL;
		pages[i].va    = NULL;
	}

	//set the flag
	vm_bootstrap_flag = 1;

}


static
paddr_t
getppages(unsigned long npages)
{
	if(!vm_bootstrap_flag)
	{
		paddr_t addr;

		spinlock_acquire(&stealmem_lock);

		addr = ram_stealmem(npages);

		spinlock_release(&stealmem_lock);
		return addr;
	}
}

