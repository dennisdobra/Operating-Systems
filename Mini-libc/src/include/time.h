#ifndef _TIME_H
#define _TIME_H

struct timespec {
	long tv_sec;
	long tv_nsec;
};

int nanosleep(const struct timespec *req, struct timespec *rem);

#endif
