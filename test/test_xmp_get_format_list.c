#include "test.h"

#include "format.h"
#include "loaders/prowizard/prowiz.h"

extern const struct pw_format *const pw_format[];
extern const struct format_loader *const format_loader[];

TEST(test_xmp_get_format_list)
{
	char **list;
	int count, i, j;

	list = xmp_get_format_list();
	fail_unless(list != 0, "returned NULL");

	for (count = i = 0; format_loader[i] != NULL; i++) {
		if (strcmp(format_loader[i]->name, "prowizard") == 0) {
			for (j = 0; pw_format[j] != NULL; j++) {
				fail_unless(count < MAX_FORMATS, "too many formats");
				fail_unless(strcmp(list[count++], pw_format[j]->name) == 0, "format name error");
			}
		} else {
			fail_unless(count < MAX_FORMATS, "too many formats");
			fail_unless(strcmp(list[count++], format_loader[i]->name) == 0, "format name error");
		}
	}
	fail_unless(count < MAX_FORMATS, "too many formats");
}
END_TEST
