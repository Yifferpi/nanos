
#include <runtime.h>
#ifdef UNITTESTING
#include <stdio.h>
#define print_u64(val, msg) printf("%s: %016lx\n", msg, (long unsigned int) val)
#endif
//#else
//#include <kernel.h>

#include <seccells.h>

void init_permission_table() {
    //allocate space for permission table from 'pagemem'-like struct

    //problem: with paging, any page-sized memory would work out of the box.
    // with seccells, we need some initial data (namely meta cell). Where does it get filled?

    //for unit testing, do this manually in the test file
}
cellflags * get_flagptr(rt_desc *base, u32 cell_idx, u32 sd_id) {
    u32 S = get_S(base);
    u32 T = get_T(base);
    cellflags * perms = (cellflags *) base + 4*(S+T);
    return perms + cell_idx;
}
/* here is another problem: there is not one cellflags, but an array of cellflags,
one per security division! 
Introduce wrapper for case of one sd*/
//physical map_one(u64 v, physical p, u64 length, cellflags flags) {
//    #ifdef UNITTESTING
//    u64 base = rtbaseaddr;
//    #else
//    u64 base = get_permissiontable_base(v);
//    #endif
//    rt_desc * tb = (rt_desc *)base;
//    u32 M = get_M(tb);
//    cellflags flagarray[M];
//    for (int i = 0; i < M; i++) {
//        flagarray[i] = flags;
//    }
//    map(v, p, length, flagarray);
//}

physical map(u64 v, physical p, u64 length, cellflags flags) {
    /* TODO

    */
    #ifdef UNITTESTING
    u64 base = rtbaseaddr;
    #else
    u64 base = get_permissiontable_base(v);
    #endif
    //rt_meta *meta = (rt_meta *) base;
    rt_desc * tb = (rt_desc *) base;
    u32 cell_nr = cell_number_from_va(v);
    if (1 <= cell_nr && cell_nr <= get_N(tb)) {
        /* We already have a mapping */
        /*
        - check if this is a remapping? same virtual but different physical? 
        - throw exception? maybe, function 'remap()' must be used for this case
        */
    } else {
        /* Mapping does not exist yet. Create new description, adapt the mapping */
        printf("made it to the else\n");
        rt_desc newcell;
        set_pbase(&newcell, p);
        set_vbase(&newcell, v);
        set_vbound(&newcell, v+length);
        // DONT forget to adapt the meta cell 
        // insert 
        //changing meta: S needs to be (N/4)+1, have S initially bigger, so we don't need to change it every time
        //if ((N / 4) + 1 > S) {
            //thats when we need to change the layout drastically..
        //}
        u32 N = get_N(tb);
        int i = 1;
        while (i <= N && get_vbound(tb[i]) < v) {
            i++;
        }
        // now at the i where we insert the cell
        rt_desc tmp = tb[i];
        //problem: one cannot just 'malloc'..
        //cellflags * tmp_flags = malloc(get_M(tb)*sizeof(cellflags));
        //for (int sd = 1; sd <= get_M(tb); sd++) {
        //    tmp_flags[sd-1] = *get_flagptr(base, i, sd);
        //}
        while ( i <= N+1) {
            tb[i] = newcell;
            newcell = tmp;
            i++;
            // also change permissions in the same go (for all! SC)
        }
        ((rt_meta *) tb)->N = N+1;
        printf("through the setting loop, i currently at %d\n", i);

        // make sure all cells still fit, move other cells

        /* create new cellflags */
    }
    
    return (u64) 0;
}

u32 cell_number_from_va(u64 vaddr) {
    #ifdef UNITTESTING
    u64 rtbase = rtbaseaddr;
    #else
    u64 rtbase = get_permissiontable_base(vaddr);
    #endif
    //rt_meta *meta = (rt_meta *) rtbase;
    rt_desc *desc_base = ((rt_desc *) rtbase);

    u32 start_idx = 1;
    u32 end_idx = get_N(desc_base) + 1; // +1 for exclusive [start,end) due to metacell
    while (start_idx < end_idx) {
        u32 middle_idx = start_idx + ((end_idx-start_idx) / 2);
        u64 lo = get_vbase(desc_base[middle_idx]);
        u64 up = get_vbound(desc_base[middle_idx]);
        if (vaddr > up) {
            /* Continue searching in upper half */
            start_idx = middle_idx + 1;
        } else if (vaddr < lo) {
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