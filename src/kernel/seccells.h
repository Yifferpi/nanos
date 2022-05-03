
#ifdef UNITTESTING
u64 rtbaseaddr;
#endif

#define pageflags cellflags
typedef struct cellflags {
    u8 w;                      /* _PAGE_* flags, keep private to page.[hc] */
} cellflags;

#define init_page_tables(a) init_permission_table(a)
void init_permission_table(heap pageheap);

// tlb shootdown stuff
void init_flush(heap);
flush_entry get_page_flush_entry();
void page_invalidate(flush_entry f, u64 address);
void page_invalidate_sync(flush_entry f, status_handler completion);
void page_invalidate_flush();


// this is publically used
//u64 physical_from_virtual(void *x);

// these are used by init_mmu()
void init_cell_initial_map(void *initial_map, range phys, u64 levelmask);
void * allocate_permission_table(u64 * phys);

//int cell_number_from_va(u64 vaddr);
u32 cell_id_from_vaddr(u64 vaddr);

physical map(u64 v, physical p, u64 length, cellflags flags);
static inline physical map_with_complete(u64 v, physical p, u64 length, cellflags flags, status_handler complete)
{
    return map(v, p, length, flags);
}
// This is for updating flags
void update_map_flags_with_complete(u64 vaddr, u64 length, pageflags flags, status_handler complete);
static inline void update_map_flags(u64 vaddr, u64 length, pageflags flags)
{
    update_map_flags_with_complete(vaddr, length, flags, 0);
}

void unmap(u64 virtual, u64 length);
static inline void unmap_pages_with_handler(u64 virtual, u64 length, range_handler rh)
{
    unmap(virtual, length);
}

//physical map_with_complete(u64 v, physical p, u64 length, pageflags flags, status_handler complete);


#include <seccells_machine.h>
cellflags * get_flagptr(rt_desc *base, u32 cell_idx, u32 sd_id);

//void init_page_initial_map(void *initial_map, range phys);
//void init_page_tables(heap pageheap);
//
///* tlb shootdown */
//void init_flush(heap);
//flush_entry get_page_flush_entry();
//void page_invalidate(flush_entry f, u64 address);
//void page_invalidate_sync(flush_entry f, status_handler completion);
//void page_invalidate_flush();
//
///* mapping and flag update */
//physical map_with_complete(u64 v, physical p, u64 length, pageflags flags, status_handler complete);
//
//static inline void map(u64 v, physical p, u64 length, pageflags flags)
//{
//    map_with_complete(v, p, length, flags, 0);
//}
//
//void update_map_flags_with_complete(u64 vaddr, u64 length, pageflags flags, status_handler complete);
//
//static inline void update_map_flags(u64 vaddr, u64 length, pageflags flags)
//{
//    update_map_flags_with_complete(vaddr, length, flags, 0);
//}
//
//void zero_mapped_pages(u64 vaddr, u64 length);
//void remap_pages(u64 vaddr_new, u64 vaddr_old, u64 length);
//void unmap(u64 virtual, u64 length);
//void unmap_pages_with_handler(u64 virtual, u64 length, range_handler rh);
//
//static inline void unmap_pages(u64 virtual, u64 length)
//{
//    unmap_pages_with_handler(virtual, length, 0);
//}
//
//#include <page_machine.h>
//
///* table traversal */
//typedef closure_type(entry_handler, boolean /* success */, int /* level */,
//                     u64 /* vaddr */, pteptr /* entry */);
//boolean traverse_ptes(u64 vaddr, u64 length, entry_handler eh);
//void dump_page_tables(u64 vaddr, u64 length);
//
///* internal use */
//void *allocate_table_page(u64 *phys);
//void page_set_allowed_levels(u64 levelmask);
//
///* if using bootstrapped page tables */
//extern boolean bootstrapping;
