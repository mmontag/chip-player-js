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

static char *color_fail = "";
static char *color_pass = "";
static char *color_test = "";
static char *color_none = "";

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

void init_colors()
{
	if (isatty(STDOUT_FILENO)) {
		color_fail = "\x1b[1;31m";
		color_pass = "\x1b[1;32m";
		color_test = "\x1b[1m";
		color_none = "\x1b[0m";
	}
}

int run_tests()
{
	struct list_head *tmp;
	int total, fail;
	pid_t pid;
	int status;

	total = fail = 0;

	init_colors();

	list_for_each(tmp, &test_list) {
		struct test *t = list_entry(tmp, struct test, list);

		printf("%stest %d:%s %s: ",
				color_test, total, color_none, t->name);
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
			printf("%s**fail**%s\n", color_fail, color_none);
		} else {
			printf("%spass%s\n", color_pass, color_none);
		}
		total++;
	}

	printf("%stotal:%d  passed:%d (%4.1f%%)  failed:%d (%4.1f%%)%s\n",
		color_test, total,
		(total - fail), 100.0 * (total - fail) / total,
		fail, 100.0 * fail / total, color_none);

	return -fail;
}

int main()
{
#define declare_test(x) add_test(x)
#include "all_tests.c"
#undef declare_test

	if (run_tests() == 0) {
		exit(EXIT_SUCCESS);
	} else {
		exit(EXIT_FAILURE);
	}
}
