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
	//replace with attribute code
	push {lr}
	sub r0, lr, #4
	bl undef_instr_handler
	pop {lr}
	movs pc, lr
	
svc_vec:
	push {lr}
	bl svc_entry
	pop {lr}
	movs pc, lr

prefetch_abort_vec:
	push {lr}
	sub r0, lr, #4
	bl prefetch_abort_handler
	pop {lr}
	subs pc, lr, #4

data_abort_vec:
	push {lr}
	sub r0, lr, #8
	bl data_abort_handler
	pop {lr}
	subs pc, lr, #8
	
irq_vec:
	push {lr}
	bl irq_handler
	pop {lr}
	subs pc, lr, #4
	
fiq_vec:
	push {lr}
	bl fiq_handler
	pop {lr}
	subs pc, lr, #4
	

