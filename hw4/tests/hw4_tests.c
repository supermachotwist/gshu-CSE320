#include <criterion/criterion.h>
#include <criterion/logging.h>

#include "legion.h"

#define PROGNAME "bin/legion"
#define VALGRIND "valgrind --error-exitcode=37 --child-silent-after-fork=yes"

/*
 * The following just tests that your command-line interface does not loop
 * when it gets an end-of-file on the standard input.  This will be important
 * when during our grading tests, which will use a driver program to interact
 * with yours, rather than entering commands manually.
 */

Test(basecode_tests_suite, startup_test, .timeout=5) {
    char *cmd = PROGNAME " < /dev/null";
    int return_code = WEXITSTATUS(system(cmd));

    cr_assert_eq(return_code, EXIT_SUCCESS,
                 "Program exited with %d instead of EXIT_SUCCESS",
		 return_code);
}

/*
 * You can make some tests that pipe a series of commands into the standard input
 * of your program, as in the following.  Due to the concurrent nature of the
 * application, though, it requires a more sophisticated test setup to fully
 * analyze the output.  It is probably useful to have some tests like this, though,
 * even if you have to "eyeball" the output, because they will provide a quick
 * way to make sure that your program doesn't crash.
 */

Test(basecode_tests_suite, lazy_test, .timeout=5) {
    char *cmd = "(echo 'register lazy lazy'; echo 'start lazy'; echo 'quit') | " PROGNAME;
    int return_code = WEXITSTATUS(system(cmd));

    cr_assert_eq(return_code, EXIT_SUCCESS,
                 "Program exited with %d instead of EXIT_SUCCESS",
		 return_code);
}

/*
 * The "yoyo" daemon sometimes misbehaves by failing to synchronize with its
 * parent when it starts, or ignoring requests to terminate.  These behaviors
 * should show up (sometimes) in this test.
 */

Test(basecode_tests_suite, yoyo_test, .timeout=5) {
    char *cmd = "(echo 'register yoyo yoyo'; echo 'start yoyo'; echo 'quit') | " PROGNAME;
    int return_code = WEXITSTATUS(system(cmd));

    cr_assert_eq(return_code, EXIT_SUCCESS,
                 "Program exited with %d instead of EXIT_SUCCESS",
		 return_code);
}

/*
 * You can run your program using valgrind as in the following test, to check for
 * uninitialized variables, storage leaks, and other memory errors.
 */

Test(basecode_tests_suite, simple_valgrind_test, .timeout=5) {
    char *cmd = "(echo 'register lazy lazy'; echo 'start lazy'; echo 'quit') | "
	         VALGRIND " " PROGNAME;
    int return_code = WEXITSTATUS(system(cmd));

    cr_assert_eq(return_code, EXIT_SUCCESS,
                 "Program exited with %d instead of EXIT_SUCCESS",
		 return_code);
}

