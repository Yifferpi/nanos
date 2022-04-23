
typedef struct cellflags {
    u8 w;                      /* _PAGE_* flags, keep private to page.[hc] */
} cellflags;

//int cell_number_from_va(u64 vaddr);
u32 cell_number_from_va(u64 rtbase, u64 vaddr);

//physical map_with_complete(u64 v, physical p, u64 length, pageflags flags, status_handler complete);


#include <seccells_machine.h>