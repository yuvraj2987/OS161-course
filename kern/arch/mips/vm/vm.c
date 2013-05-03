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

//#define VM_STACKPAGES    12

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
	//vaddr_t vbase1, vtop1, vbase2, vtop2, stackbase, stacktop;
	paddr_t paddr;
	int i;
	uint32_t ehi, elo;
	struct addrspace *as;
	int spl;

	faultaddress &= PAGE_FRAME;

	//DEBUG(DB_VM, "dumbvm: fault: 0x%x\n", faultaddress);

	switch (faulttype)
	{
	case VM_FAULT_READONLY:
		/* We always create pages read-write, so we can't get this */
		panic("dumbvm: got VM_FAULT_READONLY\n");
		break;
	case VM_FAULT_READ:
	case VM_FAULT_WRITE:
		break;
	default:
		return EINVAL;
	}

	as = curthread->t_addrspace;
	if (as == NULL)
	{
		/*
		 * No address space set up. This is probably a kernel
		 * fault early in boot. Return EFAULT so as to panic
		 * instead of getting into an infinite faulting loop.
		 */
		return EFAULT;
	}

	/* Assert that the address space has been set up properly. */
	/*
	KASSERT(as->as_vbase1 != 0);
	KASSERT(as->as_pbase1 != 0);
	KASSERT(as->as_npages1 != 0);
	KASSERT(as->as_vbase2 != 0);
	KASSERT(as->as_pbase2 != 0);
	KASSERT(as->as_npages2 != 0);
	KASSERT(as->as_stackpbase != 0);
	KASSERT((as->as_vbase1 & PAGE_FRAME) == as->as_vbase1);
	KASSERT((as->as_pbase1 & PAGE_FRAME) == as->as_pbase1);
	KASSERT((as->as_vbase2 & PAGE_FRAME) == as->as_vbase2);
	KASSERT((as->as_pbase2 & PAGE_FRAME) == as->as_pbase2);
	KASSERT((as->as_stackpbase & PAGE_FRAME) == as->as_stackpbase);*/

	KASSERT(as->as_heapEnd!=0);
	KASSERT(as->as_heapStart!=0);
	KASSERT(as->stackTop!=0);
	KASSERT(as->regionList!=NULL);
	KASSERT(as->pageList!=NULL);

	struct pageEntry* head = as->pageList;
	bool isFound = 0;

	while(head!=NULL)
	{
		if(faultaddress>=(vaddr_t)head && faultaddress<(vaddr_t)(head+PAGE_SIZE))
		{
			//paddr = (faultaddress - head->va) + head->pa;
			paddr = head->pa;
			isFound = 1;
		}
		head = head->nextPageEntry;
	}

	if(isFound==0 && (faultaddress>(as->stackTop-PAGE_SIZE) && faultaddress<=as->stackTop))
	{
		isFound = 1;
		struct pageEntry* newPage = (struct pageEntry*)kmalloc(sizeof(struct pageEntry));
		newPage->executable = 0;
		newPage->nextPageEntry = NULL;
		newPage->pa = coremap_stealmem_user(1, as, faultaddress);
		newPage->readable = 1;
		newPage->va = faultaddress;
		newPage->valid = 1;
		newPage->writable = 1;
		//addPageToList(as->pageList, newPage);
		if(as->pageList==NULL)
		{
			as->pageList = newPage;
		}
		else
		{
			struct pageEntry* head = as->pageList;
			while(head!=NULL)
			{
				head = head->nextPageEntry;
			}
			head->nextPageEntry = newPage;
		}
		paddr = newPage->pa;
	}
	else
	{
		return EFAULT;
	}

	/*vbase1 = as->as_vbase1;
	vtop1 = vbase1 + as->as_npages1 * PAGE_SIZE;
	vbase2 = as->as_vbase2;
	vtop2 = vbase2 + as->as_npages2 * PAGE_SIZE;
	stackbase = USERSTACK - DUMBVM_STACKPAGES * PAGE_SIZE;
	stacktop = USERSTACK;


	if (faultaddress >= vbase1 && faultaddress < vtop1) {
		paddr = (faultaddress - vbase1) + as->as_pbase1;
	}
	else if (faultaddress >= vbase2 && faultaddress < vtop2) {
		paddr = (faultaddress - vbase2) + as->as_pbase2;
	}
	else if (faultaddress >= stackbase && faultaddress < stacktop) {
		paddr = (faultaddress - stackbase) + as->as_stackpbase;
	}
	else {
		return EFAULT;
	}*/

	/* make sure it's page-aligned */
	KASSERT((paddr & PAGE_FRAME) == paddr);

	/* Disable interrupts on this CPU while frobbing the TLB. */
	spl = splhigh();

	for (i=0; i<NUM_TLB; i++)
	{
		tlb_read(&ehi, &elo, i);
		if (elo & TLBLO_VALID)
		{
			continue;
		}
		ehi = faultaddress;
		elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
		DEBUG(DB_VM, "dumbvm: 0x%x -> 0x%x\n", faultaddress, paddr);
		tlb_write(ehi, elo, i);
		splx(spl);
		return 0;
	}

	kprintf("dumbvm: Ran out of TLB entries - cannot handle page fault\n");
	splx(spl);
	return EFAULT;

}

/*void addPageToList(struct pageEntry* pageList, struct pageEntry* newPage)
{
	if(pageList==NULL)
	{
		newPage->nextPageEntry = NULL;
		pageList = newPage;
	}
	else
	{
		struct pageEntry* head = pageList;
		while(head!=NULL)
		{
			head = head->nextPageEntry;
		}
		newPage = NULL;
		head->nextPageEntry = newPage;
	}
}*/
