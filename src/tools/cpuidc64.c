/*
;  nasm -f elf64 -g -F stabs cpuida64.asm   for cpuida64.o
;  gcc cpuidc64.c -m64 -c                   for cpuidc64.o
;  gcc test.c cpuidc64.o cpuida64.o -m64 -lrt -lc -lm -o test
;  ./test
*/

#include "cpuidc64.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

char timeday[30];
double theseSecs = 0.0;
double startSecs = 0.0;
double secs;
int millisecs;

#if defined(__MACH__) && !defined(CLOCK_REALTIME)
#include <sys/time.h>
#define CLOCK_REALTIME 0
// clock_gettime is not implemented on older versions of OS X (< 10.12).
// If implemented, CLOCK_REALTIME will have already been defined.
int clock_gettime(int clk_id, struct timespec* t) {
        struct timeval now;
        int rv = gettimeofday(&now, NULL);
        if (rv) return rv;
        t->tv_sec  = now.tv_sec;
        t->tv_nsec = now.tv_usec * 1000;
        return 0;
}
#endif

#ifndef __MACH__

#ifndef NXDK
#include <sys/sysinfo.h>
#endif

#else
#include <sys/sysctl.h>
int get_nprocs_conf() {
    return sysconf(_SC_NPROCESSORS_CONF);
}

int get_nprocs() {
    return sysconf(_SC_NPROCESSORS_CONF);
}

long get_phys_pages() {
#ifdef _SC_PAGE_SIZE
    return sysconf(_SC_PHYS_PAGES);
#else
    uint64_t mem;
    size_t len = sizeof(mem);
    sysctlbyname("hw.memsize", &mem, &len, NULL, 0);
    return mem/sysconf(_SC_PAGE_SIZE);
#endif
}

int getpagesize() {
    return sysconf(_SC_PAGE_SIZE);
}
#endif

void local_time() {
// FIXME: PDClib seems to have these functions, why are we crashing?
#ifndef NXDK
    time_t t;

    t = time(NULL);
    sprintf(timeday, "%s", asctime(localtime(&t)));
#endif
    return;
}

struct timespec tp1;

void getSecs() {
#ifdef NXDK
    timespec_get(&tp1, 1);
#else
    clock_gettime(CLOCK_REALTIME, &tp1);
#endif

    theseSecs = tp1.tv_sec + tp1.tv_nsec / 1e9;
    return;
}

void start_time() {
    getSecs();
    startSecs = theseSecs;
    return;
}

void end_time() {
    getSecs();
    secs = theseSecs - startSecs;
    millisecs = (int) (1000.0 * secs);
    return;
}
