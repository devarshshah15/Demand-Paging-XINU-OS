  .text
pferrcode: .long 0
           .globl  pfintr,pferrcode 
pfintr:
	popl pferrcode
	pushfl
	cli	 			/*	disabled interrupt	*/
	pushal
	call pfint		/*	interrupt handler	*/
	popal
	popfl
	iret