/* vgetmem.c - vgetmem */

#include <conf.h>
#include <kernel.h>
#include <mem.h>
#include <proc.h>
#include <paging.h>

extern struct pentry proctab[];
/*------------------------------------------------------------------------
 * vgetmem  --  allocate virtual heap storage, returning lowest WORD address
 *------------------------------------------------------------------------
 */
WORD	*vgetmem(nbytes)
unsigned nbytes;
{
	STATWORD ps;    
	struct	mblock	*i, *j, *leftover;
	disable(ps);
	if (nbytes==0 || proctab[currpid].vmemlist->mnext== (struct mblock *) NULL || nbytes>proctab[currpid].vmemlist->mlen) 
	{
		restore(ps);
		return( (WORD *)SYSERR);
	}
	nbytes = (unsigned int) roundmb(nbytes);
	for (j= proctab[currpid].vmemlist,i=proctab[currpid].vmemlist->mnext ;
	     i != (struct mblock *) NULL ;
	     j=i,i=i->mnext)
	{
		if (j->mlen > nbytes) 
		{
			leftover = (struct mblock *)( nbytes+(unsigned)i );
			j->mnext = leftover;
			j->mlen -= nbytes;
			restore(ps);
			return((WORD *)i);
		}
		else if (j->mlen == nbytes) 
		{
			j->mnext = (struct mblock*)(nbytes+(unsigned)i );
			j->mlen =0;
			restore(ps);
			return((WORD *)i);
		} 
	}
	restore(ps);
	return( SYSERR );
}
