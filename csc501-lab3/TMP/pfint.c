#include <conf.h>
#include <kernel.h>
#include <paging.h>
#include <proc.h>

/*-------------------------------------------------------------------------
 * pfint - paging fault ISR
 *-------------------------------------------------------------------------
 */
SYSCALL pfint()
{
  	STATWORD ps;
	disable(ps);

	int pageth=-1,store =-1,new_pt_frame_number, new_pt_page_number=0;
	virt_addr_t *virtual_address;
	unsigned long fault_address = read_cr2();
	int pd_offset=(fault_address>>22)&(0x000003ff);
	int pt_offset = (fault_address>>12)&(0x000003ff);
  
  int qq=0;
  
  if(bsm_lookup(currpid, fault_address, &store, &pageth) == SYSERR)
  {		
	  kill(currpid);
	  restore(ps);
	  return SYSERR;
  }
  
  pd_t *pd= (pd_t *)(((pd_offset)*4)+proctab[currpid].pdbr);
  if(pd->pd_pres ==0)
  {		
    /* page is not present in main memory, get it from backing store */
  	int frame_number = -1;
	if(get_frm(&frame_number)==-1)
	{
		return -1;
	}
	pt_t *page_table_pointer = (pt_t *)((frame_number+FRAME0)*NBPG);
	
	frm_tab[frame_number].fr_status = FRM_MAPPED;
	frm_tab[frame_number].fr_type = FR_TBL;
	frm_tab[frame_number].fr_vpno[currpid] = 4096;
	frm_tab[frame_number].fr_refcnt = 0;
	frm_tab[frame_number].fr_dirty = 0;
	int k;
	k=0;
	while(k<50)
	{
		frm_tab[frame_number].fr_pid[k]=0;
		frm_tab[frame_number].fr_vpno[k] = 4096;
		k++;
	}
	page_table_pointer->pt_base = (FRAME0+frame_number);
	page_table_pointer++;
	k=1;
	while(k<NFRAMES)
	{
		page_table_pointer->pt_pres = 0;
		page_table_pointer->pt_write = 1;
		page_table_pointer->pt_user= 1;
		page_table_pointer->pt_pwt = 1;
		page_table_pointer->pt_pcd = 0;
		page_table_pointer->pt_acc = 0;
		page_table_pointer->pt_dirty = 0;
		page_table_pointer->pt_mbz = 0;
		page_table_pointer->pt_global = 0;
		page_table_pointer->pt_avail = 0;
		page_table_pointer->pt_base = 0;
		page_table_pointer++;
		k++;
	}
	//new_pt_frame_number = frame_number;

	if((new_pt_frame_number=frame_number)==-1)
	{
	  restore(ps);
	  return SYSERR;
	}
	frm_tab[new_pt_frame_number].fr_type = FR_TBL;
	frm_tab[new_pt_frame_number].fr_refcnt = 0;
	frm_tab[new_pt_frame_number].fr_status = FRM_MAPPED;
	frm_tab[new_pt_frame_number].fr_pid[currpid] = 1;
	pd->pd_write = 1;
	pd->pd_pres =1;
	pd->pd_base = new_pt_frame_number+FRAME0;	/* set the base frame of new page in page table */
  }
  pt_t *pt=((pt_offset*sizeof(pt_t))+(pd->pd_base*NBPG));
 
	int ret =-1,k=0,share_frame;
	unsigned long virtual_pno;
	pd_t *pd_offset1 ;
	pt_t *pt_offset1;
	if(bsm_tab[store].bs_private==1)
	{
		share_frame=ret;
	}
	k=0;
	while(k<50)
	{
		if(bsm_tab[store].bs_pid[k]==1&&k!=currpid)
		{
			// get the virtual address of pageth of this process
			virtual_pno = pageth+bsm_tab[store].bs_vpno[k];
			pd_offset1 = ((virtual_pno/1024)*4)+proctab[k].pdbr;
			if(pd_offset1->pd_pres==1)
			{
				pt_offset1 = ((virtual_pno&0x000003ff)*sizeof(pt_t))+(pd_offset1->pd_base*NBPG);
				if(pt_offset1->pt_pres==1)
				{
					ret = pt_offset1->pt_base;
					share_frame=ret;
				}
			}
		}
		k++;
	}
	share_frame = ret; 
	
	if(share_frame !=-1)
	{
		
		pt->pt_pres = 1;
		pt->pt_base = share_frame;
		frm_tab[share_frame-FRAME0].fr_pid[currpid]=1;
		frm_tab[share_frame-FRAME0].fr_refcnt++;
		unsigned long y=fault_address/NBPG;
		frm_tab[share_frame-FRAME0].fr_vpno[currpid] = y;
		restore(ps);
		return OK;
	}
	
	get_frm(&new_pt_page_number);
	if(get_frm(&new_pt_page_number)==-1)
	{
		restore(ps);
		return SYSERR;
	}
	if(sc_flag==1)
	{
		sc_flag = 0;
		if(sc_tail==-1)
		{
			/* implies queue is empty update the tail, current */
			sc_tail=new_pt_page_number;
			sc_current = new_pt_page_number;
			sc_q[new_pt_page_number].next = new_pt_page_number;
			sc_q[new_pt_page_number].prev = new_pt_page_number;
		}
		else
		{
			sc_q[new_pt_page_number].next = sc_q[sc_tail].next;
			sc_q[sc_tail].next =new_pt_page_number;
			sc_q[new_pt_page_number].prev = sc_tail;
			sc_q[sc_q[new_pt_page_number].next].prev = new_pt_page_number;
		}
		sc_tail = new_pt_page_number;
					/* Insert into SC  queue only if the returned frame is new and not shared	*/
	}
	
	frm_tab[(pd->pd_base)-FRAME0].fr_refcnt += 1;
	frm_tab[new_pt_page_number].fr_pid[currpid] = 1;
	frm_tab[new_pt_page_number].fr_status = FRM_MAPPED;
	frm_tab[new_pt_page_number].fr_vpno[currpid] = fault_address/NBPG;
	frm_tab[new_pt_page_number].fr_refcnt = 1;
	frm_tab[new_pt_page_number].fr_dirty = 0;
	frm_tab[new_pt_page_number].fr_type = FR_PAGE;
	pt->pt_dirty=1;
	pt->pt_write =1;
	pt->pt_pres =1;
	pt->pt_user=1;
	pt->pt_global=0;
	pt->pt_base = new_pt_page_number+FRAME0;
	unsigned long jj=0;
	read_bs((char *)(pt->pt_base * NBPG),store, pageth);
	write_cr3(proctab[currpid].pdbr);
	restore(ps);
	return OK;
}



void setPageDirectory(int pid)
{
	STATWORD ps;
	disable(ps);
	int frame_number =0,i;
	pd_t *page_directory;
	get_frm(&frame_number);
	proctab[pid].pdbr = (FRAME0+frame_number)*NBPG;	
	frm_tab[frame_number].fr_status = FRM_MAPPED;
	frm_tab[frame_number].fr_pid[pid] = 1;
	frm_tab[frame_number].fr_type = FR_DIR;
	page_directory = (pd_t *)proctab[pid].pdbr;
	i=0;
	while(i<NFRAMES)
	{
		page_directory->pd_pres= 0;
		page_directory->pd_write = 0;
		page_directory->pd_user=0;
		page_directory->pd_pwt=0;
		page_directory->pd_pcd=0;
		page_directory->pd_acc=0;
		page_directory->pd_mbz=0;
		page_directory->pd_fmb=0;
		page_directory->pd_global=0;
		page_directory->pd_avail=0;
		page_directory->pd_base= 0;
		if(i<4)
		{
			page_directory->pd_pres= 1;
			page_directory->pd_write = 1;
			page_directory->pd_base= ((FRAME0+i));
		}
		page_directory++;
		i++;
	}
	restore(ps);
	return OK;
}