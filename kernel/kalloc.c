// Physical memory allocator, intended to allocate
// memory for user processes, kernel stacks, page table pages,
// and pipe buffers. Allocates 4096-byte pages.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "mmu.h"
#include "spinlock.h"

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;

  /*
  P5 changes
  */
  uint free_pages; //track free pages
  uint ref_cnt[PHYSTOP / PGSIZE]; //track reference count

} kmem;

extern char end[]; // first address after kernel loaded from ELF file can only allocate within a certain range

// Initialize free list of physical pages.
void
kinit(void)
{
  char *p;

  initlock(&kmem.lock, "kmem");//need to aquire lock if accecessing members of kmem
  p = (char*)PGROUNDUP((uint)end);
  for(; p + PGSIZE <= (char*)PHYSTOP; p += PGSIZE)//increase by page size because that is the boundary
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(char *v)
{
  struct run *r;

  if((uint)v % PGSIZE || v < end || (uint)v >= PHYSTOP) 
    panic("kfree");//bound check on physical page need to do if getting ref count etc

  // Fill with junk to catch dangling refs.
  memset(v, 1, PGSIZE);

  acquire(&kmem.lock);
  r = (struct run*)v;

  if(kmem.ref_cnt[PADDR(v) >> PGSHIFT] > 0)//when page is freed dec ref count
    kmem.ref_cnt[PADDR(v) >> PGSHIFT] = kmem.ref_cnt[PADDR(v) >> PGSHIFT] -1;
  
  if(kmem.ref_cnt[PADDR(v) >> PGSHIFT] == 0){
    memset(v, 1, PGSIZE);
    r->next = kmem.freelist;
    kmem.freelist = r;
    kmem.free_pages = kmem.free_pages + 1;// increasing free pages as youre adding to the list 
  }
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
char*
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r){
    kmem.freelist = r->next;
    kmem.ref_cnt[PADDR((char*)r) >> PGSHIFT] = 1; //ref_cnt page is set to 1 when allocated
    kmem.free_pages = kmem.free_pages - 1;// decreasing free pages as your're subtracting from list
  }
  release(&kmem.lock);
  return (char*)r;
}

/* Get total number of free pages in the system helper*/
int getFreePagesCount(void){
	uint free_pages;//free pages
	acquire(&kmem.lock);
	free_pages = kmem.free_pages;
	release(&kmem.lock);
	return free_pages;

}
/*TODO: increment count of references*/
void inc_refCount(uint pa){
	if(pa >= PHYSTOP || pa < (uint)PADDR(end)){
		panic("increment count of references");
	}

	acquire(&kmem.lock);
	kmem.ref_cnt[pa >> PGSHIFT] = kmem.ref_cnt[pa >> PGSHIFT] + 1; //PGSHIFT found in kernel/mmu.h log2(PGSIZE)
	release(&kmem.lock);
}
/*TODO: decrement count of references*/
void dec_refCount(uint pa){
	if(pa >= PHYSTOP || pa < (uint)PADDR(end)){
		panic("decrement count of references");
	}
	
	acquire(&kmem.lock);
	kmem.ref_cnt[pa >> PGSHIFT] = kmem.ref_cnt[pa >> PGSHIFT] -1;
	release(&kmem.lock);

}
/*TODO: Get count of references*/
uint get_refCount(uint pa){
	uint count;

	if(pa >= PHYSTOP || pa < (uint)PADDR(end)){
		panic("get reference count");
	}

	acquire(&kmem.lock);
	count = kmem.ref_cnt[pa >> PGSHIFT];
	release(&kmem.lock);

  return count;

}

