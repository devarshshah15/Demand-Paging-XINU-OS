#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <paging.h>

SYSCALL release_bs(bsd_t bs_id) 
{

  /* release the backing store with ID bs_id */
	bs_map_t* main_bs;
	main_bs=&bsm_tab[bs_id];
	STATWORD ps;
	disable(ps);
	if(bs_id<0 || bs_id>=NBSM)
	{
		restore(ps);
		return  SYSERR;
	}
	if(main_bs->bs_status==BSM_UNMAPPED)
	{
		restore(ps);
		return OK;
	}
	if(main_bs->bs_private == 1)
	{			/*	if bs is private release it		*/
		free_bsm(bs_id);
		restore(ps);
		return OK;
	}
	else if(main_bs->bs_private != 1)
	{
		main_bs->bs_pid[currpid] = 0;
		main_bs->bs_vpno[currpid] = 4096;
		main_bs->bs_npages[currpid] = 0;
		if(proctab[currpid].backing_store_map[bs_id]==1)
		{
			proctab[currpid].backing_store_map[bs_id]=0;
			main_bs->ref_count--;
		}
		if(main_bs->ref_count ==0)
		{
			free_bsm(bs_id);
		}
		restore(ps);
		return OK;
	}
}