
#ifdef UNITTESTING
#include <runtime.h>
#include <seccells.h>
#include <stdio.h>
#define print_u64(val, msg) printf("%s: %016lx\n", msg, (long unsigned int) val)
#else
#include <kernel.h>
#endif

#define CURRENT_SD 0

// for debugging
#ifdef PAGE_INIT_DEBUG
#define page_init_debug(x) early_debug(x)
#define page_init_debug_u64(x) early_debug_u64(x)
#else
#define page_init_debug(x)
#define page_init_debug_u64(x)
#endif

#define PAGEMEM_ALLOC_SIZE PAGESIZE_2M

#ifdef KERNEL
static struct spinlock pt_lock;
#define pagetable_lock() u64 _savedflags = spin_lock_irq(&pt_lock)
#define pagetable_unlock() spin_unlock_irq(&pt_lock, _savedflags)
#else
#define pagetable_lock()
#define pagetable_unlock()
#endif

/* This function, given a pointer and a length, is supposed to populate
the initial datastructure. M=1, and depending on the size different R and T */
void init_permission_table(heap pageheap) {
    // set M=1, therefore there is only one SD and we can make a lot of space
    //for cells
    //allocate space for permission table from 'pagemem'-like struct

    //problem: with paging, any page-sized memory would work out of the box.
    // with seccells, we need some initial data (namely meta cell). Where does it get filled?

    //for unit testing, do this manually in the test file
}

/* TODO: implement init_mmu()
To that end, the following three functions need to be implemented here:
- page_set_allowed_levels()
- init_page_initial_map()
- allocate_table_page()
Maybe, since they are only called form init_mmu(), we can also
simplify and combine into less functions.
*/

/* This struct is mostly unused as we will allocate memory for the permission
table at the very beginning and don't extend it after that. This is due to
the physical allocator probably being unable to guarantee that additional memory
would be contiguous. */

static struct {
    range current_phys;
    heap pageheap;
    void *initial_map;
    u64 initial_physbase;
    u64 levelmask;              /* bitmap of levels allowed to map */
} pagemem;

//  this includes `page_set_allowed_levels()`
void page_set_allowed_levels(u64 levelmask)
{
    pagemem.levelmask = levelmask;
}
void init_cell_initial_map(void *initial_map, range phys, u64 levelmask)
{
    page_init_debug("init_page_initial_map: initial_map ");
    page_init_debug_u64(u64_from_pointer(initial_map));
    page_init_debug(", phys ");
    page_init_debug_u64(phys.start);
    page_init_debug(", length ");
    page_init_debug_u64(range_span(phys));
    page_init_debug("\n");

    spin_lock_init(&pt_lock);
    //pagemem.levelmask = levelmask;
    pagemem.current_phys = phys;
    pagemem.pageheap = 0;
    pagemem.initial_map = initial_map;
    pagemem.initial_physbase = phys.start;
}

BSS_RO_AFTER_INIT boolean bootstrapping;
/* replaces allocate_table_page(), in contrast to before, we call it only
at the beginning from init_mmu() and never after. The base of the allocated
memory is to be stored into the location 'phys'. */
void * allocate_permission_table(u64 * phys) {
    if (bootstrapping) {
        /* Bootloader use: single, identity-mapped pages */
        page_init_debug("Bootstrapping permission table\n");
        void *p = allocate_zero(pagemem.pageheap, PAGESIZE);
        *phys = u64_from_pointer(p);
        return p;
    }
    page_init_debug("allocate_table_page:");
    if (range_span(pagemem.current_phys) == 0) {
        assert(pagemem.pageheap);
        page_init_debug(" [new alloc, va: ");
        u64 va = allocate_u64(pagemem.pageheap, PAGEMEM_ALLOC_SIZE); // eventually change this to allocate more than one pagesize!!
        if (va == INVALID_PHYSICAL) {
            msg_err("failed to allocate page table memory\n");
            return INVALID_ADDRESS;
        }
        page_init_debug_u64(va);
        page_init_debug("] ");
        assert(is_linear_backed_address(va));
        pagemem.current_phys = irangel(phys_from_linear_backed_virt(va), PAGEMEM_ALLOC_SIZE);
    }

    *phys = pagemem.current_phys.start;
    pagemem.current_phys.start += PAGESIZE;
    void *p = (void *)*phys;
    page_init_debug(" phys: ");
    page_init_debug_u64(*phys);
    page_init_debug("\n");
    zero(p, PAGESIZE);
    // Initialize datastructure:
    set_N((rt_desc *) p, 1);
    set_M((rt_desc *) p, 1);
    set_R((rt_desc *) p, 1);
    set_T((rt_desc *) p, 12); // T needs to be set depending on the size of the permission table allocated here!

    return p;
    //return 0;
}
// this function updates flags but additionally flushes tlb stuff
void update_map_flags_with_complete(u64 vaddr, u64 length, pageflags flags, status_handler complete)
{
    //flags = pageflags_no_minpage(flags);
    //page_debug("%s: vaddr 0x%lx, length 0x%lx, flags 0x%lx\n", __func__, vaddr, length, flags.w);

    ///* Catch any attempt to change page flags in a linear_backed mapping */
    //assert(!intersects_linear_backed(irangel(vaddr, length)));
    //flush_entry fe = get_page_flush_entry();
    //traverse_ptes(vaddr, length, stack_closure(update_pte_flags, flags, fe));
    //page_invalidate_sync(fe, complete);
}
void update_flags() {

}



cellflags * get_flagptr(rt_desc *base, u32 cell_idx, u32 sd_id) {
    u32 S = get_S(base); // the max number of cachelines reserved for descriptions
    u32 T = get_T(base); // the number of cache lines for permissions per SD
    cellflags *perms = ((cellflags *) base) + S * 64 + sd_id * T * 64 + cell_idx;
    //cellflags * perms = (cellflags *) (base + S/4 + sd_id*T); 
    // above: S/4 because we are stepping in size_of(rt_desc), not cachelines
    //return perms + cell_idx;
    return perms;
}

// in a full-fletched implementation, `flags` would need to be an array of size M
physical map(u64 v, physical p, u64 length, cellflags flags) {
    #ifdef UNITTESTING
    u64 base = rtbaseaddr;
    #else
    u64 base = get_permissiontable_base(v);
    #endif
    
    rt_desc * tb = (rt_desc *) base;
    u32 cell_id = cell_id_from_vaddr(v);
    page_init_debug("map, called from ");
    page_init_debug_u64(u64_from_pointer(__builtin_return_address(0)));
    page_init_debug("\n");
    /*
    We need to check 4 cases:
    - Mapping not yet existent:     simply create the mapping
    - Same mapping existent:        adjust permissions?
    - Same base, different length:  adjust bounds?
    - Overlapping mapping:          fatal error
    The difference can also be in virtual and/or physical
    */
    if (1 <= cell_id && cell_id <= get_N(tb)) {
        /* We already have a mapping */
        return (u64) 0;
    }

    /* Mapping does not exist yet. Create new description, adapt the mapping */
    u32 N = get_N(tb);
    int i = 1;
    while (i < N && get_vbound(tb[i]) <= v) {
        i++;
    }
    page_init_debug("New map:\n");
    page_init_debug("physical:   ");
    page_init_debug_u64(p);
    page_init_debug("\n");
    page_init_debug("virt base:  ");
    page_init_debug_u64(v);
    page_init_debug("\n");
    page_init_debug("virt bound: ");
    page_init_debug_u64(v+length);
    page_init_debug("\n");
    rt_desc new_desc;
    set_pbase(&new_desc, (p >> 12));
    set_vbase(&new_desc, (v >> 12));
    set_vbound(&new_desc, ((v+length) >> 12));
    set_valid(&new_desc);
    cellflags new_flags = cellflags_valid(flags);
    page_init_debug("Cellflags: ");
    page_init_debug_u64((u64)new_flags.w);
    page_init_debug("\n");
        
    // loop fencepost
    rt_desc old_desc = tb[i];
    cellflags old_flags = *get_flagptr(tb, i, CURRENT_SD);
    while ( i < N+1) {
        tb[i] = new_desc;
        *get_flagptr(tb, i, CURRENT_SD) = new_flags;

        new_desc = old_desc;
        new_flags = old_flags;

        i++;
        old_desc = tb[i];
        old_flags = *get_flagptr(tb, i, CURRENT_SD);
    }
    ((rt_meta *) tb)->N = N+1;
    
    
    return (u64) p;
}

void dump_page_tables(u64 vaddr, u64 length) {
    return;
}
void dump_permission_table() {

    u64 base = get_permissiontable_base(0);
    rt_desc * tb = (rt_desc *) base;
    page_init_debug("\nMetacell\nN: ");
    page_init_debug_u64((u64) get_N(tb));
    page_init_debug("\nM:");
    page_init_debug_u64((u64) get_M(tb));
    page_init_debug("\nR:");
    page_init_debug_u64((u64) get_R(tb));
    page_init_debug("\nT:");
    page_init_debug_u64((u64) get_T(tb));
    page_init_debug("\n");
    for (int i = 1; i < get_N(tb); i++) {
        page_init_debug("Cell ");
        page_init_debug_u64((u64) i);
        page_init_debug(": ");
        //u64 *cell = (u64 *)(tb + i);
        page_init_debug("\npbase: ");
        page_init_debug_u64(get_pbase(tb[i]));
        page_init_debug("\nvbase: ");
        page_init_debug_u64(get_vbase(tb[i]));
        page_init_debug("\nvbound: ");
        page_init_debug_u64(get_vbound(tb[i]));
        page_init_debug("\n");
        //page_init_debug_u64(*cell);
        //page_init_debug_u64(*(cell+1));
        //page_init_debug("\n");
    }
    page_init_debug("Dump finished\n") ;
}

void remap_pages(u64 vaddr_new, u64 vaddr_old, u64 length) {
    return;    
}

#ifndef UNITTESTING
u64 physical_from_virtual(void *x) {
    return (physical) x;  
}
#endif

/* validate that all pages in vaddr range [base, base + length) are present */
boolean validate_virtual(void * base, u64 length)
{
    return true;
}
boolean validate_virtual_writable(void * base, u64 length)
{
    return true;
}

void unmap(u64 virtual, u64 length) {
    #ifdef UNITTESTING
    u64 base = rtbaseaddr;
    #else
    u64 base = get_permissiontable_base(virtual);
    #endif
    rt_desc * tb = (rt_desc *) base;

    u32 cell_id = cell_id_from_vaddr(virtual);
    // fail if not found
    u32 N = get_N(tb);
    while (cell_id < N-1) {
        tb[cell_id] = tb[cell_id+1];
        *get_flagptr(tb, cell_id, CURRENT_SD) = *get_flagptr(tb, cell_id+1, CURRENT_SD);
        cell_id++;
    }
    ((rt_meta *) tb)->N = N-1;
}

/* Find Cell ID for the provided virtual address. On success, an ID between
1 and N-1 is returned. Otherwise, if the address is not mapped, 0 is returned. */
u32 cell_id_from_vaddr(u64 vaddr) {
    #ifdef UNITTESTING
    u64 rtbase = rtbaseaddr;
    #else
    u64 rtbase = get_permissiontable_base(vaddr);
    #endif
    page_init_debug("getting table base:\n");
    page_init_debug_u64(rtbase);
    page_init_debug("\n");
    //rt_meta *meta = (rt_meta *) rtbase;
    rt_desc *desc_base = ((rt_desc *) rtbase);
    
    //if (get_N(desc_base) == 1) { //empty table, no mapping
    //    page_init_debug("empty table, returning zero\n");
    //    return 0;
    //}

    u32 start_idx = 1;
    u32 end_idx = get_N(desc_base) + 1; // +1 for exclusive [start,end) due to metacell
    while (start_idx < end_idx) {
        u32 middle_idx = start_idx + ((end_idx-start_idx) / 2);
        u64 lo = get_vbase(desc_base[middle_idx]);
        u64 up = get_vbound(desc_base[middle_idx]);
        u64 addr = (vaddr >> 12);
        if (addr > up) {
            /* Continue searching in upper half */
            start_idx = middle_idx + 1;
        } else if (addr < lo) {
            /* Continue searching in lower half */
            end_idx = middle_idx;
        } else {
            /* find right cell description */
            return middle_idx;
        }
    }
    //address is not mapped
    return 0;
}