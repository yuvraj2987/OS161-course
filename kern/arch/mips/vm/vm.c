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
#include <addrspace.h>
#include <kern/fcntl.h>
#include <vfs.h>

static struct spinlock stealmem_lock = SPINLOCK_INITIALIZER;

bool isVMStarted = 0;
unsigned long totalNumberOfPages, kernelPages;
paddr_t firstaddr, lastaddr, freeaddr;
struct page* coreMap;

unsigned long pointer = 0;
off_t swapOffset = 0;
struct vnode* swapfile = NULL;

static off_t getNewOffset(void)
{
	off_t old = swapOffset;
	swapOffset = swapOffset + PAGE_SIZE;
	return old;
}

static unsigned long getPointer(void)
{
	if(pointer==0 || pointer<kernelPages)
	{
		panic("pointer==0 || pointer<kernelPages");
	}

	unsigned long temp;
	if(pointer>=kernelPages && pointer<totalNumberOfPages)
	{
		temp = pointer;
		pointer++;
		return temp;
	}
	else
	{
		pointer = kernelPages;
		return pointer;
	}

}

void swapFile_bootstrap(void)
{
	getPointer();
	getNewOffset();

	const char * sf = "lhd0raw:";
	char* kname = kstrdup(sf);

	int err = vfs_open(kname, O_RDWR, 0, &swapfile);
	if(err==0)
		return;
	else
		panic("err!=0 in swapFile_bootstrap");

}

void vm_bootstrap(void)
{
	ram_getsize(&firstaddr, &lastaddr);
	totalNumberOfPages = lastaddr/PAGE_SIZE;
	unsigned long count;

	coreMap = (struct page*)PADDR_TO_KVADDR(firstaddr);
	freeaddr = firstaddr + totalNumberOfPages * sizeof(struct page);
	kernelPages = (freeaddr + PAGE_SIZE - 1)/PAGE_SIZE;
	pointer = kernelPages;
	/*
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
	 */
	for(count = 0; count<kernelPages; count++)
	{
		coreMap[count].as = NULL;
		coreMap[count].state = FIXED;
		coreMap[count].va = PADDR_TO_KVADDR(count*PAGE_SIZE);
		paddr_t paddr = count*PAGE_SIZE;
		KASSERT((paddr&PAGE_FRAME)==paddr);
		coreMap[count].pa = paddr;
		coreMap[count].pageCount = 1;
	}

	/*
	for(count=kernelPages; count<totalNumberOfPages; count++)
	{
		tempCoreMap->as = NULL;
		tempCoreMap->state = FREE;
		tempCoreMap->va = 0;
		tempCoreMap->pa = count* PAGE_SIZE;
		tempCoreMap->pageCount = 1;
		tempCoreMap = tempCoreMap + sizeof(struct page);
	}
	 */
	for(count = kernelPages; count<totalNumberOfPages; count++)
	{
		coreMap[count].as = NULL;
		coreMap[count].state = FREE;
		coreMap[count].va = 0;
		paddr_t paddr = count*PAGE_SIZE;
		KASSERT((paddr&PAGE_FRAME)==paddr);
		coreMap[count].pa = paddr;
		coreMap[count].pageCount = 1;
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
				KASSERT((returnAdd&PAGE_FRAME)==returnAdd);
				for(innerCount=count; innerCount<(count+npages); innerCount++)
				{
					coreMap[innerCount].as = NULL;
					coreMap[innerCount].va = PADDR_TO_KVADDR(returnAdd);
					coreMap[innerCount].state = DIRTY;
					coreMap[innerCount].pageCount = (npages
							-(innerCount-count));
				}
				return returnAdd;
			}

		}//outer if ends
	}//outer for ends
	return 0;
}

paddr_t coreMap_stealmem_user(struct addrspace* as, vaddr_t va)
{
	unsigned long count;
	for(count=0; count<totalNumberOfPages; count++)
	{
		if(coreMap[count].state==FREE)
		{
			paddr_t returnAdd = coreMap[count].pa;
			KASSERT((returnAdd&PAGE_FRAME)==returnAdd);
			coreMap[count].as = as;
			coreMap[count].va = va;
			coreMap[count].state = DIRTY;
			coreMap[count].pageCount = 1;
			//bzero((void*)returnAdd, PAGE_SIZE);
			return returnAdd;
		}
	}
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

paddr_t get_user_pages(unsigned long npages, struct addrspace* as, vaddr_t va)
{
	(void)npages;
	spinlock_acquire(&stealmem_lock);
	paddr_t addr = coreMap_stealmem_user(as,va);
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
	//bzero((void*)pa, npages*PAGE_SIZE);
	return PADDR_TO_KVADDR(pa);
}

void free_kpages(vaddr_t addr)
{
	unsigned long count, innerCount;
	spinlock_acquire(&stealmem_lock);

	for(count=0; count<totalNumberOfPages; count++)
	{
		if(coreMap[count].va==addr && coreMap[count].as==NULL)
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

void free_user_pages(paddr_t addr, int npages)
{
	(void)npages;
	unsigned long count;
	for(count=0; count<totalNumberOfPages; count++)
	{
		if(coreMap[count].pa==addr)
		{
			coreMap[count].as = NULL;
			coreMap[count].va = 0;
			coreMap[count].pageCount = 1;
			coreMap[count].state = FREE;
			return;
		}
	}

}

void vm_tlbshootdown_all(void)
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
	paddr_t paddr = 0;
	//int i;
	uint32_t ehi, elo;
	struct addrspace *as;
	int spl;
	bool page_found = 0;



	faultaddress &= PAGE_FRAME;

	DEBUG(DB_VM, "vm: fault: 0x%x\n", faultaddress);

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
	if (as == NULL) {
		/*
		 * No address space set up. This is probably a kernel
		 * fault early in boot. Return EFAULT so as to panic
		 * instead of getting into an infinite faulting loop.
		 */
		return EFAULT;
	}

	/* Assert that the address space has been set up properly. */
	KASSERT(as->region_list != NULL);
	KASSERT(as->page_table_list != NULL);

	struct page_table_entry *cur_pte = as->page_table_list;
	while(cur_pte != NULL)
	{
		KASSERT((cur_pte->as_virtual & PAGE_FRAME) == cur_pte->as_virtual);

		//if(cur_pte->as_virtual<= faultaddress && cur_pte->as_virtual+PAGE_SIZE > faultaddress)
		if(cur_pte->as_virtual == faultaddress)
		{
			//Page found
			page_found = 1;
			if(!cur_pte->allocated)
			{
				//allocate phy page
				cur_pte->as_physical = get_user_pages(1, as,cur_pte->as_virtual);

				/*No Physical page*/
				if(cur_pte->as_physical == 0)
					return ENOMEM;

				cur_pte->allocated = 1;
				/*Get page physical address*/
				paddr = cur_pte->as_physical;
				break;
			}
			else
			{
				/*Get page physical address*/
				paddr = cur_pte->as_physical;
				break;
			}

		}//end of faultaddress if

		cur_pte = cur_pte->next_page_entry;
	}

	/*If valid page not found then address is invalid*/
	if(!page_found)
		return EFAULT;

	KASSERT((paddr & PAGE_FRAME) == paddr);


	/* Disable interrupts on this CPU while frobbing the TLB. */
	spl = splhigh();

	/*for (i=0; i<NUM_TLB; i++) {
		tlb_read(&ehi, &elo, i);
		if (elo & TLBLO_VALID) {
			continue;
		}
		ehi = faultaddress;
		elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
		DEBUG(DB_VM, "dumbvm: 0x%x -> 0x%x\n", faultaddress, paddr);
		tlb_write(ehi, elo, i);
		splx(spl);
		return 0;
	}*/

	ehi = faultaddress;
	elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
	DEBUG(DB_VM, "MyVm_random: 0x%x -> 0x%x\n", faultaddress, paddr);
	tlb_random(ehi, elo);
	splx(spl);
	return 0;

}

/*
 * ENOMEM        Sufficient virtual memory to satisfy the request was
not available, or the process has reached the limit of the memory it
is allowed to allocate.
 * EINVAL        The request would move the "break" below its initial value.
 */
int sys_sbrk(int amount, int *retval)
{
	*retval = -1;
	struct addrspace *as;
	as = curthread->t_addrspace;
	vaddr_t heap_end = as->heap_end;

	KASSERT((heap_end&PAGE_FRAME)==heap_end);

	if(amount==0)
	{
		*retval = amount;
		return 0;
	}
	else if(amount<0)
	{
		vaddr_t new_heap_end = heap_end + amount;
		if(new_heap_end<as->heap_start)
		{
			*retval = -1;
			return EINVAL;
		}

		//TODO::Code to deallocate pages
		new_heap_end &= PAGE_FRAME;
		int npages = (heap_end-new_heap_end)/PAGE_SIZE;

		struct page_table_entry* page_list = as->page_table_list;
		KASSERT(page_list!=NULL);

		while(1)
		{
			if(page_list->as_virtual==new_heap_end)
				break;

			page_list = page_list->next_page_entry;
			if(page_list==NULL)
				break;
		}

		paddr_t paddr = page_list->as_physical;
		for(int count=0; count<npages; count++)
		{
			free_user_pages(paddr, 1);
			paddr = paddr + PAGE_SIZE;
		}
		*retval = heap_end;
		return 0;
	}
	else
	{
		//KASSERT((as->heap_start+amount) < as->stacktop);
		if((as->heap_start+amount) >= as->stacktop)
		{
			*retval = -1;
			return ENOMEM;
		}

		int npages = (amount +PAGE_SIZE - 1)/PAGE_SIZE;

		for(int count=0; count<npages; count++)
		{
			struct page_table_entry *new_page = (struct page_table_entry *)kmalloc(sizeof(struct page_table_entry));
			new_page->allocated = 0;
			new_page->as_physical = 0;//get_user_pages(1, as, heap_end);
			new_page->as_virtual = heap_end;
			new_page->execute = 0;
			new_page->next_page_entry = NULL;
			new_page->read = 1;
			new_page->write = 1;

			append_page_table_entry(&as->page_table_list, new_page);
			as->heap_start = as->heap_start + PAGE_SIZE;
		}

		*retval = heap_end;
		return 0;
	}

	return 0;
}
