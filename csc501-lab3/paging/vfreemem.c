#include <conf.h>
#include <kernel.h>
#include <mem.h>
#include <proc.h>

extern struct pentry proctab[];
/*------------------------------------------------------------------------
 *  vfreemem  --  free a virtual memory block, returning it to vmemlist
 *------------------------------------------------------------------------
 */
SYSCALL	vfreemem(block, size)
	struct	mblock	*block;
	unsigned size;
{
	STATWORD ps; 
	disable(ps);	
	struct	mblock *vpointer;
	vpointer = proctab[currpid].vmemlist;
	size = (unsigned)roundmb(size);
	unsigned top;
	struct pentry *pptr;
	
	
	if (size==0 || ((unsigned)block)<((unsigned) &end) || (((unsigned)block+size) > vpointer->mnext) ){
		restore(ps);
		return(SYSERR);
	}
	else if((((unsigned)block+size) == vpointer->mnext)){
		vpointer->mnext = (unsigned)block;
		vpointer->mlen += size; 
		restore(ps);
		return OK;
	}
	else
	{
		vpointer->mnext -= size;
		vpointer->mlen += size; 
		restore(ps);
		return OK;
	}
}