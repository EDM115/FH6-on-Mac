/* ts_repro: minimal D3D12 timestamp-query reproducer (no game assets).
 *
 * What it does, in order:
 *   1. LoadLibrary("d3d12.dll") and print which file was loaded (and whether a proxy's apd12.dll is
 *      behind it), plus the DXGI adapter it ends up on.
 *   2. Create a device (feature level 11_0) and a DIRECT command queue.
 *   3. Print ID3D12CommandQueue::GetTimestampFrequency and GetClockCalibration (before and after).
 *   4. Create a TIMESTAMP query heap with 2*N entries and a READBACK buffer, pre-filled with the
 *      pattern 0xA5A5A5A5A5A5A5A5 so "ResolveQueryData wrote nothing" is visible.
 *   5. For each of N frames: EndQuery(2i), a trivial workload (ClearRenderTargetView of a 256x256
 *      RGBA8 target, or with --copy a 64 KiB CopyBufferRegion), EndQuery(2i+1),
 *      ResolveQueryData(2i..2i+1) into the readback buffer, execute, wait on a fence, Sleep(--sleep ms).
 *   6. Print raw ticks, in-frame deltas (end - begin), frame-to-frame deltas (begin_i - begin_{i-1}),
 *      their value in microseconds at the reported frequency, and the elapsed CPU time (QPC) for
 *      comparison. Flags: 0x8000000000000000 sentinel, zero, the untouched pre-fill pattern,
 *      non-advancing (frame-to-frame delta <= 0), end < begin.
 * Exit code: 0 = timestamps look sane, 1 = at least one timestamp flag raised, 2 = setup failure.
 * GetClockCalibration is printed with a WARN when it does not advance, but does not change the exit code.
 *
 * This local revision removes the undefined never-written query and fails before readback on any synchronization error
 * Usage: ts_repro.exe [--frames N] [--sleep MS] [--copy]
 * Build (mingw-w64, Linux or macOS):
 *   x86_64-w64-mingw32-gcc -O2 -Wall -Wextra -o ts_repro.exe ts_repro.c -ldxguid
 * Original source status: compiled on Linux (Debian 13, gcc-mingw-w64-x86-64 14-win32) on 2026-10-08, no warnings.
 *         Original source smoke-run only under Debian's Wine 10.0 + upstream vkd3d + Mesa lavapipe (software Vulkan,
 *         Xvfb): PASS, see README.md. NOT run on Windows, CrossOver, GPTK or D3DMetal.
 */
#define COBJMACROS
#define WIDL_C_INLINE_WRAPPERS   /* struct-returning methods (GetAdapterLuid, GetCPUDescriptorHandleForHeapStart) get the right ABI */
#include <windows.h>
#include <initguid.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define SENTINEL 0x8000000000000000ULL
#define PREFILL  0xA5A5A5A5A5A5A5A5ULL
#define MAX_FRAMES 1000

static void say(const char *fmt, ...)
{
    va_list ap; va_start(ap, fmt); vfprintf(stdout, fmt, ap); va_end(ap);
    fputc('\n', stdout); fflush(stdout);
}

static const char *flag_of(UINT64 v)
{
    if (v == SENTINEL) return " <-- 0x8000000000000000 sentinel";
    if (v == PREFILL)  return " <-- untouched (ResolveQueryData wrote nothing)";
    if (v == 0)        return " <-- zero";
    return "";
}

typedef HRESULT (WINAPI *create_device_t)(IUnknown *, D3D_FEATURE_LEVEL, REFIID, void **);
typedef HRESULT (WINAPI *create_factory2_t)(UINT, REFIID, void **);

static void print_adapter(ID3D12Device *dev)
{
    LUID luid = ID3D12Device_GetAdapterLuid(dev);
    HMODULE dxgi = LoadLibraryA("dxgi.dll");
    create_factory2_t cf2 = dxgi ? (create_factory2_t)(void *)GetProcAddress(dxgi, "CreateDXGIFactory2") : NULL;
    IDXGIFactory4 *fx = NULL; IDXGIAdapter1 *ad = NULL;
    say("device adapter LUID %08lx:%08lx", (unsigned long)luid.HighPart, (unsigned long)luid.LowPart);
    if (!cf2 || FAILED(cf2(0, &IID_IDXGIFactory4, (void **)&fx)) || !fx) { say("dxgi: no factory"); return; }
    if (SUCCEEDED(IDXGIFactory4_EnumAdapterByLuid(fx, luid, &IID_IDXGIAdapter1, (void **)&ad)) && ad) {
        DXGI_ADAPTER_DESC1 d; memset(&d, 0, sizeof d);
        IDXGIAdapter1_GetDesc1(ad, &d);
        say("adapter: \"%ls\" vendor 0x%04x device 0x%04x", d.Description, d.VendorId, d.DeviceId);
        IDXGIAdapter1_Release(ad);
    } else say("adapter: EnumAdapterByLuid failed");
    IDXGIFactory4_Release(fx);
}

static void calibration(ID3D12CommandQueue *q, const char *when, UINT64 *gpu, UINT64 *cpu)
{
    HRESULT hr = ID3D12CommandQueue_GetClockCalibration(q, gpu, cpu);
    say("GetClockCalibration (%s): hr 0x%08lx gpu %llu (0x%016llx)%s cpu %llu", when, (unsigned long)hr,
        (unsigned long long)*gpu, (unsigned long long)*gpu, flag_of(*gpu), (unsigned long long)*cpu);
}

int main(int argc, char **argv)
{
    unsigned frames = 10, sleep_ms = 16, use_copy = 0, flags = 0, i;
    char path[MAX_PATH];
    HRESULT hr;

    for (i = 1; i < (unsigned)argc; i++) {
        if (!strcmp(argv[i], "--frames") && i + 1 < (unsigned)argc) frames = (unsigned)atoi(argv[++i]);
        else if (!strcmp(argv[i], "--sleep") && i + 1 < (unsigned)argc) sleep_ms = (unsigned)atoi(argv[++i]);
        else if (!strcmp(argv[i], "--copy")) use_copy = 1;
        else { say("usage: ts_repro.exe [--frames N] [--sleep MS] [--copy]"); return 2; }
    }
    if (frames < 2) frames = 2;
    if (frames > MAX_FRAMES) frames = MAX_FRAMES;

    HMODULE m = LoadLibraryA("d3d12.dll");
    if (!m) { say("LoadLibrary(d3d12.dll) failed: %lu", GetLastError()); return 2; }
    GetModuleFileNameA(m, path, sizeof path);
    say("ts_repro: frames %u, sleep %u ms, workload %s", frames, sleep_ms, use_copy ? "CopyBufferRegion 64 KiB" : "ClearRenderTargetView 256x256 RGBA8");
    say("d3d12 module: %s", path);
    create_device_t create = (create_device_t)(void *)GetProcAddress(m, "D3D12CreateDevice");
    if (!create) { say("no D3D12CreateDevice export"); return 2; }

    ID3D12Device *dev = NULL;
    hr = create(NULL, D3D_FEATURE_LEVEL_11_0, &IID_ID3D12Device, (void **)&dev);
    say("D3D12CreateDevice(11_0): 0x%08lx", (unsigned long)hr);
    if (FAILED(hr) || !dev) return 2;
    { HMODULE real = GetModuleHandleA("apd12.dll");
      if (real) { GetModuleFileNameA(real, path, sizeof path); say("proxy in front: real D3D12 implementation is %s", path); } }
    print_adapter(dev);

    ID3D12CommandQueue *q = NULL; ID3D12CommandAllocator *al = NULL; ID3D12GraphicsCommandList *cl = NULL;
    ID3D12QueryHeap *qh = NULL; ID3D12Resource *rb = NULL, *rt = NULL, *src = NULL, *dst = NULL;
    ID3D12DescriptorHeap *rtv_heap = NULL; ID3D12Fence *fence = NULL;
    D3D12_COMMAND_QUEUE_DESC qd = { D3D12_COMMAND_LIST_TYPE_DIRECT, 0, D3D12_COMMAND_QUEUE_FLAG_NONE, 0 };
    if (FAILED(hr = ID3D12Device_CreateCommandQueue(dev, &qd, &IID_ID3D12CommandQueue, (void **)&q))) { say("CreateCommandQueue 0x%08lx", (unsigned long)hr); return 2; }

    UINT64 freq = 0, gpu0 = 0, cpu0 = 0, gpu1 = 0, cpu1 = 0;
    LARGE_INTEGER qpf; QueryPerformanceFrequency(&qpf);
    hr = ID3D12CommandQueue_GetTimestampFrequency(q, &freq);
    say("GetTimestampFrequency: hr 0x%08lx -> %llu Hz (QPC frequency %lld Hz)", (unsigned long)hr, (unsigned long long)freq, (long long)qpf.QuadPart);
    if (FAILED(hr) || !freq) { say("FLAG: no usable timestamp frequency"); flags++; }
    calibration(q, "start", &gpu0, &cpu0);

    D3D12_QUERY_HEAP_DESC hd = { D3D12_QUERY_HEAP_TYPE_TIMESTAMP, 2 * frames, 0 };
    hr = ID3D12Device_CreateQueryHeap(dev, &hd, &IID_ID3D12QueryHeap, (void **)&qh);
    say("CreateQueryHeap(TIMESTAMP, %u): 0x%08lx", 2 * frames, (unsigned long)hr);
    if (FAILED(hr)) return 2;

    D3D12_HEAP_PROPERTIES readback = { D3D12_HEAP_TYPE_READBACK, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 0, 0 };
    D3D12_HEAP_PROPERTIES deflt = { D3D12_HEAP_TYPE_DEFAULT, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 0, 0 };
    D3D12_RESOURCE_DESC buf = { D3D12_RESOURCE_DIMENSION_BUFFER, 0, (UINT64)(2 * frames) * sizeof(UINT64), 1, 1, 1,
                                DXGI_FORMAT_UNKNOWN, {1, 0}, D3D12_TEXTURE_LAYOUT_ROW_MAJOR, D3D12_RESOURCE_FLAG_NONE };
    if (FAILED(hr = ID3D12Device_CreateCommittedResource(dev, &readback, D3D12_HEAP_FLAG_NONE, &buf, D3D12_RESOURCE_STATE_COPY_DEST,
                                                         NULL, &IID_ID3D12Resource, (void **)&rb))) { say("readback buffer 0x%08lx", (unsigned long)hr); return 2; }
    {   /* pre-fill so an untouched slot is recognisable */
        UINT64 *p = NULL; D3D12_RANGE none = { 0, 0 };
        if (SUCCEEDED(ID3D12Resource_Map(rb, 0, &none, (void **)&p)) && p) {
            for (i = 0; i < 2 * frames; i++) p[i] = PREFILL;
            ID3D12Resource_Unmap(rb, 0, NULL);
        } else say("note: could not pre-fill the readback buffer");
    }

    if (use_copy) {
        D3D12_RESOURCE_DESC b64 = { D3D12_RESOURCE_DIMENSION_BUFFER, 0, 65536, 1, 1, 1, DXGI_FORMAT_UNKNOWN, {1, 0},
                                    D3D12_TEXTURE_LAYOUT_ROW_MAJOR, D3D12_RESOURCE_FLAG_NONE };
        if (FAILED(hr = ID3D12Device_CreateCommittedResource(dev, &deflt, D3D12_HEAP_FLAG_NONE, &b64, D3D12_RESOURCE_STATE_COMMON, NULL, &IID_ID3D12Resource, (void **)&src)) ||
            FAILED(hr = ID3D12Device_CreateCommittedResource(dev, &deflt, D3D12_HEAP_FLAG_NONE, &b64, D3D12_RESOURCE_STATE_COMMON, NULL, &IID_ID3D12Resource, (void **)&dst)))
        { say("copy buffers 0x%08lx", (unsigned long)hr); return 2; }
    } else {
        D3D12_RESOURCE_DESC tex = { D3D12_RESOURCE_DIMENSION_TEXTURE2D, 0, 256, 256, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, {1, 0},
                                    D3D12_TEXTURE_LAYOUT_UNKNOWN, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET };
        D3D12_DESCRIPTOR_HEAP_DESC dh = { D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 0 };
        if (FAILED(hr = ID3D12Device_CreateCommittedResource(dev, &deflt, D3D12_HEAP_FLAG_NONE, &tex, D3D12_RESOURCE_STATE_RENDER_TARGET, NULL, &IID_ID3D12Resource, (void **)&rt)) ||
            FAILED(hr = ID3D12Device_CreateDescriptorHeap(dev, &dh, &IID_ID3D12DescriptorHeap, (void **)&rtv_heap)))
        { say("render target 0x%08lx", (unsigned long)hr); return 2; }
        D3D12_CPU_DESCRIPTOR_HANDLE h = ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(rtv_heap);
        ID3D12Device_CreateRenderTargetView(dev, rt, NULL, h);
    }
    if (FAILED(hr = ID3D12Device_CreateFence(dev, 0, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, (void **)&fence)) ||
        FAILED(hr = ID3D12Device_CreateCommandAllocator(dev, D3D12_COMMAND_LIST_TYPE_DIRECT, &IID_ID3D12CommandAllocator, (void **)&al)) ||
        FAILED(hr = ID3D12Device_CreateCommandList(dev, 0, D3D12_COMMAND_LIST_TYPE_DIRECT, al, NULL, &IID_ID3D12GraphicsCommandList, (void **)&cl)))
    { say("fence/allocator/list 0x%08lx", (unsigned long)hr); return 2; }
    if (FAILED(hr = ID3D12GraphicsCommandList_Close(cl))) { say("initial Close 0x%08lx", (unsigned long)hr); return 2; }

    HANDLE ev = CreateEventA(NULL, FALSE, FALSE, NULL);
    if (!ev) { say("CreateEvent failed: %lu", GetLastError()); return 2; }
    LARGE_INTEGER *qpc = calloc(frames, sizeof *qpc);
    if (!qpc) { say("QPC sample allocation failed"); return 2; }
    for (i = 0; i < frames; i++) {
        if (FAILED(hr = ID3D12CommandAllocator_Reset(al))) { say("allocator Reset 0x%08lx at frame %u", (unsigned long)hr, i); return 2; }
        if (FAILED(hr = ID3D12GraphicsCommandList_Reset(cl, al, NULL))) { say("list Reset 0x%08lx at frame %u", (unsigned long)hr, i); return 2; }
        ID3D12GraphicsCommandList_EndQuery(cl, qh, D3D12_QUERY_TYPE_TIMESTAMP, 2 * i);
        if (use_copy) ID3D12GraphicsCommandList_CopyBufferRegion(cl, dst, 0, src, 0, 65536);
        else {
            static const FLOAT colour[4] = { 0.25f, 0.5f, 0.75f, 1.0f };
            D3D12_CPU_DESCRIPTOR_HANDLE h = ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(rtv_heap);
            ID3D12GraphicsCommandList_ClearRenderTargetView(cl, h, colour, 0, NULL);
        }
        ID3D12GraphicsCommandList_EndQuery(cl, qh, D3D12_QUERY_TYPE_TIMESTAMP, 2 * i + 1);
        ID3D12GraphicsCommandList_ResolveQueryData(cl, qh, D3D12_QUERY_TYPE_TIMESTAMP, 2 * i, 2, rb, (UINT64)2 * i * sizeof(UINT64));
        if (FAILED(hr = ID3D12GraphicsCommandList_Close(cl))) { say("Close 0x%08lx at frame %u", (unsigned long)hr, i); return 2; }
        QueryPerformanceCounter(&qpc[i]);
        ID3D12CommandQueue_ExecuteCommandLists(q, 1, (ID3D12CommandList **)&cl);
        if (FAILED(hr = ID3D12CommandQueue_Signal(q, fence, i + 1))) { say("Signal 0x%08lx at frame %u", (unsigned long)hr, i); return 2; }
        if (FAILED(hr = ID3D12Fence_SetEventOnCompletion(fence, i + 1, ev))) { say("SetEventOnCompletion 0x%08lx at frame %u", (unsigned long)hr, i); return 2; }
        DWORD wait = WaitForSingleObject(ev, 5000);
        if (wait != WAIT_OBJECT_0) { say("ERROR: fence %u wait returned 0x%08lx, no readback attempted", i + 1, (unsigned long)wait); return 2; }
        UINT64 completed = ID3D12Fence_GetCompletedValue(fence);
        if (completed == ~(UINT64)0 || completed < i + 1) { say("ERROR: invalid fence completion %llu, no readback attempted", (unsigned long long)completed); return 2; }
        if (sleep_ms) Sleep(sleep_ms);
    }
    calibration(q, "end", &gpu1, &cpu1);
    /* Calibration is reported but does not decide PASS/FAIL: upstream vkd3d on lavapipe returns 0/0 here
     * while its timestamps are fine (smoke run 2026-10-08), so it is a separate observation. */
    say("calibration over the run: gpu delta %lld ticks, cpu delta %lld QPC ticks%s", (long long)(gpu1 - gpu0), (long long)(cpu1 - cpu0),
        (gpu1 == gpu0 || cpu1 == cpu0) ? "  <-- WARN: GetClockCalibration does not advance (not counted as a timestamp failure)" : "");

    UINT64 *v = NULL; D3D12_RANGE all = { 0, (SIZE_T)(2 * frames) * sizeof(UINT64) };
    if (FAILED(hr = ID3D12Resource_Map(rb, 0, &all, (void **)&v)) || !v) { say("Map readback 0x%08lx", (unsigned long)hr); return 2; }
    say("frame  begin_ticks         end_ticks           in_frame_delta  in_frame_us  frame_to_frame_delta  f2f_us     cpu_f2f_us");
    for (i = 0; i < frames; i++) {
        UINT64 b = v[2 * i], e = v[2 * i + 1];
        long long inframe = (long long)(e - b);
        long long f2f = i ? (long long)(b - v[2 * (i - 1)]) : 0;
        double cpu_us = i ? (double)(qpc[i].QuadPart - qpc[i - 1].QuadPart) * 1e6 / (double)qpf.QuadPart : 0.0;
        const char *fb = flag_of(b), *fe = flag_of(e);
        int bad = *fb || *fe || e < b || (i && f2f <= 0);
        flags += bad;
        say("%5u  0x%016llx  0x%016llx  %14lld  %11.2f  %20lld  %9.2f  %10.2f%s%s%s%s", i,
            (unsigned long long)b, (unsigned long long)e, inframe, freq ? inframe * 1e6 / (double)freq : 0.0,
            f2f, freq ? f2f * 1e6 / (double)freq : 0.0, cpu_us,
            *fb ? fb : "", *fe ? fe : "", e < b ? " <-- end < begin" : "", (i && f2f <= 0) ? " <-- not advancing" : "");
    }
    ID3D12Resource_Unmap(rb, 0, NULL);
    say("RESULT: %s (%u flag(s))", flags ? "FAIL - timestamps are not usable" : "PASS - timestamps advance", flags);
    CloseHandle(ev);
    free(qpc);
    return flags ? 1 : 0;
}
