#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <paging.h>


/*-------------------------------------------------------------------------
 * xmmap - xmmap
 *-------------------------------------------------------------------------
 */
SYSCALL xmmap(int virtpage, bsd_t source, int npages)
{
	STATWORD ps;
	disable(ps);
	//Function is called when get_bs is called
	if(bsm_tab[source].bs_status== BSM_UNMAPPED)
	{
		restore(ps);
		return SYSERR;
	}
	else if(bsm_tab[source].bs_status== BSM_MAPPED && bsm_tab[source].bs_private==1)
	{
		restore(ps);
		return SYSERR;
	}
	else if(bsm_tab[source].bs_status== BSM_MAPPED && bsm_tab[source].bs_private!=1&& npages<=bsm_tab[source].max_npages)
	{ 
		bsm_tab[source].bs_vpno[currpid]=virtpage;
		bsm_tab[source].bs_pid[currpid]=1;
		bsm_tab[source].bs_npages[currpid]=npages;
		restore(ps);
		return OK;
	}
	else if(bsm_tab[source].bs_status== BSM_MAPPED && bsm_tab[source].bs_private!=1&& npages>bsm_tab[source].max_npages)
	{
		restore(ps);
		return SYSERR;
	}
}

/*-------------------------------------------------------------------------
 * xmunmap - xmunmap
 *-------------------------------------------------------------------------
 */
SYSCALL xmunmap(int virtpage)
{
  if(bsm_unmap(currpid, (virtpage), 0)!=OK){
	return SYSERR;
  }
  return OK;
}