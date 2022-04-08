typedef struct spinlock {
    word w;
} *spinlock;

typedef struct rw_spinlock {
    struct spinlock l;
    word readers;
} *rw_spinlock;

#if defined(KERNEL) && defined(SMP_ENABLE)
static inline boolean spin_try(spinlock l)
{
    return compare_and_swap_64(&l->w, 0, 1);
}

static inline void spin_lock(spinlock l)
{
    while (l->w || !compare_and_swap_64(&l->w, 0, 1))
        kern_pause();
}

static inline void spin_unlock(spinlock l)
{
    compiler_barrier();
    *(volatile u64 *)&l->w = 0;
}

static inline void spin_rlock(rw_spinlock l)
{
    while (1) {
        if (l->l.w) {
            kern_pause();
            continue;
        }
        fetch_and_add(&l->readers, 1);
        if (!l->l.w)
            return;
        fetch_and_add(&l->readers, -1);
    }
}

static inline void spin_runlock(rw_spinlock l)
{
    fetch_and_add(&l->readers, -1);
}

static inline void spin_wlock(rw_spinlock l)
{
    spin_lock(&l->l);
    while (l->readers)
        kern_pause();
}

static inline void spin_wunlock(rw_spinlock l)
{
    spin_unlock(&l->l);
}
#else
#ifdef SPIN_LOCK_DEBUG_NOSMP
u64 get_program_counter(void);

static inline boolean spin_try(spinlock l)
{
    if (l->w)
        return false;
    l->w = get_program_counter();
    return true;
}

static inline void spin_lock(spinlock l)
{
    if (l->w != 0) {
        print_frame_trace_from_here();
        halt("spin_lock: lock %p already locked by 0x%lx\n", l, l->w);
    }
    l->w = get_program_counter();
}

static inline void spin_unlock(spinlock l)
{
    assert(l->w != 1);
    l->w = 0;
}

static inline void spin_rlock(rw_spinlock l) {
    assert(l->l.w == 0);
    assert(l->readers == 0);
    l->readers++;
}

static inline void spin_runlock(rw_spinlock l) {
    assert(l->readers == 1);
    assert(l->l.w == 0);
    l->readers--;
}

static inline void spin_wlock(rw_spinlock l) {
    assert(l->readers == 0);
    spin_lock(&l->l);
}

static inline void spin_wunlock(rw_spinlock l) {
    assert(l->readers == 0);
    spin_unlock(&l->l);
}
#else
#define spin_try(x) (true)
#define spin_lock(x) ((void)x)
#define spin_unlock(x) ((void)x)
#define spin_wlock(x) ((void)x)
#define spin_wunlock(x) ((void)x)
#define spin_rlock(x) ((void)x)
#define spin_runlock(x) ((void)x)
#endif
#endif

static inline void spin_lock_init(spinlock l)
{
    l->w = 0;
}

static inline void spin_rw_lock_init(rw_spinlock l)
{
    spin_lock_init(&l->l);
    l->readers = 0;
}
