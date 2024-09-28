#include <time.h>

long syscall(long num, ...);

int nanosleep(const struct timespec *req, struct timespec *rem)
{
	return syscall(35, req, rem);
}
