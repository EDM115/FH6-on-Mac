#include <windows.h>
#include <mmreg.h>
#include <spatialaudioclient.h>
#include <stdio.h>

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define TRACE(...) do {} while (0)
#if TEST_PATCHED
#include "map-patched.h"
#else
#include "map-control.h"
#endif

static int failures;
struct Expected { UINT index, channel; };

static void check(const char *name, AudioObjectType objects, WORD expected_count, DWORD expected_mask, const struct Expected *entries, UINT n)
{
    WORD count = 0;
    DWORD mask = 0;
    UINT32 actual[17], expected[17];
    for (UINT i = 0; i < 17; i++) actual[i] = expected[i] = ~0u;
    for (UINT i = 0; i < n; i++) expected[entries[i].index] = entries[i].channel;
    static_mask_to_channels(objects, &count, &mask, actual);
    if (count != expected_count || mask != expected_mask) {
        printf("FAIL %s count=%u expected=%u mask=0x%lx expected=0x%lx\n", name, count, expected_count, (unsigned long)mask, (unsigned long)expected_mask);
        failures++;
    }
    for (UINT i = 0; i < 17; i++) if (actual[i] != expected[i]) {
        printf("FAIL %s object_index=%u channel=%u expected=%u\n", name, i, actual[i], expected[i]);
        failures++;
    }
}

int main(void)
{
    /* WAVEFORMATEXTENSIBLE orders FL FR FC LFE BL BR SL SR, object indices place SL SR before BL BR */
    AudioObjectType bed71 = AudioObjectType_FrontLeft | AudioObjectType_FrontRight | AudioObjectType_FrontCenter | AudioObjectType_LowFrequency | AudioObjectType_SideLeft | AudioObjectType_SideRight | AudioObjectType_BackLeft | AudioObjectType_BackRight;
    const struct Expected expected71[] = {{0,0},{1,1},{2,2},{3,3},{4,6},{5,7},{6,4},{7,5}};
    const struct Expected expected714[] = {{0,0},{1,1},{2,2},{3,3},{4,6},{5,7},{6,4},{7,5},{8,8},{9,9},{10,10},{11,11}};
    const struct Expected back_center[] = {{16,0}};
    const struct Expected mixed[] = {{1,0},{16,1},{4,2},{11,3}};
    check("7.1", bed71, 8, 0x63f, expected71, ARRAY_SIZE(expected71));
    check("7.1.4", bed71 | AudioObjectType_TopFrontLeft | AudioObjectType_TopFrontRight | AudioObjectType_TopBackLeft | AudioObjectType_TopBackRight, 12, 0x2d63f, expected714, ARRAY_SIZE(expected714));
    check("back-center", AudioObjectType_BackCenter, 1, 0x100, back_center, ARRAY_SIZE(back_center));
    check("mixed subset", AudioObjectType_FrontRight | AudioObjectType_BackCenter | AudioObjectType_SideLeft | AudioObjectType_TopBackRight, 4, 0x20302, mixed, ARRAY_SIZE(mixed));
    check("empty", 0, 0, 0, NULL, 0);
    printf("RESULT: %s (%d failures)\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
