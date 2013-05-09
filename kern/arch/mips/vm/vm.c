/*
 * vm.c
 *
 *  Created on: May 7, 2013
 *  Author: Chirag Todarka, Amit Kulkarni
 */

#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <spl.h>
#include <spinlock.h>
#include <thread.h>
#include <current.h>
#include <mips/tlb.h>
#include <vm.h>

#define VM_STACKPAGES    12
static struct spinlock stealmem_lock = SPINLOCK_INITIALIZER;

bool isVMStarted = 0;
unsigned long totalNumberOfPages, kernelPages;
paddr_t firstaddr, lastaddr, freeaddr;
struct page* coreMap;

unsigned long count;

void vm_bootstrap(void)
{
	ram_getsize(&firstaddr, &lastaddr);
	totalNumberOfPages = lastaddr/PAGE_SIZE;

	coreMap = (struct page*)PADDR_TO_KVADDR(firstaddr);
	freeaddr = firstaddr + totalNumberOfPages * sizeof(struct page);
	kernelPages = (freeaddr + PAGE_SIZE - 1)/PAGE_SIZE;

	struct page* tempCoreMap = coreMap;

	for(count=0; count<kernelPages; count++)
	{
		tempCoreMap->as = NULL;
		tempCoreMap->state = FIXED;
		tempCoreMap->va = PADDR_TO_KVADDR(count*PAGE_SIZE);
		tempCoreMap->pa = count* PAGE_SIZE;
		tempCoreMap->pageCount = 1;
		tempCoreMap = tempCoreMap + sizeof(struct page);
	}

	for(count=kernelPages; count<totalNumberOfPages; count++)
	{
		tempCoreMap->as = NULL;
		tempCoreMap->state = FREE;
		tempCoreMap->va = 0;
		tempCoreMap->pa = count* PAGE_SIZE;
		tempCoreMap->pageCount = 1;
		tempCoreMap = tempCoreMap + sizeof(struct page);
	}

	isVMStarted = 1;
}

paddr_t coreMap_stealmem(unsigned long npages)
{
	unsigned long count;
	unsigned long innerCount;
	bool foundContinuousPages = 0;

	for(count=0; count<totalNumberOfPages; count++)
	{
		if(coreMap[count].state==FREE)
		{
			for(innerCount=count; innerCount<(count+npages); innerCount++)
			{
				if(coreMap[innerCount].state!=FREE)
				{
					foundContinuousPages = 0;
					break;
				}
				foundContinuousPages = 1;
			}

			if(foundContinuousPages==1)
			{
				paddr_t returnAdd = coreMap[count].pa;
				coreMap[count].as = NULL;
				coreMap[count].va = PADDR_TO_KVADDR(returnAdd);
				coreMap[count].state = DIRTY;
				coreMap[count].pageCount = npages;
				return returnAdd;
			}

		}//outer if ends
	}//outer for ends
	return 0;
}

static paddr_t getppages(unsigned long npages)
{
	paddr_t addr;

	spinlock_acquire(&stealmem_lock);

	if(isVMStarted==0)
		addr = ram_stealmem(npages);
	else
		addr = coreMap_stealmem(npages);

	spinlock_release(&stealmem_lock);

	return addr;
}

/* Allocate/free some kernel-space virtual pages */
vaddr_t alloc_kpages(int npages)
{
	paddr_t pa;
	pa = getppages(npages);
	if (pa==0)
	{
		return 0;
	}
	return PADDR_TO_KVADDR(pa);
}

void free_kpages(vaddr_t addr)
{
	unsigned long count, innerCount;
	spinlock_acquire(&stealmem_lock);

	for(count=0; count<totalNumberOfPages; count++)
	{
		if(coreMap[count].va==addr)
		{
			int pgC = coreMap[count].pageCount;
			for(innerCount=count; innerCount<(count+pgC); innerCount++)
			{
				coreMap[innerCount].as = NULL;
				coreMap[innerCount].va = 0;
				coreMap[innerCount].pageCount = 1;
				coreMap[innerCount].state = FREE;
			}
		}

	}

	spinlock_release(&stealmem_lock);
}

void
vm_tlbshootdown_all(void)
{
	panic("dumbvm tried to do tlb shootdown?!\n");
}

void
vm_tlbshootdown(const struct tlbshootdown *ts)
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

int sys_sbrk(intptr_t amount, void *retval)
{

	return 0;
}
