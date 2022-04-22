
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

int main(int argc, char *argv[])
{
    Suite *s = suite_create("SecureCells");
    
    TCase *tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_getDescFields);
    tcase_add_test(tc_core, test_setDescFields);
    tcase_add_test(tc_core, test_cellflags);
    tcase_add_test(tc_core, test_check_cellflags);
    

    suite_add_tcase(s, tc_core);
    
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    
    //msg_err("something went terribly wrong, basil\n");
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}