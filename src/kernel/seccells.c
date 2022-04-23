
#include <runtime.h>
#ifdef UNITTESTING
//#include <stdio.h>
//#define print_u64(val, msg) printf("%s: %016lx\n", msg, (long unsigned int) val)
#endif
//#else
//#include <kernel.h>

#include <seccells.h>
//physical map(u64 v, physical p, u64 length, pageflags flags) {
//    /* 
//    
//    */
//    return (u64) 0;
//}
//int cell_number_from_va(u64 vaddr) {
//    u64 base = get_permissiontable_base(vaddr);
//    return _cell_number_from_va(base, vaddr);
//}
u32 cell_number_from_va(u64 rtbase, u64 vaddr) {
    rt_meta *meta = (rt_meta *) rtbase;
    //u32 N = get_N(meta);
    rt_desc *desc_base = ((rt_desc *) rtbase);

    u32 start_idx = 1;
    u32 end_idx = get_N(meta) + 1; // +1 for exclusive [start,end) due to metacell
    while (start_idx < end_idx) {
        //printf("Start: %d, End: %d\n", start_idx, end_idx);
        u32 middle_idx = start_idx + ((end_idx-start_idx) / 2);
        u64 lo = get_vbase(desc_base[middle_idx]);
        u64 up = get_vbound(desc_base[middle_idx]);
        //printf("Target: %016llx\n", vaddr);
        //printf("Lower : %016llx\n", lo);
        //printf("Upper : %016llx\n", up);
        if (vaddr > up) {
            /* Continue searching in upper half */
            //printf("moving right\n");
            start_idx = middle_idx + 1;
        } else if (vaddr < lo) {
            /* Continue searching in lower half */
            //printf("moving left\n");
            end_idx = middle_idx;
        } else {
            /* find right cell description */
            //printf("returning %d\n", middle_idx);
            return middle_idx;
        }
    }
    //address is not mapped
    return 0;
}