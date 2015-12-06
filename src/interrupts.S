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
	sub lr, lr, #4
	//srsdb sp!, #0x13 //write lr and spsr to supervisor stack
	push {lr,pc} //pc is dummy
	mrs lr, spsr
	str lr, [sp, #0x4]
	
	stmdb sp!, {r0-r14}^
	
	//if we were just in privileged mode, we've grabbed the wrong lr and sp
	//lr holds spsr value
	//switch to previous mode
	mov r0, sp
	and r1, lr, #0x1f
	
	//if we were in user, the registers are fine
	cmp r2, #0x10
	beq irq_regs_saved //moveq r2, #0x1f
	
	mrs r1, cpsr
	orn r1, r1, #0x1f
	orr r3, r2, r1
	msr r3, cpsr
	
	str lr, [r0, #0x38]
	str sp, [r0, #0x3c]
	
	//switch back
	orr r3, r1, #0x13
	msr r3, cpsr
	
irq_regs_saved:
	bl irq_handler
	
	//load target thread's cpsr
	ldr r0, [sp, #0x44]
	
	//get mode
	and r1, r0, #0x1f
	cmp r1, #0x10
	beq irq_load_regs
	
	mrs r2, cpsr
	orn r2, r2, #0x1f
	orr r
	
	ldmia sp!, {r0-r14}^
	
	rfeia sp!
	
fiq_vec:
	push {lr}
	bl fiq_handler
	pop {lr}
	subs pc, lr, #4
	

