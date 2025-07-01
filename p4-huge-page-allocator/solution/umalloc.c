#include "types.h"
#include "stat.h"
#include "user.h"
#include "param.h"
#include "memlayout.h"

// Memory allocator by Kernighan and Ritchie,
// The C programming Language, 2nd ed.  Section 8.7.

typedef long Align;

union header {
  struct {
    union header *ptr;
    uint size;
  } s;
  Align x;
};

typedef union header Header;

static Header base;
static Header *freep;

static Header huge_base;
static Header *huge_freep;

void
free(void *ap)
{
  Header *bp, *p;

  if((uint)ap >= KERNBASE) {
    printf(2, "Error - trying to free a kernel pointer in userspace\n");
    exit();
  }

  bp = (Header*)ap - 1;
  for(p = freep; !(bp > p && bp < p->s.ptr); p = p->s.ptr)
    if(p >= p->s.ptr && (bp > p || bp < p->s.ptr))
      break;
  if(bp + bp->s.size == p->s.ptr){
    bp->s.size += p->s.ptr->s.size;
    bp->s.ptr = p->s.ptr->s.ptr;
  } else
    bp->s.ptr = p->s.ptr;
  if(p + p->s.size == bp){
    p->s.size += bp->s.size;
    p->s.ptr = bp->s.ptr;
  } else
    p->s.ptr = bp;
  freep = p;
}

static Header*
morecore(uint nu, int flag)
{
  char *p;
  Header *hp;

  if(flag != VMALLOC_SIZE_BASE && flag != VMALLOC_SIZE_HUGE) {
    printf(2, "Please pass VMALLOC_SIZE_BASE or VMALLOC_SIZE_HUGE as flag\n");
    exit();
  } 

  if(flag == VMALLOC_SIZE_BASE) {
    if(nu < 4096)
      nu = 4096;
    p = hugesbrk(nu * sizeof(Header), VMALLOC_SIZE_BASE); // allowed to take in 2 params despite only 1 declared for sbrk
  } else { // VMALLOC_SIZE_HUGE
    if(nu < HUGE_PAGE_SIZE / sizeof(Header)) {
      nu = HUGE_PAGE_SIZE / sizeof(Header);
    }
    p = hugesbrk(nu * sizeof(Header), VMALLOC_SIZE_HUGE);
  }
  
  if(p == (char*)-1)
    return 0;
  hp = (Header*)p;
  hp->s.size = nu;

  if(flag == VMALLOC_SIZE_BASE) {
    // free for regular size
    free((void*)(hp + 1));
    return freep;
  } else { // VMALLOC_SIZE_HUGE
    vfree((void*)(hp + 1));
    return huge_freep;
  }
  // free((void*)(hp + 1));
  // return freep;
}

void*
malloc(uint nbytes)
{
  Header *p, *prevp;
  uint nunits;

  int thp = getthp();
  
  nunits = (nbytes + sizeof(Header) - 1)/sizeof(Header) + 1;

  if(thp == 1 && nbytes > 1024*1024){
    if((prevp = huge_freep) == 0){
      huge_base.s.ptr = huge_freep = prevp = &huge_base;
      huge_base.s.size = 0;
    }
    for(p = prevp->s.ptr;; prevp = p, p = p->s.ptr){
      if(p->s.size >= nunits){
        if(p->s.size == nunits){
          prevp->s.ptr = p->s.ptr;
        }else{
          p->s.size -= nunits;
          p += p->s.size;
          p->s.size = nunits;
        }
        huge_freep = prevp;
        return (void*)(p + 1);
      }
      if(p == huge_freep){
        if((p == morecore(nunits, VMALLOC_SIZE_HUGE)) == 0){
          return 0;
        }
      }
    }
  }else{
    if((prevp = freep) == 0){
      base.s.ptr = freep = prevp = &base;
      base.s.size = 0;
    }
    for(p = prevp->s.ptr; ; prevp = p, p = p->s.ptr){
      if(p->s.size >= nunits){
        if(p->s.size == nunits)
          prevp->s.ptr = p->s.ptr;
        else {
          p->s.size -= nunits;
          p += p->s.size;
          p->s.size = nunits;
        }
        freep = prevp;
        return (void*)(p + 1);
      }
      if(p == freep)
        if((p = morecore(nunits, VMALLOC_SIZE_BASE)) == 0)
          return 0;
    }
  }
}

void*
vmalloc(uint nbytes, int flag) 
{
  Header *p, *prevp;
  uint nunits;

  if(flag != VMALLOC_SIZE_BASE && flag != VMALLOC_SIZE_HUGE) {
    printf(2, "Please pass VMALLOC_SIZE_BASE or VMALLOC_SIZE_HUGE as flag\n");
    exit();
  }
  
  nunits = (nbytes + sizeof(Header) - 1)/sizeof(Header) + 1;
  if(flag == VMALLOC_SIZE_BASE) {
    // base page allocation, same as malloc() above
    if((prevp = freep) == 0){
      base.s.ptr = freep = prevp = &base;
      base.s.size = 0;
    }
    for(p = prevp->s.ptr; ; prevp = p, p = p->s.ptr){
      if(p->s.size >= nunits){
        if(p->s.size == nunits)
          prevp->s.ptr = p->s.ptr;
        else {
          p->s.size -= nunits;
          p += p->s.size;
          p->s.size = nunits;
        }
        freep = prevp;
        return (void*)(p + 1);
      }
      if(p == freep)
        if((p = morecore(nunits, VMALLOC_SIZE_BASE)) == 0)
          return 0;
    }
  } else { // VMALLOC_SIZE_HUGE
    // alloc huge page
    if((prevp = huge_freep) == 0) {
      huge_base.s.ptr = huge_freep = prevp = &huge_base;
      huge_base.s.size = 0;
    }

    for(p = prevp->s.ptr; ; prevp = p, p = p->s.ptr){
      if(p->s.size >= nunits){
        if(p->s.size == nunits)
          prevp->s.ptr = p->s.ptr;
        else {
          p->s.size -= nunits;
          p += p->s.size;
          p->s.size = nunits;
        }
        huge_freep = prevp;
        return (void*)(p + 1);
      }
      if(p == huge_freep)
        if((p = morecore(nunits, VMALLOC_SIZE_HUGE)) == 0)
          return 0;
    }
  }
}

void
vfree(void *ap)
{
  Header *bp, *p;

  if((uint)ap >= KERNBASE) {
    printf(2, "Error - trying to free a kernel pointer in userspace\n");
    exit();
  }

  bp = (Header*) ap - 1;
  for(p = huge_freep; !(bp > p && bp < p->s.ptr); p = p->s.ptr)
    if(p >= p->s.ptr && (bp > p || bp < p->s.ptr))
      break;
  if(bp + bp->s.size == p->s.ptr){
    bp->s.size += p->s.ptr->s.size;
    bp->s.ptr = p->s.ptr->s.ptr;
  }else{
    bp->s.ptr = p->s.ptr;
  }
  if(p + p->s.size == bp){
    p->s.size += bp->s.size;
    p->s.ptr = bp->s.ptr;
  }else{
    p->s.ptr = bp;
  }
  huge_freep = p;
}