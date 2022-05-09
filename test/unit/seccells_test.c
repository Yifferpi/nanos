
#include <runtime.h>
#include <stdlib.h>
#include <check.h>
#include <stdio.h>
#include <seccells.h>

#define print_u64(val, msg) printf("%s: %016lx\n", msg, (long unsigned int) val)
#define print_u8(val, msg) printf("%s: %02x\n", msg, (unsigned char) val)
#define print_desc(d, msg) printf("%s: %016lx%016lx\n", msg, (long unsigned int) d.upper, (long unsigned int) d.lower)

START_TEST (test_getDescFields) {
    rt_desc desc = {
        .upper = 0xdddcccccccccccaa,
        .lower = 0xaaaaaaabbbbbbbbb,
    };
    u64 ans = get_pbase(desc);
    ck_assert(0xccccccccccc == ans);
    ans = get_vbase(desc);
    ck_assert(0xbbbbbbbbb == ans);
    ans = get_vbound(desc);
    ck_assert(0xaaaaaaaaa == ans);

    //MISSING: valid bit
    
}
END_TEST

START_TEST (test_setDescFields) {
    rt_desc desc = {.upper = 0,.lower = 0,};
    
    set_pbase(&desc, 0xdeadbeef);
    ck_assert(desc.upper == 0x000000deadbeef00);

    set_vbase(&desc, 0xcafecafe);
    ck_assert(desc.lower == 0x00000000cafecafe);

    set_vbound(&desc, 0xaaaabbbb);
    ck_assert(desc.upper == 0x000000deadbeef0a);
    ck_assert(desc.lower == 0xaaabbbb0cafecafe);
}
END_TEST

/* RSW | A | G | U | X | W | R | V */
START_TEST (test_cellflags) {
    cellflags tmp = {.w = 0x00};
    tmp = cellflags_writable(tmp);
    ck_assert(tmp.w == 0x04); // -W-
    tmp = cellflags_exec(tmp);
    ck_assert(tmp.w == 0x0c); // XW-
    tmp = cellflags_writable(tmp);  //omnipotency?
    tmp = cellflags_exec(tmp);
    ck_assert(tmp.w == 0x0c); // XW-
    tmp = cellflags_readonly(tmp);
    ck_assert(tmp.w == 0x08); // X--
    tmp = cellflags_noexec(tmp);
    ck_assert(tmp.w == 0x00);

}
END_TEST

START_TEST (test_check_cellflags) {
    cellflags tmp = {.w = 0x00}; // ---
    ck_assert(cellflags_is_noexec(tmp));
    ck_assert(!cellflags_is_exec(tmp));
    ck_assert(cellflags_is_readonly(tmp));
    ck_assert(!cellflags_is_writable(tmp));
    tmp.w = 0x0c; // XW-
    ck_assert(!cellflags_is_noexec(tmp));
    ck_assert(cellflags_is_exec(tmp));
    ck_assert(!cellflags_is_readonly(tmp));
    ck_assert(cellflags_is_writable(tmp));
}
END_TEST

START_TEST (test_metafield) {
    rt_desc *desc = malloc(16 * sizeof(rt_desc));
    set_N(desc, 4);
    set_M(desc, 1);
    set_R(desc, 1);
    set_T(desc, 2);
    ck_assert_int_eq(get_N(desc), 4);
    ck_assert_int_eq(get_M(desc), 1);
    ck_assert_int_eq(get_R(desc), 1);
    ck_assert_int_eq(get_T(desc), 2);
    ck_assert_int_eq(get_S(desc), 8);
    ck_assert_int_eq(get_Q(desc), 2);
    free(desc);
}
END_TEST

START_TEST (test_cellnumber) {
    rt_desc *desc = malloc(16 * sizeof(rt_desc));
    set_N(desc, 4);
    set_M(desc, 1);
    set_T(desc, 2);
    set_R(desc, 1);

    rt_desc tmp; 
    set_vbase(&tmp,  0x008000);
    set_vbound(&tmp, 0x00f000);
    desc[1] = tmp;
    set_vbase(&tmp,  0x00f000);
    set_vbound(&tmp, 0x010000);
    desc[2] = tmp;
    set_vbase(&tmp,  0x010000);
    set_vbound(&tmp, 0x0f0000);
    desc[3] = tmp;

    rtbaseaddr = (u64) desc; //this is the replacement for the legitimate tablebase
    ck_assert(cell_id_from_vaddr(0x008000) == 1);
    ck_assert(cell_id_from_vaddr(0x00a000) == 1);
    ck_assert(cell_id_from_vaddr(0x00f000) == 2);
    ck_assert(cell_id_from_vaddr(0x00ffff) == 2);
    ck_assert(cell_id_from_vaddr(0x010000) == 3);
    ck_assert(cell_id_from_vaddr(0x01ffff) == 3);
    ck_assert(cell_id_from_vaddr(0x0effff) == 3);
    ck_assert(cell_id_from_vaddr(0x007000) == 0); // unmapped addresses
    ck_assert(cell_id_from_vaddr(0x0fffff) == 0);
    free(desc);
}
END_TEST

START_TEST (test_map_description) {
    rt_desc * desc = malloc(16 * sizeof(rt_desc));
    rtbaseaddr = (u64) desc; //this is the replacement for the legitimate tablebase
    set_N(desc, 1);
    set_M(desc, 1);
    set_T(desc, 2);
    set_R(desc, 1);

    map(0x1000, 0x8000, 0x0080, cellflags_writable((cellflags){.w=0x00}));
    map(0x3000, 0x8000, 0x0080, cellflags_writable((cellflags){.w=0x00}));
    ck_assert(get_vbase(desc[1])  == 0x1000);
    ck_assert(get_vbound(desc[1]) == 0x1080);
    ck_assert(get_vbase(desc[2])  == 0x3000);
    ck_assert(get_vbound(desc[2]) == 0x3080);
    map(0x2000, 0x8000, 0x0080, cellflags_writable((cellflags){.w=0x00}));
    ck_assert(get_vbase(desc[1])  == 0x1000);
    ck_assert(get_vbound(desc[1]) == 0x1080);
    ck_assert(get_vbase(desc[2])  == 0x2000);
    ck_assert(get_vbound(desc[2]) == 0x2080);
    ck_assert(get_vbase(desc[3])  == 0x3000);
    ck_assert(get_vbound(desc[3]) == 0x3080);
    free(desc);
}
END_TEST

START_TEST (test_map_sorted) {
    /* This test ensures that inserted cell descriptions remain sorted
    by virtual address */
    rt_desc * desc = malloc(16 * sizeof(rt_desc));
    rtbaseaddr = (u64) desc; //this is the replacement for the legitimate tablebase
    set_N(desc, 1);
    set_M(desc, 1);
    set_T(desc, 2);
    set_R(desc, 1);
    
    cellflags flags = {.w = 0x00};
    map(0x2000, 0x0, 4096, flags);
    map(0x3000, 0x0, 8192, flags);
    map(0x1000, 0x0, 4096, flags);
    map(0x5000, 0x0, 4096, flags);
    map(0x8000, 0x0, 1000, flags);
    for (int i = 1; i < get_N(desc) - 1; i++) {
        ck_assert(get_vbound(desc[i]) <= get_vbase(desc[i+1]));
    }
    free(desc);
}
END_TEST

START_TEST (test_map_permissions) {
    /* This test checks whether permissions are set correctly on mapping
    and whether the permissions are moved along with the cell descriptions */
    rt_desc * desc = malloc(16 * sizeof(rt_desc));
    rtbaseaddr = (u64) desc; //this is the replacement for the legitimate tablebase
    set_N(desc, 1);
    set_M(desc, 1);
    set_T(desc, 2);
    set_R(desc, 1);

    cellflags * permissionptr = (cellflags *)(desc + 8); //pointer to where the permissions start
    for (int i = 0; i < 16; i++) {
        print_desc(desc[i],"");
    }
    //map(0x3000, 0x8000, 4096, cellflags_writable((cellflags){.w=0x00}));
    map(0x3000, 0x8000, 4096, (cellflags){.w=0xff});
    printf("after\n");
    for (int i = 0; i < 16; i++) {
        print_desc(desc[i],"");
    }
    ck_assert(cellflags_is_writable(permissionptr[1]));
    map(0x5000, 0xa000, 4096, (cellflags){.w = 0xab});
    ck_assert(permissionptr[2].w == 0xab);
    map(0x1000, 0x6000, 4096, cellflags_exec((cellflags){.w=0x00}));
    ck_assert(cellflags_is_exec(permissionptr[1]));
    map(0x8000, 0x0, 4096, (cellflags){.w = 0xcd});
    ck_assert(cellflags_is_exec(permissionptr[4]));
    free(desc);
}
END_TEST

START_TEST (test_unmap) {
    rt_desc * desc = malloc(16 * sizeof(rt_desc));
    rtbaseaddr = (u64) desc; //this is the replacement for the legitimate tablebase
    set_N(desc, 1);
    set_M(desc, 1);
    set_T(desc, 2);
    set_R(desc, 1);

    cellflags nullflags = { .w = 0x00};
    map(0x3000, 0x8000, 4096, cellflags_writable(nullflags));
    unmap(0x3000, 4096);
    ck_assert(get_N((rt_desc *) desc) == 1);

    for (int i = 0; i < 12; i++) {
        rt_desc * d = (rt_desc *) desc;
        print_desc(d[i], "");
    }
    free(desc);
}
END_TEST

/* Testing TODO
- map already existing address, overlapping address, check expected error
- query / translate address
- extended testing of unmap
*/

int main(int argc, char *argv[])
{
    Suite *s = suite_create("SecureCells");
    
    TCase *tc_core = tcase_create("Core");
    TCase *tc_macros = tcase_create("Macros");
    TCase *tc_map = tcase_create("Map");
    TCase *tc_unmap = tcase_create("Unmap");

    tcase_add_test(tc_macros, test_getDescFields);
    tcase_add_test(tc_macros, test_setDescFields);
    tcase_add_test(tc_macros, test_cellflags);
    tcase_add_test(tc_macros, test_check_cellflags);
    tcase_add_test(tc_macros, test_metafield);

    tcase_add_test(tc_core, test_cellnumber);
    tcase_add_test(tc_map, test_map_description);
    tcase_add_test(tc_map, test_map_sorted);
    tcase_add_test(tc_map, test_map_permissions);
    tcase_add_test(tc_unmap, test_unmap);

    suite_add_tcase(s, tc_core);
    suite_add_tcase(s, tc_map);
    
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}