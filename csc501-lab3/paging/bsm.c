#include <conf.h>
#include <kernel.h>
#include <paging.h>
#include <proc.h>

/*-------------------------------------------------------------------------
 * init_bsm- initialize bsm_tab
 *-------------------------------------------------------------------------
 */
bs_map_t bsm_tab[NBSM];
SYSCALL init_bsm()
{
	STATWORD ps;
	disable(ps);
	int i;
	// assign 2048 address to bsm_tab[0]
	// since we have 16 backing stores
	i=0;
	while(i<NBSM)
	{
		free_bsm(i);
		i++;
	}
	restore(ps);
	return OK;
}

/*-------------------------------------------------------------------------
 * get_bsm - get a free entry from bsm_tab 
 *-------------------------------------------------------------------------
 */
SYSCALL get_bsm(int* avail)
{
	STATWORD ps;
	disable(ps);
	*avail = 0;
	int i;
	i=0;
	while(i<NBSM)
	{
		//kprintf("current bs is %d, status = %d\n",i, bsm_tab[i].bs_status);
		if(bsm_tab[i].bs_status == BSM_UNMAPPED)
		{
			*avail = i;
			restore(ps);
			return OK;
		}
		i++;
	}
	restore(ps);
	return SYSERR;
}


/*-------------------------------------------------------------------------
 * free_bsm - free an entry from bsm_tab 
 *-------------------------------------------------------------------------
 */
SYSCALL free_bsm(int i)
{
	STATWORD ps;
	disable(ps);
	if(i<0 || i>=NBSM)
	{
		restore(ps);
		return SYSERR;
	}
	bsm_tab[i].bs_status = BSM_UNMAPPED;
	int k;
	k=0;
	while(k<50)
	{
		bsm_tab[i].bs_vpno[k] = -1; 
		bsm_tab[i].bs_pid[k] = 0;
		bsm_tab[i].bs_npages[k] = 0;
		k++;
	}	
	int oo=0;
	bsm_tab[i].ref_count = 0;
	bsm_tab[i].bs_sem = 0;
	bsm_tab[i].bs_private =0;
	restore(ps);
	return OK;
}

/*-------------------------------------------------------------------------
 * bsm_lookup - lookup bsm_tab and find the corresponding entry
 *-------------------------------------------------------------------------
 */
SYSCALL bsm_lookup(int pid, unsigned long vaddr, int* store, int* pageth)
{
	STATWORD ps;
	disable(ps);
	unsigned long vpno = (vaddr&0xfffff000)>>12;
	int k;
	k=0;
	while(k<NBSM)
	{
	 if(vpno>=bsm_tab[k].bs_vpno[pid])
		{
			if(vpno<(bsm_tab[k].bs_vpno[pid]+bsm_tab[k].bs_npages[pid]))
			{
			if(bsm_tab[k].bs_pid[pid]==1)
			{
				*store = k;
				*pageth = (vaddr/NBPG)-bsm_tab[k].bs_vpno[pid];
				restore(ps);
				return OK;
			}
		}
		}
		k++;
	}
	kprintf("bsm failed for address %lu,  process %d", vaddr, pid);
	restore(ps);
	return SYSERR;
}


/*-------------------------------------------------------------------------
 * bsm_map - add an mapping into bsm_tab 
 *-------------------------------------------------------------------------
 */
SYSCALL bsm_map(int pid, int vpno, int source, int npages)
{
	STATWORD ps;
	disable(ps);
	if(npages<=0 || npages>128)
	{
		restore(ps);
		return SYSERR;
	}
	if(source<0||source>=NBSM)
	{
		restore(ps);
		return SYSERR;
	}
	proctab[pid].store = source;
	proctab[pid].backing_store_map[source]=1;
	bsm_tab[source].bs_pid[pid] = 1;
	bsm_tab[source].bs_status = BSM_MAPPED;
	bsm_tab[source].ref_count =1;
	bsm_tab[source].bs_npages[pid] = npages;
	bsm_tab[source].bs_vpno[pid] = vpno;
	restore(ps);
	return OK;
}

/*-------------------------------------------------------------------------
 * bsm_unmap - delete an mapping from bsm_tab and free relateed frames
 *-------------------------------------------------------------------------
 */
SYSCALL bsm_unmap(int pid, int vpno, int flag)
{
	/* will unmap given process from current backing store, free when all the process are unmapped unmap should save the page in backing store by calling write function before unmapping	*/
	STATWORD ps;
	disable(ps);
	int store, pageth;
	if(isbadpid(pid))
	{
		restore(ps);
		return SYSERR;
	}
	/* if flag is 1 then bsm_unmap is called while killing the process and 
	if flag is 0 then if the unmapping is for private heap throw an error */
	if((bsm_lookup(pid, vpno*4096, &store,&pageth)==SYSERR)||(bsm_tab[store].bs_status==BSM_UNMAPPED))
	{
		restore(ps);
		return SYSERR;
	}
	unsigned int k, temp;
	unsigned long vir_addr;
	int vpno_base = bsm_tab[store].bs_vpno[pid];
	pd_t *pd_offset;
	pt_t *pt_offset;
	/* unmapp all the frames of corresponding backing store for the current process */
	k = vpno_base;
	int t;
	t=0;
	while(bsm_tab[store].bs_npages[pid]>t)
	{
		vir_addr = k*4096;
		pd_offset = ((vir_addr>>22)*4)+(proctab[currpid].pdbr);
		temp = (vir_addr&(0x0003ff000))>>12;
		/*  fetch the middle 10 bits of virtual page address */
		if (pd_offset->pd_pres==1)
		{
			pt_offset = (temp*sizeof(pd_t))+(pd_offset->pd_base*4096);
			if ((pt_offset->pt_pres ==1) && (frm_tab[pt_offset->pt_base - FRAME0].fr_status==FRM_MAPPED))
			{	
					/*  write only if the frame is mapped */
					write_bs(pt_offset->pt_base * 4096, store, pageth );

					frm_tab[(pt_offset->pt_base) - FRAME0].fr_refcnt--;
					int xx=0;
					if(frm_tab[(pt_offset->pt_base) - FRAME0].fr_refcnt<=0)
					{
						int i=pt_offset->pt_base - FRAME0;
						if(sc_q[i].next!=i)
						{
							sc_q[sc_q[i].prev].next = sc_q[i].next;
							sc_q[sc_q[i].next].prev = sc_q[i].prev;
						}
						else
						{
							i=0;
							while(i<FRAME0) //initialize sc_queue
							{
							sc_q[i].next=-1;
							sc_q[i].prev=-1;
							i++;
							}
							sc_tail=-1;
							sc_current = -1;
							sc_q[i].next = -1;
							sc_q[i].prev = -1;
						}
						free_frm(pt_offset->pt_base - FRAME0);
					}			
			}
		}
		k++;
		t++;
	}
	

	/*  error if invalid npages or source, no process is mapped to bs, current process in not mapped to bs */
	struct pentry *pptr;
	pptr=&proctab[pid];
	
	if(pptr->backing_store_map[store]==1)
	{
			pptr->backing_store_map[store]=0;
			bsm_tab[store].ref_count--;
	}
	else
	{

	}
	bsm_tab[store].bs_pid[pid] = 0;
	bsm_tab[store].bs_vpno[pid] = 4096;
	int vv=0;
	bsm_tab[store].bs_npages[pid] = 0;
	
	if(bsm_tab[store].ref_count==0)
	{
		free_bsm(store);
	}
	restore(ps);
	return OK;
}