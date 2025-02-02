#include <kernel.h>
#include <symtab.h>
#include <plic.h>

//#define INT_DEBUG
#ifdef INT_DEBUG
#define int_debug(x, ...) do {log_printf("  INT", x, ##__VA_ARGS__);} while(0)
#else
#define int_debug(x, ...)
#endif

static const char *interrupt_names[] = {
    "Instruction address misaligned",
    "Instruction access fault",
    "Illegal Instruction",
    "Breakpoint",
    "Load address misaligned",
    "Load access fault",
    "Store/AMO address misaligned",
    "Store/AMO access fault",
    "Environment call from U-mode",
    "Environment call from S-mode",
    "reserved 10",
    "Environment call from M-mode",
    "Instruction page fault",
    "Load page fault",
    "reserved 14",
    "Store/AMO page fault",
};


static const char *register_names[] = {
    "        pc",
    "        ra",
    "        sp",
    "        gp",
    "        tp",
    "        t0",
    "        t1",
    "        t2",
    "        fp",
    "        s1",
    "        a0",
    "        a1",
    "        a2",
    "        a3",
    "        a4",
    "        a5",
    "        a6",
    "        a7",
    "        s2",
    "        s3",
    "        s4",
    "        s5",
    "        s6",
    "        s7",
    "        s8",
    "        s9",
    "       s10",
    "       s11",
    "        t3",
    "        t4",
    "        t5",
    "        t6",
};

typedef struct inthandler {
    struct list l;
    thunk t;
    const char *name;
} *inthandler;

static struct list *handlers;

static heap int_general;

void frame_trace(u64 *fp)
{
    for (unsigned int frame = 0; frame < FRAME_TRACE_DEPTH; frame ++) {
        if (!validate_virtual(fp - 1 , sizeof(u64)) ||
            !validate_virtual(fp - 2, sizeof(u64)))
            break;

        u64 n = fp[-1];
        if (n == 0)
            break;
        print_u64(u64_from_pointer(fp - 1));
        rputs(":   ");
        fp = pointer_from_u64(fp[-2]);
        print_u64_with_sym(n);
        rputs("\n");
    }
}

void print_frame_trace_from_here(void)
{
    rputs("\nframe trace: \n");
    u64 fp;
    asm("mv %0, fp" : "=r" (fp));
    frame_trace(pointer_from_u64(fp));
}

void print_stack(context_frame c)
{
    rputs("\nframe trace: \n");
    frame_trace(pointer_from_u64(c[FRAME_FP]));

    rputs("\nstack trace:\n");
    u64 *sp = pointer_from_u64(c[FRAME_SP]);
    for (u64 *x = sp; x < (sp + STACK_TRACE_DEPTH) &&
             validate_virtual(x, sizeof(u64)); x++) {
        print_u64(u64_from_pointer(x));
        rputs(":   ");
        print_u64_with_sym(*x);
        rputs("\n");
    }
    rputs("\n");
}

void dump_context(context ctx)
{
    context_frame f = ctx->frame;
    u64 v = SCAUSE_CODE(f[FRAME_CAUSE]);
    boolean isint = SCAUSE_INTERRUPT(f[FRAME_CAUSE]);
    if (isint)
        rputs(" interrupt: ");
    else
        rputs(" exception: ");
    print_u64(v);

    if (!isint && v < sizeof(interrupt_names)/sizeof(interrupt_names[0])) {
        rputs(" (");
        rputs((char *)interrupt_names[v]);
        rputs(")");
    }
    rputs("\n     frame: ");
    print_u64_with_sym(u64_from_pointer(f));
    if (ctx->type >= CONTEXT_TYPE_UNDEFINED && ctx->type < CONTEXT_TYPE_MAX) {
        rputs("\n      type: ");
        rputs(context_type_strings[ctx->type]);
    }
    rputs("\nactive_cpu: ");
    print_u64(ctx->active_cpu);
    rputs("\n stack top: ");
    print_u64(f[FRAME_STACK_TOP]);
    rputs("\n    status: ");
    print_u64_with_sym(u64_from_pointer(f[FRAME_STATUS]));
    rputs("\n     stval: ");
    print_u64_with_sym(u64_from_pointer(f[FRAME_FAULT_ADDRESS]));
    rputs("\n");
    for (int i = 0; i < sizeof(register_names)/sizeof(register_names[0]); i++) {
        rputs(register_names[i]);
        rputs(": ");
        print_u64_with_sym(f[i]);
        rputs("\n");
    }
    print_stack(f);
}

void register_interrupt(int vector, thunk t, const char *name)
{
    // XXX ignore handlers for vector 0 (i.e. not implemented)
    if (vector == 0)
        return;
    boolean initialized = !list_empty(&handlers[vector]);
    int_debug("%s: vector %d, thunk %p (%F), name %s%s\n",
              __func__, vector, t, t, name, initialized ? ", shared" : "");

    inthandler h = allocate(int_general, sizeof(struct inthandler));
    assert(h != INVALID_ADDRESS);
    h->t = t;
    h->name = name;
    list_insert_before(&handlers[vector], &h->l);

    if (!initialized) {
        plic_set_int_priority(vector, 1);
        plic_clear_pending_int(vector);
        plic_enable_int(vector);
    }
}

void unregister_interrupt(int vector)
{
    // XXX ignore handlers for vector 0 (i.e. not implemented)
    if (vector == 0)
        return;
    int_debug("%s: vector %d\n", __func__, vector);
    plic_disable_int(vector);
    if (list_empty(&handlers[vector]))
        halt("%s: no handler registered for vector %d\n", __func__, vector);
    list_foreach(&handlers[vector], l) {
        inthandler h = struct_from_list(l, inthandler, l);
        int_debug("   remove handler %s (%F)\n", h->name, h->t);
        list_delete(&h->l);
        deallocate(int_general, h, sizeof(struct inthandler));
    }
}

extern void *trap_handler;

void riscv_timer(void)
{
    /* disable timer via sbi */
    supervisor_ecall(SBI_SETTIME, -1ull);
    schedule_timer_service();
}

void trap_interrupt(void)
{
    cpuinfo ci = current_cpu();
    context ctx = get_current_context(ci);
    context_frame f = ctx->frame;

    if (f[FRAME_FULL])
        halt("\nframe %p already full\n", f);

    f[FRAME_FULL] = true;
    context_reserve_refcount(ctx);

    int saved_state = ci->state;
    switch (SCAUSE_CODE(f[FRAME_CAUSE])) {
    case TRAP_I_SSOFT:
        console("software interrupt\n"); // XXX need to implement for smp
        break;
    case TRAP_I_STIMER:
        int_debug("[%2d] timer interrupt, state %s\n", ci->id,
                state_strings[ci->state]);
        riscv_timer();
        break;
    case TRAP_I_SEXT: {
        u64 i;
        while ((i = plic_dispatch_int())) {
            int_debug("[%2d] # %d, state %s user %s\n", ci->id, i,
                    state_strings[ci->state], (f[FRAME_STATUS]&STATUS_SPP)?"false":"true" );

            if (i > PLIC_MAX_INT)
                halt("dispatched interrupt %d exceeds PLIC_MAX_INT\n", i);

            if (list_empty(&handlers[i]))
                halt("no handler for interrupt %d\n", i);

            list_foreach(&handlers[i], l) {
                inthandler h = struct_from_list(l, inthandler, l);
                int_debug("   invoking handler %s (%F)\n", h->name, h->t);
                ci->state = cpu_interrupt;
                apply(h->t);
            }

            int_debug("   eoi %d\n", i);
            plic_eoi(i);
        }
        break;
    }
    default:
        assert(0);
    }

    /* enqueue interrupted user thread */
    if (is_thread_context(ctx) && !shutting_down) {
        int_debug("int sched %p\n", ctx);
        context_schedule_return(ctx);
    }

    if (is_kernel_context(ctx) || is_syscall_context(ctx)) {
        context_release_refcount(ctx);
        if (saved_state != cpu_idle) {
            ci->state = cpu_kernel;
            frame_return(f);
        }
        f[FRAME_FULL] = false;      /* no longer saving frame for anything */
    }
    int_debug("   calling runloop\n");
    runloop();
}

extern void (*syscall)(context_frame f);

void trap_exception(void)
{
    cpuinfo ci = current_cpu();
    context ctx = get_current_context(ci);
    context_frame f = ctx->frame;

    if (f[FRAME_FULL])
        halt("\nframe %p already full\n", f);

    f[FRAME_FULL] = true;
    context_reserve_refcount(ctx);

    if (f[FRAME_CAUSE] == TRAP_E_ECALL_UMODE) {
        f[FRAME_PC] += 4;   /* must advance pc, hw does not do it */
        context ctx = ci->m.syscall_context;
        set_current_context(ci, ctx);
        switch_stack_1(frame_get_stack_top(ctx->frame), syscall, f); /* frame is top of stack */
        halt("%s: syscall returned\n", __func__);
    }
    fault_handler fh = ctx->fault_handler;
    if (fh) {
#ifdef INT_DEBUG
        u64 v = SCAUSE_CODE(f[FRAME_CAUSE]);
        if (v < sizeof(interrupt_names)/sizeof(interrupt_names[0])) {
            rputs(" (");
            rputs((char *)interrupt_names[v]);
            rputs(")");
        }
        rputs("\n   context: ");
        print_u64_with_sym(u64_from_pointer(ctx));
        rputs(" (");
        rputs(state_strings[ci->state]);
        rputs(")");
        rputs("\n    status: ");
        print_u64_with_sym(u64_from_pointer(f[FRAME_STATUS]));
        rputs("\n     stval: ");
        print_u64_with_sym(u64_from_pointer(f[FRAME_FAULT_ADDRESS]));
        rputs("\n       epc: ");
        print_u64_with_sym(u64_from_pointer(f[FRAME_PC]));
        rputs("\n");
#endif
        context retctx = apply(fh, ctx);
        if (retctx) {
            context_release_refcount(retctx);
            frame_return(retctx->frame);
        }
        if (is_syscall_context(ctx)) {
            /* This indicates an unhandled fault on a user page from within a
               syscall. We need to abandon the syscall at this point and let
               the thread run so it may receive the appropriate signal. The
               frame is left full so that future context dumps will report the
               actual processor state when the exception occurred. */
            context_release_refcount(ctx);
        }
        assert(!is_kernel_context(ctx));
        runloop();
    } else {
        console("\nno fault handler for frame\n");
        dump_context(ctx);
        vm_exit(VM_EXIT_FAULT);
    }
}

void init_interrupts(kernel_heaps kh)
{
    int_general = heap_locked(kh);
    handlers = allocate_zero(int_general, (PLIC_MAX_INT + 1) * sizeof(handlers[0]));
    assert(handlers != INVALID_ADDRESS);
    for (int i = 0; i <= PLIC_MAX_INT; i++)
        list_init(&handlers[i]);
    init_plic();
    plic_set_c1_threshold(0);
    asm volatile("csrw stvec, %0" :: "r"(&trap_handler));
}

u64 allocate_interrupt(void)
{
    assert(0);
    return 0;
}

void deallocate_interrupt(u64 irq)
{
    assert(0);
}

u64 allocate_mmio_interrupt(void)
{
    // no programmable hw interrupt support
    assert(0);
    return 0;
}

void deallocate_mmio_interrupt(u64 irq)
{
}

u64 allocate_ipi_interrupt(void)
{
    // XXX need to implement software interrupt support
    return 0;
}

void deallocate_ipi_interrupt(u64 irq)
{
}

u64 allocate_msi_interrupt(void)
{
    // no programmable hw interrupt support
    assert(0);
}

void deallocate_msi_interrupt(u64 irq)
{
}

void __attribute__((noreturn)) __stack_chk_fail(void)
{
    cpuinfo ci = current_cpu();
    context ctx = get_current_context(ci);
    rprintf("stack check failed on cpu %d\n", ci->id);
    dump_context(ctx);
    vm_exit(VM_EXIT_FAULT);
}

