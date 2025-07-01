// Physical memory allocator, intended to allocate
// memory for user processes, kernel stacks, page table pages,
// and pipe buffers. Allocates 4096-byte pages.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "spinlock.h"

void freerange(void *vstart, void *vend);
extern char end[]; // first address after kernel loaded from ELF file
                   // defined by the kernel linker script in kernel.ld

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  int use_lock;
  struct run *freelist;
} kmem;

// Initialization happens in two phases.
// 1. main() calls kinit1() while still using entrypgdir to place just
// the pages mapped by entrypgdir on free list.
// 2. main() calls kinit2() with the rest of the physical pages
// after installing a full page table that maps them on all cores.
void
kinit1(void *vstart, void *vend)
{
  initlock(&kmem.lock, "kmem");
  kmem.use_lock = 0;
  freerange(vstart, vend);
}

void
kinit2(void *vstart, void *vend)
{
  freerange(vstart, vend);
  kmem.use_lock = 1;
}

void
freerange(void *vstart, void *vend)
{
  char *p;
  p = (char*)PGROUNDUP((uint)vstart);
  for(; p + PGSIZE <= (char*)vend; p += PGSIZE)
    kfree(p);
}
//PAGEBREAK: 21
// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(char *v)
{
  struct run *r;

  if((uint)v % PGSIZE || v < end || V2P(v) >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(v, 1, PGSIZE);

  if(kmem.use_lock)
    acquire(&kmem.lock);
  r = (struct run*)v;
  r->next = kmem.freelist;
  kmem.freelist = r;
  if(kmem.use_lock)
    release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
char*
kalloc(void)
{
  struct run *r;

  if(kmem.use_lock)
    acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  if(kmem.use_lock)
    release(&kmem.lock);
  return (char*)r;
}

#define HUGE_PGROUNDUP(sz) (((sz) + HUGE_PAGE_SIZE - 1) & ~(HUGE_PAGE_SIZE - 1))

struct huge_run{
  struct huge_run *next;
};

struct{
  struct spinlock lock;
  int use_lock;
  struct huge_run *freelist;
} hmem;

void khugefree(char *v){
  struct huge_run *hr;

  if((uint)v % HUGE_PAGE_SIZE || V2P(v) < HUGE_PAGE_START || V2P(v) >= HUGE_PAGE_END)
    panic("khugefree");

  memset(v, 1, HUGE_PAGE_SIZE);

  if(hmem.use_lock)
    acquire(&hmem.lock);
  hr = (struct huge_run*)v;
  hr->next = hmem.freelist;
  hmem.freelist = hr;
  if(hmem.use_lock)
    release(&hmem.lock);
}

void khuge_freerange(int start,int end){
  char *p;
  p = (char*)HUGE_PGROUNDUP((uint)start);
  for(; p + HUGE_PAGE_SIZE <= (char*)end; p += HUGE_PAGE_SIZE)
    khugefree(p);
}

void khugeinit(){
  initlock(&hmem.lock, "hmem");  // initialize lock
  hmem.use_lock = 0;             // start with locking disabled during initialization
  hmem.freelist = 0;             // explicitly initialize freelist to NULL
  hmem.use_lock = 1;
}


char* khugealloc(void) {
  struct huge_run *r;
  static uint next_addr = HUGE_PAGE_START;

  if(hmem.use_lock)
    acquire(&hmem.lock);
  
  r = hmem.freelist;
  if(r) {
    // If we have a free huge page in our list, use it
    hmem.freelist = r->next;
    cprintf("khugealloc: from freelist, returning %x\n", r);
  } else {
    // No free huge pages, allocate a new one from the reserved region
    if(next_addr + HUGE_PAGE_SIZE > HUGE_PAGE_END) {
      // Out of huge page memory
      cprintf("khugealloc: out of memory\n");
      if(hmem.use_lock)
        release(&hmem.lock);
      return 0;
    }
    
    // Map this physical address to a kernel virtual address
    r = (struct huge_run*)P2V(next_addr);
    cprintf("khugealloc: new page, phys=%x, virt=%x\n", next_addr, r);
    next_addr += HUGE_PAGE_SIZE;
  }
  
  if(hmem.use_lock)
    release(&hmem.lock);
    
  return (char*)r;
}