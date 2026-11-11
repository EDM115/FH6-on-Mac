/* Read native process memory counters without reading process memory */
#include <libproc.h>
#include <sys/proc_info.h>
#include <sys/resource.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(int argc, char **argv)
{
    char *end;
    if (argc != 2) return 2;
    long parsed = strtol(argv[1], &end, 10);
    if (!*argv[1] || *end || parsed <= 0 || parsed > INT_MAX) return 2;
    int pid = (int)parsed;
    struct rusage_info_v4 usage = {0};
    struct proc_taskinfo task = {0};
    struct timespec now;
    if (proc_pid_rusage(pid, RUSAGE_INFO_V4, (rusage_info_t *)&usage) != 0) {
        perror("proc_pid_rusage");
        return 1;
    }
    if (proc_pidinfo(pid, PROC_PIDTASKINFO, 0, &task, sizeof(task)) != sizeof(task)) {
        perror("proc_pidinfo");
        return 1;
    }
    clock_gettime(CLOCK_REALTIME, &now);
    printf("{\"pid\":%d,\"unix_time\":%.3f,\"footprint_bytes\":%llu,\"resident_bytes\":%llu,\"virtual_bytes\":%llu,\"lifetime_peak_footprint_bytes\":%llu}\n", pid, now.tv_sec + now.tv_nsec / 1e9, (unsigned long long)usage.ri_phys_footprint, (unsigned long long)usage.ri_resident_size, (unsigned long long)task.pti_virtual_size, (unsigned long long)usage.ri_lifetime_max_phys_footprint);
    return 0;
}
