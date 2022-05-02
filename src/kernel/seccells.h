
#ifdef UNITTESTING
u64 rtbaseaddr;
#endif

typedef struct cellflags {
    u8 w;                      /* _PAGE_* flags, keep private to page.[hc] */
} cellflags;

//int cell_number_from_va(u64 vaddr);
u32 cell_id_from_vaddr(u64 vaddr);
physical map(u64 v, physical p, u64 length, cellflags flags);
void unmap(u64 virtual, u64 length);

//physical map_with_complete(u64 v, physical p, u64 length, pageflags flags, status_handler complete);


#include <seccells_machine.h>
cellflags * get_flagptr(rt_desc *base, u32 cell_idx, u32 sd_id);