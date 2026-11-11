/* Test the actual source function with independent Mach counter fixtures */
#include <mach/mach.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef uint64_t SIZE_T;
typedef uint32_t ULONG;
typedef struct {
    SIZE_T PeakVirtualSize, VirtualSize;
    ULONG PageFaultCount;
    SIZE_T PeakWorkingSetSize, WorkingSetSize;
    SIZE_T QuotaPeakPagedPoolUsage, QuotaPagedPoolUsage;
    SIZE_T QuotaPeakNonPagedPoolUsage, QuotaNonPagedPoolUsage;
    SIZE_T PagefileUsage, PeakPagefileUsage, PrivateUsage;
} VM_COUNTERS_EX;

static int vm_failure, basic_failure, calls, failures;
static kern_return_t fixture_task_info(task_name_t task, task_flavor_t flavor, task_info_t out, mach_msg_type_number_t *count)
{
    (void)task;
    calls++;
    if (flavor == MACH_TASK_BASIC_INFO) {
        if (basic_failure) return KERN_FAILURE;
        if (*count != MACH_TASK_BASIC_INFO_COUNT) return KERN_INVALID_ARGUMENT;
        mach_task_basic_info_data_t *p = (void *)out;
        memset(p, 0, sizeof(*p));
        p->virtual_size = UINT64_C(8589934592);
        p->resident_size = UINT64_C(67108864);
        p->resident_size_max = UINT64_C(83886080);
        return KERN_SUCCESS;
    }
    if (flavor == TASK_VM_INFO) {
        if (vm_failure) return KERN_FAILURE;
        if (*count != TASK_VM_INFO_COUNT) return KERN_INVALID_ARGUMENT;
        task_vm_info_data_t *p = (void *)out;
        memset(p, 0, sizeof(*p));
        p->phys_footprint = UINT64_C(100663296);
        p->resident_size = UINT64_C(67108864);
        return KERN_SUCCESS;
    }
    return KERN_INVALID_ARGUMENT;
}

#define task_info fixture_task_info
#include COUNTER_SOURCE
#undef task_info

static void check(const char *name, uint64_t actual, uint64_t expected)
{
    if (actual != expected) {
        printf("FAIL %s actual=%llu expected=%llu\n", name, (unsigned long long)actual, (unsigned long long)expected);
        failures++;
    }
}

int main(void)
{
    VM_COUNTERS_EX p = {0};
    fill_vm_counters(&p, -1);
    check("footprint differs from reserved address space and resident memory", p.PagefileUsage, UINT64_C(100663296));
    check("peak commit follows footprint", p.PeakPagefileUsage, UINT64_C(100663296));
    check("virtual size keeps existing behavior", p.VirtualSize, UINT64_C(8657043456));
    check("working set remains resident", p.WorkingSetSize, UINT64_C(67108864));
    check("peak working set preserved", p.PeakWorkingSetSize, UINT64_C(83886080));
    vm_failure = 1;
    memset(&p, 0, sizeof(p));
    fill_vm_counters(&p, -1);
    check("failed footprint query retains fallback", p.PagefileUsage, UINT64_C(8589934592));
    check("failed footprint query does not fabricate peak", p.PeakPagefileUsage, 0);
    vm_failure = 0;
    basic_failure = 1;
    memset(&p, 0, sizeof(p));
    fill_vm_counters(&p, -1);
    check("footprint works independently of basic query", p.PagefileUsage, UINT64_C(100663296));
    check("failed basic query leaves working set untouched", p.WorkingSetSize, 0);
    basic_failure = 0;
    memset(&p, 0x5a, sizeof(p));
    VM_COUNTERS_EX before = p;
    calls = 0;
    fill_vm_counters(&p, 1234);
    check("unsupported other-process query stays unchanged", memcmp(&p, &before, sizeof(p)) != 0, 0);
    check("other-process query makes no Mach calls", calls, 0);
    printf("RESULT failures=%d\n", failures);
    return failures != 0;
}
