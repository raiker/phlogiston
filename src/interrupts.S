.section ".text.ivt"

.global "ivt_start"

ivt_start:
	b reset_vec
	b undef_vec
	b svc_vec
	b prefetch_abort_vec
	b data_abort_vec
	nop
	b irq_vec
	b fiq_vec

reset_vec:
	//should never be reached

undef_vec:
	movs pc, lr
	
svc_vec:
	push {lr}
	bl svc_entry
	pop {lr}
	movs pc, lr

prefetch_abort_vec:
	movs pc, lr

data_abort_vec:
	movs pc, lr
	
irq_vec:
	movs pc, lr
	
fiq_vec:
	movs pc, lr
	

