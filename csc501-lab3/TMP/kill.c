#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <sem.h>
#include <mem.h>
#include <io.h>
#include <q.h>
#include <stdio.h>
#include <paging.h>

/*------------------------------------------------------------------------
 * kill  --  kill a process and remove it from the system
 *------------------------------------------------------------------------
 */
SYSCALL kill(int pid)
{
	STATWORD ps;    
	struct	pentry	*pptr;		/* points to proc. table for pid*/
	int	dev, k;

	disable(ps);
	if (isbadpid(pid) || (pptr= &proctab[pid])->pstate==PRFREE) {
		restore(ps);
		return(SYSERR);
	}
	if (--numproc == 0)
		xdone();

	dev = pptr->pdevs[0];
	if (! isbaddev(dev) )
		close(dev);
	dev = pptr->pdevs[1];
	if (! isbaddev(dev) )
		close(dev);
	dev = pptr->ppagedev;
	if (! isbaddev(dev) )
		close(dev);
	
	send(pptr->pnxtkin, pid);

	freestk(pptr->pbase, pptr->pstklen);
	
	k=0;
	while(k<NBSM)
	{
			/* all mappings should be removed from backing store map */
		if(proctab[pid].backing_store_map[k]==1&&(bsm_unmap(pid, bsm_tab[k].bs_vpno[pid], 1)==SYSERR))
		{
			
				restore(ps);
				return SYSERR;
			
		}
		k++;
	}
	
	k=4;
	while(k<NFRAMES)
	{		/* all page tables,  should be freed */
		if(frm_tab[k].fr_type==FR_TBL && frm_tab[k].fr_pid[pid]==1)
		{
			free_frm(k);
		}
		k++;
	}
	
	reset_frame((proctab[pid].pdbr>>12)-FRAME0);		
	proctab[pid].pdbr=0;
	
	
	switch (pptr->pstate) 
	{

	case PRCURR:	pptr->pstate = PRFREE;	/* suicide */
			resched();

	case PRWAIT:	semaph[pptr->psem].semcnt++;

	case PRREADY:	dequeue(pid);
			pptr->pstate = PRFREE;
			break;

	case PRSLEEP:
	case PRTRECV:	unsleep(pid);
						/* fall through	*/
	default:	pptr->pstate = PRFREE;
	}
	restore(ps);
	return(OK);
}