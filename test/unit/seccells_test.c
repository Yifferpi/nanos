
#include <runtime.h>
#include <stdlib.h>
#include <check.h>

START_TEST (test_one) {
    ck_assert(true);
}
END_TEST

START_TEST (test_two) {
    ck_assert(false);
}
END_TEST

int main(int argc, char *argv[])
{
    Suite *s = suite_create("SecureCells");
    
    TCase *tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_one);
    tcase_add_test(tc_core, test_two);

    suite_add_tcase(s, tc_core);
    
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    
    //msg_err("something went terribly wrong, basil\n");
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}