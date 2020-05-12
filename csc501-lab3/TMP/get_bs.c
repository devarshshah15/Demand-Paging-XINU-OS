#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <paging.h>

int get_bs(bsd_t bs_id, unsigned int npages) 
{

  /* requests a new mapping of npages with ID map_id */
	STATWORD ps;
	disable(ps);
	bs_map_t* main_bs;
	main_bs=&bsm_tab[bs_id];
	if((npages<=0 || npages>128)||(bs_id<0 || bs_id>=NBSM))
	{
		restore(ps);
		return SYSERR;
	}
	if(main_bs->bs_status == BSM_UNMAPPED)
	{
		/* map this using the bsm_map */
		if(bsm_map(currpid, 4096, bs_id, npages)==SYSERR)
		{
			restore(ps);
			return SYSERR;
		}
		main_bs->max_npages = npages;
		restore(ps);
		return npages;
	}
	else
	{
		if(main_bs->bs_status == BSM_MAPPED && main_bs->bs_private)
		{ 
			restore(ps);
			return SYSERR;
		}
		if(npages >main_bs->max_npages)
		{
			restore(ps);
			return main_bs->max_npages;
		}
		proctab[currpid].backing_store_map[bs_id]=1;
		main_bs->bs_pid[currpid] = 1;
		main_bs->bs_npages[currpid] = npages; 
		main_bs->bs_vpno[currpid] = 4096;
		main_bs->ref_count++;
		
		restore(ps);
		return main_bs->max_npages;
	}

}
