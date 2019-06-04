#include "param.h"
#include "types.h"
#include "defs.h"
#include "x86.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"

struct {
  struct spinlock lock;
  struct shm_page {
    uint id;
    char *frame;
    int refcnt;
  } shm_pages[64];
} shm_table;

void shminit() {
  int i;
  initlock(&(shm_table.lock), "SHM lock");
  acquire(&(shm_table.lock));
  for (i = 0; i< 64; i++) {
    shm_table.shm_pages[i].id =0;
    shm_table.shm_pages[i].frame =0;
    shm_table.shm_pages[i].refcnt =0;
  }
  release(&(shm_table.lock));
}

int shm_open(int id, char **pointer) { //Lab 4: implement
    struct proc *curproc = myproc();
    uint va, pa;
    pde_t *pgdir;
    int perm, i;
    int done = 0;

    acquire(&(shm_table.lock));
    for (i = 0; i < 64; ++i) {
	if (shm_table.shm_pages[i].id == id) { //Condition 1: if id already exists
	    pgdir = curproc->pgdir;
	    va = PGROUNDUP(curproc->sz);
	    pa = V2P(shm_table.shm_pages[i].frame);
	    perm = PTE_W|PTE_U;
	    
	    if (mappages(pgdir, (void *)va, PGSIZE, pa, perm) != -1) {
		shm_table.shm_pages[i].refcnt += 1; // Increment reference count
		*pointer = (char *)va; // Set pointer to new address
		curproc->sz += PGSIZE; // Update sz
		done = 1;
	  	break;
	    }
	}
    }

    for (i = 0; i < 64; ++i) {
	if (done) {break;}
	if (shm_table.shm_pages[i].id == 0) { //Condition 2: if id not found
	    // add id to table
	    char * p=kalloc();
	    memset(p,0,PGSIZE);
 
	    va = PGROUNDUP(curproc->sz);
	    pgdir = curproc->pgdir;
	    perm = PTE_W|PTE_U;

	    if ( mappages(pgdir, (void *)va, PGSIZE, V2P(p), perm) != -1) {
		shm_table.shm_pages[i].id = id;
	        shm_table.shm_pages[i].frame = p;
        	shm_table.shm_pages[i].refcnt = 1;

		*pointer = (char *)va;
		curproc->sz += PGSIZE;
		done = 1;
		break;
	    }
	}
    }

    release(&(shm_table.lock));

    if (done) { return 0; }

    return -1; //something went wrong
}


int shm_close(int id) { //Lab 4: implement
    int i;
    int done = 0;

    acquire(&(shm_table.lock));

    for (i = 0; i < 64; ++i) {
	if (shm_table.shm_pages[i].id == id) {
	    shm_table.shm_pages[i].refcnt -= 1;

	    if (shm_table.shm_pages[i].refcnt <= 0) {
	        shm_table.shm_pages[i].id = 0; 
	        shm_table.shm_pages[i].frame = 0;
	        shm_table.shm_pages[i].refcnt = 0;
	    }
	    done = 1;
	    break;
	}
    }

    if (done) {
	release(&(shm_table.lock));
	return 0;
    }
    release(&(shm_table.lock));

    return -1; //id not found
}
