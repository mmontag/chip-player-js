#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "test.h"
#include "../src/list.h"

struct test {
	struct list_head list;
	char *name;
	int (*func)(void);
};

static LIST_HEAD(test_list);

#define add_test(x) _add_test(#x, _test_func_##x)

void _add_test(char *name, int (*func)(void))
{
	struct test *t;

	t = malloc(sizeof (struct test));
	if (t == NULL)
		return;
	t->name = name;
	t->func = func;
	list_add_tail(&t->list, &test_list);
}

void run_tests()
{
	struct list_head *tmp;
	int total, fail;
	pid_t pid;
	int status;
	int color = 0;

	total = fail = 0;

	if (isatty(STDOUT_FILENO)) {
		color = 1;
	}

	list_for_each(tmp, &test_list) {
		struct test *t = list_entry(tmp, struct test, list);

		printf("test %d: %s: ", total, t->name);
		fflush(stdout);

		if ((pid = fork()) == 0) {
			exit(t->func());
		}

		waitpid(pid, &status, 0);

		if (status != 0) {
			fail++;
			if (WIFSIGNALED(status)) {
				printf("%s: ", sys_siglist[WTERMSIG(status)]);
			}
			if (color) {
				printf("\x1b[1;31m**fail**\x1b[0m\n");
			} else {
				printf("**fail**\n");
			}
		} else {
			if (color) {
				printf("\x1b[1;32mpass\x1b[0m\n");
			} else {
				printf("pass\n");
			}
		}
		total++;
	}

	printf("total:%d  failed:%d\n", total, fail);
}

int main()
{
#define declare_test(x) add_test(x)
#include "all_tests.c"
#undef declare_test

	run_tests();

	return 0;
}
