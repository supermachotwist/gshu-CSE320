#include <stdio.h>
#include <unistd.h>
#include <criterion/criterion.h>
#include <string.h>

#define TEST_TIMEOUT 15

#define TEST_REF_DIR "tests/rsrc"
#define TEST_OUTPUT_DIR "tests.out"

extern int errors, warnings;

static char program_options[500];
static char test_output_subdir[100];
static char test_log_outfile[100];

/*
 * Sets up to run a test.
 * Initialize various filenames, using the name of the test as a base,
 * and then initialize and run a command to remove old output from this test
 * and to make sure that the test output directory exists.
 */
int setup_test(char *name) {
    char cmd[1000];
    sprintf(test_log_outfile, "%s/%s", TEST_OUTPUT_DIR, name);
    sprintf(test_output_subdir, "%s/%s", TEST_OUTPUT_DIR, name);
    sprintf(cmd, "rm -f %s.out %s.err; rm -fr %s; mkdir -p %s",
	    test_log_outfile, test_log_outfile, test_output_subdir, test_output_subdir);
    fprintf(stderr, "setup(%s)\n", cmd);
    return system(cmd);
}

/*
 * Run the program as a "black box" using system().
 * A shell command is constructed and run that first performs test setup,
 * then runs the program to be tested with input redirected from a test input file
 * and standard and error output redirected to separate output files.
 */
int run_using_system(char *name, char *pre_cmd, char *valgrind_cmd) {
    char cmd[1000];
    setup_test(name);
    sprintf(cmd, "%s%s bin/finddup %s > %s.out 2> %s.err",
	    pre_cmd, valgrind_cmd, program_options, test_log_outfile, test_log_outfile);
    fprintf(stderr, "run(%s)\n", cmd);
    return system(cmd);
}

void assert_normal_exit(int status) {
    cr_assert_eq(status, 0, "The program did not exit normally (status = 0x%x).\n", status);
}

void assert_error_exit(int status) {
    cr_assert_eq(WEXITSTATUS(status), 0xff,
		 "The program did not exit with status 0xff (status was 0x%x).\n",
		 status);
}

/*
 * Compare the standard output from the program being tested with reference output,
 * after first possibly using "grep" to remove lines that match a filter pattern.
 */
void assert_outfile_matches(char *name, char *filter) {
    char cmd[500];
    if(filter) {
	sprintf(cmd, "grep -v '%s' %s.out > %s_A.out; grep -v '%s' %s/%s.out > %s_B.out; "
		     "diff --ignore-tab-expansion --ignore-trailing-space --ignore-space-change --ignore-blank-lines %s_A.out %s_B.out",
		     filter, test_log_outfile, name,
		     filter, TEST_REF_DIR, name, name,
		     name, name);
    } else {
	sprintf(cmd, "diff --ignore-tab-expansion --ignore-trailing-space --ignore-space-change --ignore-blank-lines %s.out %s/%s.out", test_log_outfile, TEST_REF_DIR, name);
    }
    int err = system(cmd);
    cr_assert_eq(err, 0, "The output was not what was expected (diff exited with status %d).\n", WEXITSTATUS(err));
}

/*
 * Compare the standard error output from the program being tested with reference output,
 * after first possibly using "grep" to remove lines that match a filter pattern.
 */
void assert_errfile_matches(char *name, char *filter) {
    char cmd[500];
    if(filter) {
	sprintf(cmd, "grep -v '%s' %s.err > %s_A.err; grep -v '%s' %s/%s.err > %s_B.err; "
		     "diff ---ignore-tab-expansion --ignore-trailing-space --ignore-space-change --ignore-blank-lines %s_A.err %s_B.err",
		     filter, test_log_outfile, name,
		     filter, TEST_REF_DIR, name, name,
		     name, name);
    } else {
	sprintf(cmd, "diff --ignore-tab-expansion --ignore-trailing-space --ignore-space-change --ignore-blank-lines %s.err %s/%s.err", test_log_outfile, TEST_REF_DIR, name);
    }
    int err = system(cmd);
    cr_assert_eq(err, 0, "The output was not what was expected (diff exited with status %d).\n", WEXITSTATUS(err));
}

void assert_no_valgrind_errors(int status) {
    cr_assert_neq(WEXITSTATUS(status), 37, "Valgrind reported errors -- see %s.err", test_log_outfile);
}

/*
 * Tests the basic program operation.
 */
Test(base_suite, quick_test) {
    char *name = "quick_test";
    sprintf(program_options, "tests/rsrc/quick_test_names");
    int err = run_using_system(name, "", "");
    assert_normal_exit(err);
    assert_outfile_matches(name, NULL);
    assert_errfile_matches(name, NULL);
}

/*
 * This test sets up a test tree that contains "hard links" to the same file.
 * These should be correctly identified as such by the program.
 */
Test(base_suite, hard_links_test) {
    char *name = "hard_links_test";
    system("rm -f tests/rsrc/test_tree/file1.lnk tests/rsrc/test_tree/file2.lnk; "
           "ln tests/rsrc/test_tree/file1 tests/rsrc/test_tree/file1.lnk; "
           "ln tests/rsrc/test_tree/file2 tests/rsrc/test_tree/file2.lnk");
    sprintf(program_options, "tests/rsrc/hard_links_test_names");
    int err = run_using_system(name, "", "");
    assert_normal_exit(err);
    assert_outfile_matches(name, NULL);
    assert_errfile_matches(name, NULL);
}

/*
 * Tests correct behavior of the program on binary files.
 */
Test(base_suite, binary_test) {
    char *name = "binary_test";
    sprintf(program_options, "tests/rsrc/binary_test_names");
    int err = run_using_system(name, "", "");
    assert_normal_exit(err);
    assert_outfile_matches(name, NULL);
    assert_errfile_matches(name, NULL);
}

/*
 * Tests the basic program operation with a larger set of files, containing
 * examples of various types of potential issues.
 */
Test(base_suite, larger_test) {
    char *name = "larger_test";
    sprintf(program_options, "tests/rsrc/larger_test_names");
    int err = run_using_system(name, "", "");
    assert_normal_exit(err);
    assert_outfile_matches(name, NULL);
    assert_errfile_matches(name, NULL);
}

/*
 * This test runs valgrind to check for the use of uninitialized variables.
 */
Test(base_suite, valgrind_uninitialized_test) {
    char *name = "valgrind_uninitialized_test";
    sprintf(program_options, "tests/rsrc/quick_test_names");
    int err = run_using_system(name, "", "valgrind --leak-check=no --undef-value-errors=yes --error-exitcode=37");
    assert_no_valgrind_errors(err);
    assert_normal_exit(err);
    assert_outfile_matches(name, NULL);
}

/*
 * This test runs valgrind to check for memory leaks.
 */
Test(base_suite, valgrind_leak_test) {
    char *name = "valgrind_leak_test";
    sprintf(program_options, "tests/rsrc/quick_test_names");
    int err = run_using_system(name, "", "valgrind --leak-check=full --undef-value-errors=no --error-exitcode=37");
    assert_no_valgrind_errors(err);
    assert_normal_exit(err);
    assert_outfile_matches(name, NULL);
}
