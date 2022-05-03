#define PAGEMASK MASK(PAGELOG)

extern u64 tablebase;

static inline u64 get_pagetable_base(u64 vaddr)
{
    return tablebase;
}
static inline u64 get_permissiontable_base(u64 vaddr)
{
    return tablebase;
}

/* Page flags default to minimum permissions:
   - read-only
   - no user access
   - no execute
*/
// this is the same as in Sv48
#define CELL_VALID      U64_FROM_BIT(0)
#define CELL_READABLE   U64_FROM_BIT(1)
#define CELL_WRITABLE   U64_FROM_BIT(2)
#define CELL_EXEC       U64_FROM_BIT(3)
#define CELL_USER       U64_FROM_BIT(4)
#define CELL_GLOBAL     U64_FROM_BIT(5)
#define CELL_DIRTY      U64_FROM_BIT(7)

//#define PAGE_VALID      U64_FROM_BIT(0)
//#define PAGE_READABLE   U64_FROM_BIT(1)
//#define PAGE_WRITABLE   U64_FROM_BIT(2)
//#define PAGE_EXEC       U64_FROM_BIT(3)
//#define PAGE_USER       U64_FROM_BIT(4)
//#define PAGE_GLOBAL     U64_FROM_BIT(5)
//#define PAGE_DIRTY      U64_FROM_BIT(7)
//#define PAGE_NO_BLOCK   U64_FROM_BIT(8) // RSW[0]
//#define PAGE_DEFAULT_PERMISSIONS (PAGE_READABLE)
//#define PAGE_PROT_FLAGS (PAGE_USER | PAGE_READABLE)

/* Seccells definitions */

/* Note: struct definitions are theoretically not necessary but are
done to emphasize that one should interface with the entries
through helper functions.
*/

/*
Description field:
V   | Pbase     | Vbound     | Vbase
127 | (116:72]  | (72:36]   | (36:0]
*/
#define HALF_DESC_SHIFT 64
typedef struct rt_desc {
    u64     upper;
    u64     lower;
} rt_desc;

/*
Meta field:
N       | M         | T
(96:64] | (64:32]   | (32:0]
*/
typedef struct rt_meta {
    u32     N;      // # of Cells (including the metacell!)
    u32     M;      // # of Security Divisions
    u32     S;      // # of cache lines for cells
    u32     T;      // # of permission cache lines per SD
} rt_meta;
/* N is the number of cells, including the meta cell. This means
that there are cell descriptions numbered from 1 to N-1.
In contrast, M is the number of SDs and they are numbered
from 0 to M-1.
*/


/* Range table constants (we call it RT so as not to confuse it with PT pagetable)
these are more or less directly taken from the patched SecureCells qemu 
*/
#define CELL_DESC_SZ  16 // 16 bytes per cell description
#define CELL_PERM_SZ  1 // 1 byte of permission per cell
#define RT_V          0x0000000000000001ull // Valid
#define RT_R          0x0000000000000002ull // Read
#define RT_W          0x0000000000000004ull // Write
#define RT_X          0x0000000000000008ull // Execute
#define RT_G          0x0000000000000020ull // Global
#define RT_A          0x0000000000000040ull // Accessed
#define RT_D          0x0000000000000080ull // Dirty

#define RT_DEFAULT_PERMISSIONS (RT_R)       // default permissions for memory (as opposed to devices)

/* Shifts */
#define RT_VA_START_SHIFT 0            // in 128-bit cell desc
#define RT_VA_END_SHIFT   36           // in 128-bit cell desc
#define RT_PA_SHIFT       72           // in 128-bit cell desc
#define RT_VAL_SHIFT      127          // in 128-bit cell desc
#define RT_META_N_SHIFT   96           // in 128-bit metacell desc
#define RT_META_M_SHIFT   64           // in 128-bit metacell desc
#define RT_META_T_SHIFT   32           // in 128-bit metacell desc
#define RT_PROT_SHIFT     1            // in 8-bit permissions
/* Sizes */
#define RT_VFN_SIZE       36
#define RT_PFN_SIZE       44
#define RT_META_N_SIZE    32
#define RT_META_M_SIZE    32
#define RT_META_T_SIZE    32
/* Masks */
#define RT_VA_MASK        ((1ull << RT_VFN_SIZE) - 1) // 36 bits of VA
#define RT_PA_MASK        ((1ull << RT_PFN_SIZE) - 1) // 44 bits of PA
#define RT_VAL_MASK       1ull // 1 bit for valid marker
#define RT_META_N_MASK    ((1ull << RT_META_N_SIZE) - 1) // 32 bits for number of cells
#define RT_META_M_MASK    ((1ull << RT_META_M_SIZE) - 1) // 32 bits for number of SecDivs
#define RT_META_T_MASK    ((1ull << RT_META_T_SIZE) - 1) // 32 bits for number of permission lines per SecDiv
/* Special */
#define RT_PERMS          (RT_R | RT_W | RT_X)
#define RT_ID_ANY         0xffff
#define RT_ID_SUPERVISOR  0x0000

/* legacy */
//#define PAGE_FLAGS_MASK 0x3ff   //no longer needed as flags don't share pte with address
//
////everything level related no longer needed
//#define PT_FIRST_LEVEL 0
//#define PT_PTE_LEVEL   3
//
//#define PAGE_NLEVELS 4
//
//#define PT_SHIFT_L0 39
//#define PT_SHIFT_L1 30
//#define PT_SHIFT_L2 21
//#define PT_SHIFT_L3 12

#define Sv39 (8ull<<60)
#define Sv48 (9ull<<60)
#define Seccells48 (15ull<<60)  // MMU MODE designated for custom use

//#define IS_LEAF(e) (((e) & (PAGE_EXEC|PAGE_READABLE)) != 0)

/* Description Field manipulation */
static inline u64 get_pbase(rt_desc desc)
{
    return (u64) (desc.upper >> (RT_PA_SHIFT - HALF_DESC_SHIFT)) & RT_PA_MASK;
}
static inline u64 get_vbase(rt_desc desc)
{
    return (u64) (desc.lower >> RT_VA_START_SHIFT) & RT_VA_MASK;
}
static inline u64 get_vbound(rt_desc desc)
{
    u64 lo = (desc.lower >> RT_VA_END_SHIFT);
    u64 up = (desc.upper << (HALF_DESC_SHIFT - RT_VA_END_SHIFT)) & RT_VA_MASK;
    return up | lo;
}
/* Pack the (physical) address into the pbase field of the Cell Description*/
static inline void set_pbase(rt_desc *desc, u64 pbase) {
    u64 addr = pbase << (RT_PA_SHIFT - HALF_DESC_SHIFT);
    u64 mask = RT_PA_MASK << (RT_PA_SHIFT - HALF_DESC_SHIFT);
    desc->upper = addr | (desc->upper & ~mask);
}
/* Pack the (virtual) address into the vbase field of the Cell Description*/
static inline void set_vbase(rt_desc *desc, u64 vbase) {
    desc->lower = (desc->lower & ~RT_VA_MASK) | (vbase & RT_VA_MASK);
}
/* Pack the (virtual) address into the vbound field of the Cell Description*/
static inline void set_vbound(rt_desc *desc, u64 vbound) {
    int lower_bits = HALF_DESC_SHIFT - RT_VA_END_SHIFT;
    int upper_bits = RT_VFN_SIZE - lower_bits - 1;
    u64 upper_mask = (1ull << (upper_bits+1)) - 1;
    u64 lower_mask = (1ull << (lower_bits+1)) - 1;
    //u64 put_in_upper = (vbound >> lower_bits) & upper_mask;
    //u64 put_in_lower = vbound & lower_mask;
    desc->upper = ((desc->upper & ~upper_mask) | ((vbound >> lower_bits) & upper_mask));
    desc->lower = (desc->lower & ~(lower_mask << RT_VA_END_SHIFT)) | ((vbound & lower_mask) << RT_VA_END_SHIFT);
}
/* Meta field manipulation */
static inline u32 get_N(rt_desc *base) {
    return ((rt_meta *) base)->N;
}
static inline u32 get_M(rt_desc *base) {
    return ((rt_meta *) base)->M;
}
static inline u32 get_S(rt_desc *base) {
    return ((rt_meta *) base)->S;
}
static inline u32 get_T(rt_desc *base) {
    return ((rt_meta *) base)->T;
}
static inline void set_N(rt_desc *base, u32 N) {
    ((rt_meta *) base)->N = N;
}
static inline void set_M(rt_desc *base, u32 M) {
    ((rt_meta *) base)->M = M;
}
static inline void set_S(rt_desc *base, u32 S) {
    ((rt_meta *) base)->S = S;
}
static inline void set_T(rt_desc *base, u32 T) {
    ((rt_meta *) base)->T = T;
}

/* next three inlines seem to be permissions used for different types
of memory: 
*/
#define pageflags_memory() cellflags_memory()
#define pageflags_device() cellflags_memory()
#define pageflags_writable(a) cellflags_writable(a)
#define pageflags_readonly(a) cellflags_readonly(a)
#define pageflags_user(a) cellflags_user(a)
#define pageflags_noexec(a) cellflags_noexec(a)
#define pageflags_exec(a) cellflags_exec(a)
#define pageflags_minpage(a) cellflags_minpage(a)
#define pageflags_default_user(a) cellflags_default_user(a)

static inline cellflags cellflags_memory()
{
    return (cellflags){.w = RT_DEFAULT_PERMISSIONS};
}

static inline cellflags cellflags_writable(cellflags flags)
{
    return (cellflags){.w = flags.w | CELL_WRITABLE};
}
// Removes write permission, does not set read permission
static inline cellflags cellflags_readonly(cellflags flags)
{
    return (cellflags){.w = flags.w & ~CELL_WRITABLE};
}

static inline cellflags cellflags_user(cellflags flags)
{
    return (cellflags){.w = flags.w | CELL_USER};
}

static inline cellflags cellflags_noexec(cellflags flags)
{
    return (cellflags){.w = flags.w & ~CELL_EXEC };
}

static inline cellflags cellflags_exec(cellflags flags)
{
    return (cellflags){.w = flags.w | CELL_EXEC};
}
// this would manipulate the RSW bits which we don't have, so we return identity for now
static inline cellflags cellflags_minpage(cellflags flags)
{
    return flags;
}
static inline cellflags cellflags_default_user(void)
{
    return cellflags_user(cellflags_minpage(cellflags_memory()));
}
// unclear: we don't have the RSW bits in Seccells
//static inline cellflags cellflags_minpage(cellflags flags)
//{
//    return (cellflags){.w = flags.w | CELL_NO_BLOCK};
//}

/* bit 8 and 9 in the PTE are reserved for use by the supervisor software
according to the RISCV specification */
// unclear: these might need to be ported once we know about the RSW bits
//static inline pageflags pageflags_no_minpage(pageflags flags)
//{
//    return (pageflags){.w = flags.w & ~PAGE_NO_BLOCK};
//}
//
///* no-exec, read-only */
//static inline pageflags pageflags_default_user(void)
//{
//    return pageflags_user(pageflags_minpage(pageflags_memory()));
//}

/* the following set of functions are checker functions */
//static inline boolean pageflags_is_writable(pageflags flags)
//{
//    return (flags.w & PAGE_WRITABLE) != 0;
//}
//
//static inline boolean pageflags_is_readonly(pageflags flags)
//{
//    return !pageflags_is_writable(flags);
//}
//
//static inline boolean pageflags_is_noexec(pageflags flags)
//{
//    return (flags.w & PAGE_EXEC) == 0;
//}
//
//static inline boolean pageflags_is_exec(pageflags flags)
//{
//    return !pageflags_is_noexec(flags);
//}
// check functions
#define pageflags_is_writable(a) cellflags_is_writable(a)
#define pageflags_is_readonly(a) cellflags_is_readonly(a)
#define pageflags_is_noexec(a) cellflags_is_noexec(a)
#define pageflags_is_exec(a) cellflags_is_exec(a)

static inline boolean cellflags_is_writable(cellflags flags)
{
    return (flags.w & CELL_WRITABLE) != 0;
}

static inline boolean cellflags_is_readonly(cellflags flags)
{
    return !cellflags_is_writable(flags);
}

static inline boolean cellflags_is_noexec(cellflags flags)
{
    return (flags.w & CELL_EXEC) == 0;
}
static inline boolean cellflags_is_exec(cellflags flags)
{
    return !cellflags_is_noexec(flags);
}

// These typedefs are used by the pagecache. 
typedef u64 pte;
typedef volatile pte *pteptr;
//
static inline pte pte_from_pteptr(pteptr pp)
{
    return *pp;
}
// This might be a problem if pp points to a cellflags (u8)
static inline void pte_set(pteptr pp, pte p)
{
    *pp = p;
}
//
static inline boolean pte_is_present(pte entry)
{
    return (entry & RT_V) != 0;
}

//static inline boolean pte_is_block_mapping(pte entry)
//{
//    return (entry & PAGE_NO_BLOCK) != 0;
//}
//
//static inline int pt_level_shift(int level)
//{
//    switch (level) {
//    case 0:
//        return PT_SHIFT_L0;
//    case 1:
//        return PT_SHIFT_L1;
//    case 2:
//        return PT_SHIFT_L2;
//    case 3:
//        return PT_SHIFT_L3;
//    default:
//        assert(0);
//    }
//    return 0;
//}
//
///* log of mapping size (block or page) if valid leaf, else 0 */
//static inline int pte_order(int level, pte entry)
//{
//    assert(level < PAGE_NLEVELS);
//    if (level == 0 || !pte_is_present(entry) ||
//        !IS_LEAF(entry))
//        return 0;
//    return pt_level_shift(level);
//}
// THIS IS A PROBLEM! USING ptes!! call from mmap()
static inline u64 pte_map_size(int level, pte entry)
{
    //int order = pte_order(level, entry);
    //return order ? U64_FROM_BIT(order) : INVALID_PHYSICAL;
    return 0;
}
//
///* they distinguish between mapping, which is a translation entry and
//non-mapping which is an entry pointing to another pagetable */
//static inline boolean pte_is_mapping(int level, pte entry)
//{
//    // XXX is leaf?
//    return pte_is_present(entry) && IS_LEAF(entry);
//}
//
//static inline u64 flags_from_pte(u64 pte)
//{
//    return pte & PAGE_FLAGS_MASK;
//}
//
//static inline pageflags pageflags_from_pte(pte pte)
//{
//    return (pageflags){.w = flags_from_pte(pte)};
//}
//
//static inline u64 page_pte(u64 phys, u64 flags)
//{
//    // XXX?
//    return (phys>>2) | flags | PAGE_VALID;
//}
//
//static inline u64 block_pte(u64 phys, u64 flags)
//{
//    // XXX?
//    return (phys>>2) | flags | PAGE_VALID;
//}
//
//static inline u64 new_level_pte(u64 tp_phys)
//{
//    return (tp_phys>>2) | PAGE_VALID;
//}
//
//static inline boolean flags_has_minpage(u64 flags)
//{
//    return (flags & PAGE_NO_BLOCK) != 0;
//}
//
static inline boolean pte_is_dirty(pte entry)
{
    return (entry & RT_D) != 0;
}
//
static inline void pt_pte_clean(pteptr pte)
{
    *pte &= ~RT_D;
}
// THIS WILL NOT WORK! here, the part of the pte is extracted that does
//not exist as such anymore...
//static inline u64 page_from_pte(pte pte)
//{
//    return (pte & (MASK(54) & ~PAGE_FLAGS_MASK))<<2;
//}
//
//u64 *pointer_from_pteaddr(u64 pa);
//
//static inline u64 pte_lookup_phys(u64 table, u64 vaddr, int offset)
//{
//    return table + (((vaddr >> offset) & MASK(9)) << 3);
//}
//
//static inline u64 *pte_lookup_ptr(u64 table, u64 vaddr, int offset)
//{
//    return pointer_from_pteaddr(pte_lookup_phys(table, vaddr, offset));
//}
//
//#define _pfv_level(table, vaddr, level)                                  
//    u64 *l ## level = pte_lookup_ptr(table, vaddr, PT_SHIFT_L ## level); 
//    if (!(*l ## level & 1))                                              
//        return INVALID_PHYSICAL;
//
//#define _pfv_check_leaf(level, vaddr, e)                                    
//    if (IS_LEAF(*l ## level)) {                                          
//        if (e) *e = *l ## level;                                                
//        return page_from_pte(*l ## level) | (vaddr & MASK(PT_SHIFT_L ## level)); 
//    }
//
//static inline physical physical_and_pte_from_virtual(u64 xt, pte *e)
//{
//    _pfv_level(tablebase, xt, 0);
//    _pfv_check_leaf(0, xt, e);
//    _pfv_level(page_from_pte(*l0), xt, 1);
//    _pfv_check_leaf(1, xt, e);
//    _pfv_level(page_from_pte(*l1), xt, 2);
//    _pfv_check_leaf(2, xt, e);
//    _pfv_level(page_from_pte(*l2), xt, 3);
//    _pfv_check_leaf(3, xt, e);
//    assert(0); // should not get here
//}
//
//static inline physical __physical_from_virtual_locked(void *x)
//{
//    return physical_and_pte_from_virtual(u64_from_pointer(x), 0);
//}
//
//physical physical_from_virtual(void *x);
//
//#define table_from_pte page_from_pte

void init_mmu(range init_pt, u64 vtarget);

