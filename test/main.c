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

	total = fail = 0;

	list_for_each(tmp, &test_list) {
		struct test *t = list_entry(tmp, struct test, list);
		printf("test %d: %s: ", total, t->name);
		if (t->func() < 0) {
			fail++;
			printf(": **fail**\n");
		} else {
			printf("pass\n");
		}
		total++;
	}

	printf("total:%d  failed:%d\n", total, fail);
}

int main()
{
	add_test(test_pp);
	add_test(test_sqsh);
	add_test(test_s404);
	add_test(test_mmcmp);
	run_tests();

	return 0;
}
