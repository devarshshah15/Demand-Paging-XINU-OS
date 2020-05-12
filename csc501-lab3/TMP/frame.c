#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <paging.h>

/*-------------------------------------------------------------------------
 * init_frm - initialize frm_tab
 *-------------------------------------------------------------------------
 */
 extern int page_replace_policy;
fr_map_t frm_tab[NFRAMES];
SYSCALL init_frm()
{
	STATWORD ps;
	disable(ps);
  	 /* frm_tab is the inverted page table */
  int i;
  i=0;
  while(i<NFRAMES)
  {
	  initialize_frm(i);
	  i++;
  }
  restore(ps);
  return OK;
}

void initialize_frm(int i)
{
	pt_t *page_table_pointer;
	page_table_pointer = (pt_t *)((FRAME0+i)*NBPG);
	frm_tab[i].fr_type = FR_PAGE;
	int dd=0;
	frm_tab[i].fr_status = FRM_UNMAPPED;
	frm_tab[i].fr_dirty = 0;
	frm_tab[i].fr_refcnt = 0;
	int k=1;
	while(k<NFRAMES)
	{
		page_table_pointer->pt_pres = 0;
		page_table_pointer->pt_user= 0;
		page_table_pointer->pt_write = 0;
		page_table_pointer->pt_pcd = 0;
		page_table_pointer->pt_pwt = 0;
		page_table_pointer->pt_acc = 0;
		page_table_pointer->pt_dirty = 0;
		page_table_pointer->pt_global = 0;
		page_table_pointer->pt_mbz = 0;
		page_table_pointer->pt_avail = 0;
		page_table_pointer->pt_base = 0;
		page_table_pointer++;
		k++;
	}
	k=0;
	while(k<50)
	{
		frm_tab[i].fr_pid[k]=0;
		frm_tab[i].fr_vpno[k] = 4096;
		k++;
	}
}

/*-------------------------------------------------------------------------
 * get_frm - get a free frame according page replacement policy
 *-------------------------------------------------------------------------
 */
SYSCALL get_frm(int* avail)
{
	STATWORD ps;
	disable(ps);	
	*avail = -1;
	int i;
	i=0;
	while(i<NFRAMES)
	{
		if(frm_tab[i].fr_status == FRM_UNMAPPED)
		{
			*avail = i;
			sc_flag=1;
			return OK;
		}
		i++;
	}
	/* if no free frame found look into the page scheduling policy and find a replacable frame */
	if(page_replace_policy==LFU)
	{
		int ret = frame_replace_LFU();
		kprintf("Replacing Frame :%d\n",ret+FRAME0);
		free_frm(ret);
		*avail = ret;
		restore(ps);
		return OK;
	}
	else if(page_replace_policy==SC)
	{
		int ret =  frame_replace_sc_queue();
		kprintf("Replacing Frame :%d\n",ret+FRAME0);
		*avail = ret;
		restore(ps);
		return OK;
	}
  restore(ps);
  return SYSERR;
}
int frame_replace_LFU()
{
	int m;
	int min=0x7fffffff;
	int frame = -1;
	m=0;
	while(m<1024)
	{
		if(frm_tab[m].fr_type == FR_PAGE)
		{
			if(frm_tab[m].fr_cnt == min)
			{
				if(frm_tab[frame].fr_vpno<frm_tab[m].fr_vpno)
				{
					frame = m;
				}
			}
			else if(frm_tab[m].fr_cnt < min)
			{
				min = frm_tab[m].fr_cnt;
				frame = m;
			}
		}
		m++;
	}
	return frame;
}

int frame_replace_sc_queue()
{
	int k, pid=currpid, found=0;
	sc_flag = 0;
	k=0;
	while(found==0)
	{
	while(k<50)
	{
		if(frm_tab[sc_current].fr_vpno[k]>4096)
		{
			pid = k;
			break;
		}
		k++;
	}
	int vpno = frm_tab[sc_current].fr_vpno[pid];
	int pd_offset = ((vpno>>10)&0x000003ff),pt_offset = ((vpno&0x000003ff));
	pt_t *paget_offset;
	pd_t *paged_offset;
	
	paged_offset = (pd_offset * sizeof(pd_t))+proctab[k].pdbr;
	paget_offset = (pt_offset* sizeof(pt_t))+(paged_offset->pd_base * 4096);
	sc_current = sc_q[sc_current].next;
	if(paget_offset->pt_acc !=1)
	{
		found = 1;
		free_frm(paget_offset->pt_base - FRAME0);
		break; 
	}
	else
	{
		paget_offset->pt_acc =0;
		break; 
	}
	}
	return sc_q[sc_current].prev;
}

/*	called only for freeing page directory	*/
int reset_frame(int i)
{
	if(i<6)
	{
		return OK;
	}
	else
	{
		frm_tab[i].fr_type = FR_PAGE;
		frm_tab[i].fr_status = FRM_UNMAPPED;
		frm_tab[i].fr_refcnt = 0;
		return OK;
	}
}


/*-------------------------------------------------------------------------
 * free_frm - free a frame 
 *-------------------------------------------------------------------------
 */
SYSCALL free_frm(int i)
{
	STATWORD ps;
	disable(ps);
	/* if the frame is unmapped or frame type is not frame page return error */
	if(frm_tab[i].fr_status == FRM_UNMAPPED)
	{
		restore(ps);
		return SYSERR;
	}
	else
	{

	}
	if(frm_tab[i].fr_type!=FR_PAGE )
	{
		if(i>3)
		{
			initialize_frm(i);
		}
		restore(ps);
		return OK;
	}
	int  pageth=-1,store=-1;
	int fr_vpno = frm_tab[i].fr_vpno[currpid],fr_pid = currpid;
	int z=fr_vpno*4096;
	if(bsm_lookup(fr_pid, z, &store, &pageth)==SYSERR)
	{
		restore(ps);
		return SYSERR;
	}
	write_bs((FRAME0+i)*NBPG, store, pageth);	/* write the frame back to backing store */
	pd_t *pdbr,*paged_entry ;
	pt_t *paget_entry;
	virt_addr_t *virtual_address;
	virtual_address->pd_offset = fr_vpno/1024; 
	virtual_address->pt_offset = fr_vpno;
	pdbr = proctab[fr_pid].pdbr;
	paged_entry = (pd_t *)((( virtual_address->pd_offset * sizeof(pd_t))+proctab[fr_pid].pdbr));
	paget_entry = (pt_t *)(((virtual_address->pt_offset) * sizeof(pt_t))+(paged_entry->pd_base*NBPG));
	if(paget_entry->pt_pres ==1)
	{
		paget_entry->pt_pres = 0;		/* update in page table unmapped frame is not present */
		frm_tab[paged_entry->pd_base - FRAME0].fr_refcnt -=1; 		/*  update the frame ref count of page table frame */
		initialize_frm(paget_entry->pt_base - FRAME0);
	if(frm_tab[paged_entry->pd_base - FRAME0].fr_refcnt==0)
	{
		paged_entry->pd_pres=0;
		initialize_frm(paged_entry->pd_base - FRAME0);
	}
	else
	{
		
	}
	}
	restore(ps);
	return OK;
}


