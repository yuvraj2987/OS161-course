/*
 * vm.c

 *
 *  Created on: Apr 30, 2013
 *      Author: trinity
 */
#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <spl.h>
#include <spinlock.h>
#include <thread.h>
#include <current.h>
#include <mips/tlb.h>
#include <addrspace.h>
#include <vm.h>
#include <synch.h>

static unsigned long total_pages;
static paddr_t  freeaddr;
static paddr_t firstaddr, lastaddr;
bool isVmBootstrap = 0;
static struct spinlock stealmem_lock = SPINLOCK_INITIALIZER;

void
vm_bootstrap(void)
{

	//coremap_lock = lock_create("coremap_lock");

	/*Do all init*/
	ram_getsize(&firstaddr, &lastaddr);

	/* pages should be a kernel virtual address !!  */
	struct page* pages = (struct page*)PADDR_TO_KVADDR(firstaddr);
	total_pages = lastaddr/PAGE_SIZE;
	//unsigned long kern_page_num = firstaddr/PAGE_SIZE;
	freeaddr = firstaddr + total_pages * sizeof(struct page);
	unsigned long kern_page_num = freeaddr/PAGE_SIZE;
	/*Init kern pages*/
	for(unsigned long i =0; i<kern_page_num; i++)
	{
		pages[i].as   = NULL;
		pages[i].state = FIXED;
		pages[i].va    = PADDR_TO_KVADDR(i*PAGE_SIZE);
	}
	/*Init user pages*/
	for(unsigned long i = kern_page_num; i<total_pages; i++)
	{
		pages[i].state = FREE;
		pages[i].as   = NULL;
		pages[i].va    = -1;
	}

	//set the flag
	isVmBootstrap = 1;

}


paddr_t getppages(unsigned long npages)
{
	paddr_t addr;

	if(!isVmBootstrap)
	{
		spinlock_acquire(&stealmem_lock);
		addr = ram_stealmem(npages);
		spinlock_release(&stealmem_lock);
		//return addr;
	}
	else
	{
		spinlock_acquire(&stealmem_lock);
		addr = coremap_stealmem(npages);
		spinlock_release(&stealmem_lock);

	}
	return addr;
}

paddr_t coremap_stealmem_user(unsigned long npages, struct addrspace* as, vaddr_t va)
{
	(void)npages;
	unsigned long count =0;
	paddr_t returnAdd = 0;

	spinlock_acquire(&stealmem_lock);

	struct page* pages = (struct page*)PADDR_TO_KVADDR(firstaddr);

	for(count=0; count<total_pages; count++)
	{
		if(pages[count].state==FREE)
		{
			returnAdd = count*PAGE_SIZE;
			pages[count].state=DIRTY;
			pages[count].as = as;
			pages[count].va = va;
		}
	}

	spinlock_release(&stealmem_lock);
	return returnAdd;

}

paddr_t coremap_stealmem(unsigned long npages)
{
	(void)npages;
	unsigned long count =0;
	struct page* pages = (struct page*)PADDR_TO_KVADDR(firstaddr);

	for(count=0; count<total_pages; count++)
	{
		if(pages[count].state==FREE)
		{
			paddr_t returnAdd = count*PAGE_SIZE;
			pages[count].state=DIRTY;
			pages[count].as = NULL;
			pages[count].va = PADDR_TO_KVADDR(returnAdd);
			return returnAdd;
		}
	}
	// In case of no free page
	return 0;
}

vaddr_t alloc_kpages(int npages)
{
	paddr_t psyAdd = getppages(npages);
	return PADDR_TO_KVADDR(psyAdd);
}

void free_kpages(vaddr_t addr)
{
	unsigned long count =0;
	struct page* pages = (struct page*)PADDR_TO_KVADDR(firstaddr);

	spinlock_acquire(&stealmem_lock);
	for(count=0; count<total_pages; count++)
	{
		if(pages[count].va==addr)
		{
			pages[count].state=FREE;
			pages[count].as = NULL;
			pages[count].va = -1;
			//pages[count].timestmap = NULL;
			break;
		}
	}
	spinlock_release(&stealmem_lock);
	return;
}

void vm_tlbshootdown_all(void)
{
	panic("dumbvm tried to do tlb shootdown?!\n");
}

void vm_tlbshootdown(const struct tlbshootdown *ts)
{
	(void)ts;
	panic("dumbvm tried to do tlb shootdown?!\n");
}

int vm_fault(int faulttype, vaddr_t faultaddress)
{
	(void)faulttype;
	(void)faultaddress;
	return 0;
}
