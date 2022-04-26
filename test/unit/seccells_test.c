
#include <runtime.h>
#include <stdlib.h>
#include <check.h>
#include <stdio.h>
#include <seccells.h>

#define print_u64(val, msg) printf("%s: %016lx\n", msg, (long unsigned int) val)
#define print_u8(val, msg) printf("%s: %02x\n", msg, (unsigned char) val)
#define print_desc(d, msg) printf("%s: %016lx%016lx\n", msg, (long unsigned int) d.upper, (long unsigned int) d.lower)

START_TEST (test_getDescFields) {
    rt_desc desc;
    desc.upper = 0xdddcccccccccccaa;
    desc.lower = 0xaaaaaaabbbbbbbbb;
    print_desc(desc, "before");

    u64 ans = get_pbase(desc);
    print_u64(ans, "pbase");
    ck_assert(0xccccccccccc == ans);

    ans = get_vbase(desc);
    print_u64(ans, "vbase");
    ck_assert(0xbbbbbbbbb == ans);
    
    ans = get_vbound(desc);
    print_u64(ans, "vbound");
    ck_assert(0xaaaaaaaaa == ans);

    //MISSING: valid bit
    
}
END_TEST

START_TEST (test_setDescFields) {
    rt_desc desc;
    desc.upper = 0;
    desc.lower = 0;

    print_desc(desc, "before");
    set_pbase(&desc, 0xdeadbeef);
    print_desc(desc, "insert pbase");
    ck_assert(desc.upper == 0x000000deadbeef00);

    set_vbase(&desc, 0xcafecafe);
    print_desc(desc, "insert vbase");
    ck_assert(desc.lower == 0x00000000cafecafe);

    set_vbound(&desc, 0xaaaabbbb);
    print_desc(desc, "insert bound");
    ck_assert(desc.upper == 0x000000deadbeef0a);
    ck_assert(desc.lower == 0xaaabbbb0cafecafe);


}
END_TEST
/* RSW | A | G | U | X | W | R | V */
START_TEST (test_cellflags) {
    cellflags tmp;
    tmp.w = 0x00;
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
    cellflags tmp;
    tmp.w = 0x00; // ---
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

START_TEST (test_cellnumber) {
    rt_desc *desc = malloc(4 * sizeof(rt_desc));
    ((rt_meta *)desc)->N = 3;
    ((rt_meta *)desc)->M = 1;
    ((rt_meta *)desc)->T = 1;
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
    ck_assert(cell_number_from_va(0x008001) == 1);
    ck_assert(cell_number_from_va(0x00a000) == 1);
    ck_assert(cell_number_from_va(0x00f000) == 2);
    ck_assert(cell_number_from_va(0x00ffff) == 2);
    ck_assert(cell_number_from_va(0x01ffff) == 3);
    ck_assert(cell_number_from_va(0x0effff) == 3);
    // Missing: case of virtual address not mapped
}
END_TEST

START_TEST (test_map) {
    rt_desc * desc = malloc(12 * sizeof(rt_desc));
    rtbaseaddr = (u64) desc; //this is the replacement for the legitimate tablebase
    rt_meta meta = {
        .M = 1,
        .N = 0,
        .S = 2,
        .T = 1
    };
    *(rt_meta *)desc = meta;
    cellflags flags;
    flags.w = 0x00;
    map(0x1000, 0x8000, 0x0080, cellflags_writable(flags));
    map(0x3000, 0x8000, 0x0080, cellflags_writable(flags));
    print_desc(desc[1], "first insert");
    print_desc(desc[2], "second insert");
    ck_assert(get_vbase(desc[1])  == 0x1000);
    ck_assert(get_vbound(desc[1]) == 0x1080);
    ck_assert(get_vbase(desc[2])  == 0x3000);
    ck_assert(get_vbound(desc[2]) == 0x3080);
    map(0x2000, 0x8000, 0x0080, cellflags_writable(flags));
    ck_assert(get_vbase(desc[1])  == 0x1000);
    ck_assert(get_vbound(desc[1]) == 0x1080);
    ck_assert(get_vbase(desc[2])  == 0x2000);
    ck_assert(get_vbound(desc[2]) == 0x2080);
    ck_assert(get_vbase(desc[3])  == 0x3000);
    ck_assert(get_vbound(desc[3]) == 0x3080);
}
END_TEST

int main(int argc, char *argv[])
{
    Suite *s = suite_create("SecureCells");
    
    TCase *tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_getDescFields);
    tcase_add_test(tc_core, test_setDescFields);
    tcase_add_test(tc_core, test_cellflags);
    tcase_add_test(tc_core, test_check_cellflags);
    tcase_add_test(tc_core, test_cellnumber);
    tcase_add_test(tc_core, test_map);

    suite_add_tcase(s, tc_core);
    
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    
    //msg_err("something went terribly wrong, basil\n");
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}