#include <string.h>
#include <unistd.h>

int puts(const char *str)
{
	size_t len = strlen(str);

	write(1, str, len);
	write(1, "\n", 1);
	return 1;
}
