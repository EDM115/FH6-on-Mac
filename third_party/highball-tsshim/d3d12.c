/* SPDX-License-Identifier: GPL-3.0-only
 * Exported from the October 2026 FH6 experiments, see THIRD_PARTY_NOTICES.md
 */
// tsshim: a d3d12.dll that sits in front of D3DMetal's and serves DirectX 12 timestamp queries.
//
// D3DMetal 3.0 has no timestamp queries: CreateQueryHeap hands back a heap, then EndQuery and
// ResolveQueryData for query type 2 log a refusal and do nothing, so a game that times its
// frames reads garbage and crashes (Guardians of the Galaxy, highball #63). This proxy loads
// the real D3DMetal d3d12.dll (HB_D3D12_REAL, a Windows path), forwards everything, and after
// D3D12CreateDevice patches four vtable slots: the device's CreateQueryHeap answers timestamp
// heaps itself, the command list's EndQuery stamps the CPU clock into that heap, its
// ResolveQueryData copies the stamps into the destination buffer through an upload ring, and
// the queue reports the CPU clock's frequency. Everything else is D3DMetal untouched.
//
// Two more gaps are closed the same way (both found on D3DMetal 4.0b2 with Forza Horizon 6,
// 2026-10-05, see the hooks for the measurements):
// - CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL) with a shader model D3DMetal does not know
//   returns E_INVALIDARG where a current Windows runtime answers with the highest model it
//   supports. The hook asks again with lower values and returns that answer.
// - ResolveSubresource from a multisampled depth texture into a colour texture ends in a Metal
//   assertion that aborts the thread. The hook drops the call (HB_TSSHIM_DEPTHRESOLVE=1 lets it
//   through, for a D3DMetal that has learned to do it).
//
// Build: x86_64-w64-mingw32-gcc -shared -O2 -Wall -static-libgcc -o d3d12.dll d3d12.c d3d12.def
// then stamp the "Wine builtin DLL" marker so WINEDLLPATH_PREPEND picks it up.
#define INITGUID
#include <windows.h>
#include <initguid.h>
#include <d3d12.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static HMODULE real_dll;
static HINSTANCE self;
static int dbg, idle;
// FH6 local diagnostics (not in Highball): call counters, the real frequency, an optional SM clamp.
static volatile LONG n_endq, n_resolve, n_heaps;
static UINT sm_clamp;   // FH6SHIM_SM_CLAMP=0x66 etc.; 0 = off (Highball behaviour)
static int diag;        // FH6SHIM_DIAG=1: only the FH6 lines below, without HB_TSSHIM_DEBUG's per-resource logging
// v2 trial switches, all off by default (= Highball behaviour). See fh6-macos-wine.md items 1, 2 and 5.
static int ts_mode;      // FH6SHIM_TS: fake (default, Highball) | unsupported (A1a) | unsupported-noheap (A1b)
static int prefill;      // FH6SHIM_PREFILL=1: stamp every entry of a new timestamp heap (vkd3d-proton init_query_heap analogue)
static int vrs_off;      // FH6SHIM_VRS_OFF=1: report OPTIONS6 VariableShadingRateTier NOT_SUPPORTED (C1)
static int sampler_pad;  // FH6SHIM_SAMPLER_PAD=1: shader-visible sampler heaps padded to 2048 and pre-filled (vkd3d-proton #3033 analogue)
static int rtas_barrier; // FH6SHIM_RTAS_BARRIER=1: UAV barrier after BuildRaytracingAccelerationStructure (vkd3d-proton #3033 analogue)
static UINT64 qpc_freq;
// Debug lines go to a file: a GUI-subsystem game started by Steam has no stderr handle on the
// Windows side, so fprintf(stderr) vanished (The Last Caretaker, 2026-09-19). C:\highball\logs is
// where Highball keeps the renderers' own logs inside the bottle.
static FILE *dbg_file;
static FILE *dbg_out(void)
{
    if (!dbg_file) {
        char path[MAX_PATH]; snprintf(path, sizeof path, "C:\\highball\\logs\\tsshim-%lu.log", (unsigned long)GetCurrentProcessId());
        dbg_file = fopen(path, "a");
        if (!dbg_file) dbg_file = stderr;
    }
    return dbg_file;
}
static CRITICAL_SECTION log_lock; static int log_lock_ready;
#define LOG(...) do { if (dbg) { if (!log_lock_ready) { InitializeCriticalSection(&log_lock); log_lock_ready = 1; } \
    EnterCriticalSection(&log_lock); FILE *f_ = dbg_out(); SYSTEMTIME st_; GetLocalTime(&st_); \
    fprintf(f_, "%02u:%02u:%02u.%03u tsshim[%lu]: ", st_.wHour, st_.wMinute, st_.wSecond, st_.wMilliseconds, (unsigned long)GetCurrentThreadId()); fprintf(f_, __VA_ARGS__); fputc('\n', f_); fflush(f_); LeaveCriticalSection(&log_lock); } } while (0)
// FH6 diagnostics: same sink as LOG, enabled by HB_TSSHIM_DEBUG or FH6SHIM_DIAG.
#define DLOG(...) do { if (dbg || diag) { if (!log_lock_ready) { InitializeCriticalSection(&log_lock); log_lock_ready = 1; } \
    EnterCriticalSection(&log_lock); FILE *f_ = dbg_out(); SYSTEMTIME st_; GetLocalTime(&st_); \
    fprintf(f_, "%02u:%02u:%02u.%03u tsshim[%lu]: ", st_.wHour, st_.wMinute, st_.wSecond, st_.wMilliseconds, (unsigned long)GetCurrentThreadId()); fprintf(f_, __VA_ARGS__); fputc('\n', f_); fflush(f_); LeaveCriticalSection(&log_lock); } } while (0)

// Wine tells builtin modules apart by name (the file name on Wine 10, the PE's internal export name
// on CrossOver's Wine 11), so a second "d3d12.dll" at another path comes back as this shim. Highball
// lays the real one out beside the shim as apd12.dll, export name patched to match, with its Mach-O
// half as x86_64-unix/apd12.so (InstalledEngine.timestampShimDir), and passes the path in
// HB_D3D12_REAL. The bare-name fallback below is for experiments only.
static HMODULE load_real(void)
{
    WCHAR path[1024];
    DWORD n;
    if (real_dll) return real_dll;
    n = GetEnvironmentVariableW(L"HB_D3D12_REAL", path, 1024);
    if (n && n < 1024) real_dll = LoadLibraryW(path);
    else real_dll = LoadLibraryW(L"d3d12_d3dmetal.dll");
    if (real_dll == self) { fprintf(stderr, "tsshim: the real d3d12.dll resolved to the shim itself\n"); real_dll = NULL; }
    if (!real_dll) fprintf(stderr, "tsshim: cannot load the real D3DMetal d3d12.dll (%ls, error %lu)\n", n ? path : L"d3d12_d3dmetal.dll", GetLastError());
    return real_dll;
}

// Forwarded exports: a jump through a pointer resolved at load, so no signature is guessed.
#define FORWARD(name) \
    static void *p_##name __attribute__((used)); \
    __asm__(".text\n.globl " #name "\n" #name ":\n\tjmp *p_" #name "(%rip)\n");
FORWARD(D3D12CoreCreateLayeredDevice)
FORWARD(D3D12CoreGetLayeredDeviceSize)
FORWARD(D3D12CoreRegisterLayers)
FORWARD(D3D12CreateRootSignatureDeserializer)
FORWARD(D3D12CreateVersionedRootSignatureDeserializer)
FORWARD(D3D12EnableExperimentalFeatures)
FORWARD(D3D12GetDebugInterface)
FORWARD(D3D12SerializeRootSignature)
FORWARD(D3D12SerializeVersionedRootSignature)
FORWARD(GetBehaviorValue)
#define RESOLVE(name) do { p_##name = (void *)GetProcAddress(real_dll, #name); if (!p_##name) LOG("real d3d12.dll lacks %s", #name); } while (0)

// ---- the fake timestamp query heap -------------------------------------------------------

typedef struct FakeHeap {
    const ID3D12QueryHeapVtbl *lpVtbl;
    LONG ref;
    UINT count;
    UINT64 *values;
    ID3D12Device *device;
} FakeHeap;

static const ID3D12QueryHeapVtbl fake_heap_vtbl;
static int is_fake(ID3D12QueryHeap *h) { return h && h->lpVtbl == &fake_heap_vtbl; }

static HRESULT STDMETHODCALLTYPE fh_QueryInterface(ID3D12QueryHeap *iface, REFIID riid, void **out)
{
    if (!out) return E_POINTER;
    if (IsEqualGUID(riid, &IID_IUnknown) || IsEqualGUID(riid, &IID_ID3D12Object) || IsEqualGUID(riid, &IID_ID3D12DeviceChild)
        || IsEqualGUID(riid, &IID_ID3D12Pageable) || IsEqualGUID(riid, &IID_ID3D12QueryHeap)) {
        InterlockedIncrement(&((FakeHeap *)iface)->ref); *out = iface; return S_OK;
    }
    *out = NULL; return E_NOINTERFACE;
}
static ULONG STDMETHODCALLTYPE fh_AddRef(ID3D12QueryHeap *iface) { return InterlockedIncrement(&((FakeHeap *)iface)->ref); }
static ULONG STDMETHODCALLTYPE fh_Release(ID3D12QueryHeap *iface)
{
    FakeHeap *h = (FakeHeap *)iface; ULONG r = InterlockedDecrement(&h->ref);
    if (!r) { h->device->lpVtbl->Release(h->device); free(h->values); free(h); }
    return r;
}
static HRESULT STDMETHODCALLTYPE fh_GetPrivateData(ID3D12QueryHeap *iface, REFGUID guid, UINT *size, void *data) { if (size) *size = 0; return (HRESULT)0x887A0002L; /* DXGI_ERROR_NOT_FOUND */ }
static HRESULT STDMETHODCALLTYPE fh_SetPrivateData(ID3D12QueryHeap *iface, REFGUID guid, UINT size, const void *data) { return S_OK; }
static HRESULT STDMETHODCALLTYPE fh_SetPrivateDataInterface(ID3D12QueryHeap *iface, REFGUID guid, const IUnknown *data) { return S_OK; }
static HRESULT STDMETHODCALLTYPE fh_SetName(ID3D12QueryHeap *iface, LPCWSTR name) { return S_OK; }
static HRESULT STDMETHODCALLTYPE fh_GetDevice(ID3D12QueryHeap *iface, REFIID riid, void **out)
{
    FakeHeap *h = (FakeHeap *)iface; return h->device->lpVtbl->QueryInterface(h->device, riid, out);
}
static const ID3D12QueryHeapVtbl fake_heap_vtbl = {
    fh_QueryInterface, fh_AddRef, fh_Release, fh_GetPrivateData, fh_SetPrivateData, fh_SetPrivateDataInterface, fh_SetName, fh_GetDevice
};

// ---- the upload ring the stamps travel through ---------------------------------------------

#define RING_BYTES (8u << 20)
typedef struct Ring { ID3D12Device *device; ID3D12Resource *buffer; UINT8 *cpu; volatile LONG64 cursor; } Ring;
static Ring rings[4];
static CRITICAL_SECTION ring_lock;

static Ring *ring_for(ID3D12Device *dev)
{
    Ring *r = NULL; int i;
    EnterCriticalSection(&ring_lock);
    for (i = 0; i < 4 && !r; i++) if (rings[i].device == dev) r = &rings[i];
    for (i = 0; i < 4 && !r; i++) if (!rings[i].device) {
        D3D12_HEAP_PROPERTIES hp = { D3D12_HEAP_TYPE_UPLOAD, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 0, 0 };
        D3D12_RESOURCE_DESC rd = { D3D12_RESOURCE_DIMENSION_BUFFER, 0, RING_BYTES, 1, 1, 1, DXGI_FORMAT_UNKNOWN, { 1, 0 }, D3D12_TEXTURE_LAYOUT_ROW_MAJOR, D3D12_RESOURCE_FLAG_NONE };
        ID3D12Resource *buf = NULL; void *cpu = NULL;
        if (SUCCEEDED(dev->lpVtbl->CreateCommittedResource(dev, &hp, D3D12_HEAP_FLAG_NONE, &rd, D3D12_RESOURCE_STATE_GENERIC_READ, NULL, &IID_ID3D12Resource, (void **)&buf))
            && SUCCEEDED(buf->lpVtbl->Map(buf, 0, NULL, &cpu))) {
            rings[i].device = dev; dev->lpVtbl->AddRef(dev);
            rings[i].buffer = buf; rings[i].cpu = cpu; rings[i].cursor = 0; r = &rings[i];
            LOG("upload ring of %u bytes created", RING_BYTES);
        } else { if (buf) buf->lpVtbl->Release(buf); LOG("upload ring creation failed"); }
        break;
    }
    LeaveCriticalSection(&ring_lock);
    return r;
}

static UINT64 now_ticks(void) { LARGE_INTEGER c; QueryPerformanceCounter(&c); return (UINT64)c.QuadPart; }

// ---- the hooks -----------------------------------------------------------------------------

typedef HRESULT (STDMETHODCALLTYPE *PFN_CreateQueryHeap)(ID3D12Device *, const D3D12_QUERY_HEAP_DESC *, REFIID, void **);
typedef void (STDMETHODCALLTYPE *PFN_Query)(ID3D12GraphicsCommandList *, ID3D12QueryHeap *, D3D12_QUERY_TYPE, UINT);
typedef void (STDMETHODCALLTYPE *PFN_ResolveQueryData)(ID3D12GraphicsCommandList *, ID3D12QueryHeap *, D3D12_QUERY_TYPE, UINT, UINT, ID3D12Resource *, UINT64);
typedef HRESULT (STDMETHODCALLTYPE *PFN_GetTimestampFrequency)(ID3D12CommandQueue *, UINT64 *);
typedef HRESULT (STDMETHODCALLTYPE *PFN_GetClockCalibration)(ID3D12CommandQueue *, UINT64 *, UINT64 *);
static PFN_CreateQueryHeap real_CreateQueryHeap;
static PFN_Query real_EndQuery, real_BeginQuery;
static PFN_ResolveQueryData real_ResolveQueryData;
static PFN_GetTimestampFrequency real_GetTimestampFrequency;
static PFN_GetClockCalibration real_GetClockCalibration;

static HRESULT STDMETHODCALLTYPE hook_CreateQueryHeap(ID3D12Device *dev, const D3D12_QUERY_HEAP_DESC *desc, REFIID riid, void **out)
{
    FakeHeap *h; HRESULT hr;
    if (!desc || (desc->Type != D3D12_QUERY_HEAP_TYPE_TIMESTAMP && desc->Type != D3D12_QUERY_HEAP_TYPE_COPY_QUEUE_TIMESTAMP))
        return real_CreateQueryHeap(dev, desc, riid, out);
    if (ts_mode == 1) return real_CreateQueryHeap(dev, desc, riid, out);   // A1a: D3DMetal's own heap; its EndQuery/Resolve refuse type 2
    if (ts_mode == 2) { DLOG("A1b: timestamp query heap of %u entries refused with E_NOTIMPL", desc->Count); return E_NOTIMPL; }
    if (!out) return S_FALSE;
    h = calloc(1, sizeof *h); if (!h) return E_OUTOFMEMORY;
    h->lpVtbl = &fake_heap_vtbl; h->ref = 1; h->count = desc->Count;
    h->values = calloc(desc->Count ? desc->Count : 1, sizeof(UINT64));
    if (h->values && prefill) { UINT i_; UINT64 t_ = now_ticks(); for (i_ = 0; i_ < desc->Count; i_++) h->values[i_] = t_; }
    h->device = dev; dev->lpVtbl->AddRef(dev);
    hr = fh_QueryInterface((ID3D12QueryHeap *)h, riid, out);
    fh_Release((ID3D12QueryHeap *)h);
    InterlockedIncrement(&n_heaps);
    DLOG("timestamp query heap of %u entries served by the shim (hr 0x%08lx, heap #%ld)", desc->Count, (unsigned long)hr, (long)n_heaps);
    return hr;
}

static void STDMETHODCALLTYPE hook_EndQuery(ID3D12GraphicsCommandList *list, ID3D12QueryHeap *heap, D3D12_QUERY_TYPE type, UINT index)
{
    FakeHeap *h;
    if (!is_fake(heap)) { real_EndQuery(list, heap, type, index); return; }
    h = (FakeHeap *)heap; if (index < h->count) h->values[index] = now_ticks();
    { LONG n = InterlockedIncrement(&n_endq);
      if (n <= 5 || !(n % 5000)) DLOG("EndQuery #%ld type=%d index=%u value=0x%016llx", (long)n, (int)type, index,
          (unsigned long long)(index < h->count ? h->values[index] : 0)); }
}

static void STDMETHODCALLTYPE hook_BeginQuery(ID3D12GraphicsCommandList *list, ID3D12QueryHeap *heap, D3D12_QUERY_TYPE type, UINT index)
{
    if (!is_fake(heap)) real_BeginQuery(list, heap, type, index);
}

static void STDMETHODCALLTYPE hook_ResolveQueryData(ID3D12GraphicsCommandList *list, ID3D12QueryHeap *heap, D3D12_QUERY_TYPE type,
                                                    UINT start, UINT count, ID3D12Resource *dst, UINT64 offset)
{
    FakeHeap *h; Ring *r; UINT64 bytes, off; LONG64 pos; int tries;
    if (!is_fake(heap)) { real_ResolveQueryData(list, heap, type, start, count, dst, offset); return; }
    h = (FakeHeap *)heap; r = ring_for(h->device);
    if (!r || !dst || !count || start >= h->count) return;
    if (start + count > h->count) count = h->count - start;
    bytes = (UINT64)count * 8; if (bytes > RING_BYTES / 2) return;
    for (tries = 0; tries < 2; tries++) {
        pos = InterlockedExchangeAdd64(&r->cursor, (LONG64)bytes); off = (UINT64)pos % RING_BYTES;
        if (off + bytes <= RING_BYTES) break;
        InterlockedExchangeAdd64(&r->cursor, (LONG64)(RING_BYTES - off));   // skip the tail, start over
    }
    if (tries == 2) return;
    memcpy(r->cpu + off, h->values + start, (size_t)bytes);
    { LONG n = InterlockedIncrement(&n_resolve);
      if (n <= 5 || !(n % 5000)) DLOG("ResolveQueryData #%ld start=%u count=%u first=0x%016llx last=0x%016llx span_ticks=%lld", (long)n, start, count,
          (unsigned long long)h->values[start], (unsigned long long)h->values[start + count - 1],
          (long long)(h->values[start + count - 1] - h->values[start])); }
    list->lpVtbl->CopyBufferRegion(list, dst, offset, r->buffer, off, bytes);
}

// A0: log what the real d3d12 answers, once per queue (up to 8 queues), with the queue type.
static void *seen_queues[8];
static int first_sight(void *q)
{
    int i;
    for (i = 0; i < 8; i++) {
        void *cur = InterlockedCompareExchangePointer(&seen_queues[i], q, NULL);
        if (!cur) return 1;          // newly recorded
        if (cur == q) return 0;      // already logged
    }
    return 0;
}
static void log_queue(ID3D12CommandQueue *q)
{
    D3D12_COMMAND_QUEUE_DESC qd; UINT64 rf = 0, g = 0, c = 0; HRESULT fhr = E_FAIL, chr = E_FAIL;
    if (!(dbg || diag) || !first_sight(q)) return;
    memset(&qd, 0, sizeof qd); q->lpVtbl->GetDesc(q, &qd);
    if (real_GetTimestampFrequency) fhr = real_GetTimestampFrequency(q, &rf);
    if (real_GetClockCalibration) chr = real_GetClockCalibration(q, &g, &c);
    DLOG("queue %p type %d: real GetTimestampFrequency hr 0x%08lx -> %llu Hz; real GetClockCalibration hr 0x%08lx -> gpu %llu cpu %llu; shim mode %s",
         (void *)q, (int)qd.Type, (unsigned long)fhr, (unsigned long long)rf, (unsigned long)chr, (unsigned long long)g, (unsigned long long)c,
         ts_mode == 0 ? "fake" : ts_mode == 1 ? "unsupported" : "unsupported-noheap");
}
static HRESULT STDMETHODCALLTYPE hook_GetTimestampFrequency(ID3D12CommandQueue *q, UINT64 *f)
{
    log_queue(q);
    if (ts_mode) return E_FAIL;                          // A1: vkd3d-proton without timestamp bits; *f untouched
    if (!f) return E_INVALIDARG;
    *f = qpc_freq; return S_OK;                          // one value for every queue type
}
static HRESULT STDMETHODCALLTYPE hook_GetClockCalibration(ID3D12CommandQueue *q, UINT64 *gpu, UINT64 *cpu)
{
    UINT64 t;
    log_queue(q);
    if (ts_mode) return E_FAIL;
    t = now_ticks(); if (gpu) *gpu = t; if (cpu) *cpu = t; return S_OK;
}

// ---- descriptor heaps and ray tracing (vkd3d-proton PR #3033 analogues, env-gated) -----------

static HRESULT (STDMETHODCALLTYPE *real_CreateDescriptorHeap)(ID3D12Device *, const D3D12_DESCRIPTOR_HEAP_DESC *, REFIID, void **);
static HRESULT STDMETHODCALLTYPE hook_CreateDescriptorHeap(ID3D12Device *dev, const D3D12_DESCRIPTOR_HEAP_DESC *desc, REFIID riid, void **out)
{
    D3D12_DESCRIPTOR_HEAP_DESC d; HRESULT hr; ID3D12DescriptorHeap *heap = NULL;
    if (!sampler_pad || !desc || !out || desc->Type != D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER || !(desc->Flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE))
        return real_CreateDescriptorHeap(dev, desc, riid, out);
    d = *desc; if (d.NumDescriptors < 2048) d.NumDescriptors = 2048;   // D3D12's shader-visible sampler heap limit
    hr = real_CreateDescriptorHeap(dev, &d, riid, out);
    if (SUCCEEDED(hr) && *out &&
        SUCCEEDED(((IUnknown *)*out)->lpVtbl->QueryInterface((IUnknown *)*out, &IID_ID3D12DescriptorHeap, (void **)&heap)) && heap) {
        // No null samplers exist; a slot the game never writes (or indexes past its own count) now holds a valid sampler.
        D3D12_SAMPLER_DESC smp = { D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP,
                                   D3D12_TEXTURE_ADDRESS_MODE_WRAP, 0.0f, 1, D3D12_COMPARISON_FUNC_NEVER, {0.0f, 0.0f, 0.0f, 0.0f}, 0.0f, D3D12_FLOAT32_MAX };
        UINT inc = dev->lpVtbl->GetDescriptorHandleIncrementSize(dev, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER), i;
        D3D12_CPU_DESCRIPTOR_HANDLE start;
        heap->lpVtbl->GetCPUDescriptorHandleForHeapStart(heap, &start);
        for (i = 0; i < d.NumDescriptors; i++) {
            D3D12_CPU_DESCRIPTOR_HANDLE h = { start.ptr + (SIZE_T)i * inc };
            dev->lpVtbl->CreateSampler(dev, &smp, h);
        }
        heap->lpVtbl->Release(heap);
    }
    DLOG("shader-visible sampler heap: asked %u, created %u, pre-filled with a linear/wrap sampler (hr 0x%08lx)", desc->NumDescriptors, d.NumDescriptors, (unsigned long)hr);
    return hr;
}

typedef void (STDMETHODCALLTYPE *PFN_BuildRTAS)(ID3D12GraphicsCommandList4 *, const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC *,
                                               UINT, const D3D12_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO_DESC *);
static PFN_BuildRTAS real_BuildRTAS;
static void STDMETHODCALLTYPE hook_BuildRTAS(ID3D12GraphicsCommandList4 *list, const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC *desc,
                                             UINT n, const D3D12_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO_DESC *post)
{
    D3D12_RESOURCE_BARRIER b; static LONG count;
    real_BuildRTAS(list, desc, n, post);
    memset(&b, 0, sizeof b); b.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV; b.UAV.pResource = NULL;   // global UAV barrier
    list->lpVtbl->ResourceBarrier(list, 1, &b);
    { LONG k = InterlockedIncrement(&count); if (k == 1 || !(k % 1000)) DLOG("UAV barrier #%ld added after BuildRaytracingAccelerationStructure", (long)k); }
}

// ---- patching ------------------------------------------------------------------------------

static void patch_slot(void **slot, void *hook, void **saved)
{
    DWORD old;
    if (*slot == hook) return;
    // D3DMetal's vtables live in its framework's __DATA segment on the Mach-O side: writable, but
    // outside Wine's view of memory, so VirtualProtect refuses the address. Try it for the PE case,
    // then write regardless (verified on D3DMetal 3.0 with Wine 10 and 11, 2026-09-07).
    if (!VirtualProtect(slot, sizeof(void *), PAGE_READWRITE, &old)) old = 0;
    if (!*saved) *saved = *slot;
    *slot = hook;
    if (old) VirtualProtect(slot, sizeof(void *), old, &old);
}

// Diagnostic only (HB_TSSHIM_DEBUG): log each resource the game creates, so a Metal assertion
// such as "MTLTextureDescriptor has invalid pixelFormat (0)" (The Last Caretaker, 2026-09-19)
// can be tied to the DXGI format D3DMetal failed to map. Forwarded untouched otherwise.
static HRESULT (STDMETHODCALLTYPE *real_CreateCommittedResource)(ID3D12Device *, const D3D12_HEAP_PROPERTIES *, D3D12_HEAP_FLAGS, const D3D12_RESOURCE_DESC *, D3D12_RESOURCE_STATES, const D3D12_CLEAR_VALUE *, REFIID, void **);
static HRESULT (STDMETHODCALLTYPE *real_CreatePlacedResource)(ID3D12Device *, ID3D12Heap *, UINT64, const D3D12_RESOURCE_DESC *, D3D12_RESOURCE_STATES, const D3D12_CLEAR_VALUE *, REFIID, void **);
static void log_resource(const char *what, const D3D12_RESOURCE_DESC *d, HRESULT hr)
{
    if (!d || d->Dimension == D3D12_RESOURCE_DIMENSION_BUFFER) return;
    LOG("%s dim=%d format=%u %llux%u depth/array=%u mips=%u samples=%u flags=0x%x hr=0x%lx", what, (int)d->Dimension, (unsigned)d->Format,
        (unsigned long long)d->Width, (unsigned)d->Height, (unsigned)d->DepthOrArraySize, (unsigned)d->MipLevels, (unsigned)d->SampleDesc.Count, (unsigned)d->Flags, (unsigned long)hr);
}
static HRESULT STDMETHODCALLTYPE hook_CreateCommittedResource(ID3D12Device *dev, const D3D12_HEAP_PROPERTIES *hp, D3D12_HEAP_FLAGS hf, const D3D12_RESOURCE_DESC *d, D3D12_RESOURCE_STATES st, const D3D12_CLEAR_VALUE *cv, REFIID riid, void **out)
{
    if (dbg) { log_resource("CreateCommittedResource begin", d, 0); }
    HRESULT hr = real_CreateCommittedResource(dev, hp, hf, d, st, cv, riid, out);
    if (dbg) { log_resource("CreateCommittedResource done", d, hr); if (d && d->Dimension != D3D12_RESOURCE_DIMENSION_BUFFER) LOG("  -> res=%p", out ? *out : NULL); }
    return hr;
}
static HRESULT STDMETHODCALLTYPE hook_CreatePlacedResource(ID3D12Device *dev, ID3D12Heap *heap, UINT64 off, const D3D12_RESOURCE_DESC *d, D3D12_RESOURCE_STATES st, const D3D12_CLEAR_VALUE *cv, REFIID riid, void **out)
{
    if (dbg) { log_resource("CreatePlacedResource begin", d, 0); }
    HRESULT hr = real_CreatePlacedResource(dev, heap, off, d, st, cv, riid, out);
    if (dbg) { log_resource("CreatePlacedResource done", d, hr); if (d && d->Dimension != D3D12_RESOURCE_DIMENSION_BUFFER) LOG("  -> res=%p", out ? *out : NULL); }
    return hr;
}
static HRESULT (STDMETHODCALLTYPE *real_CreateReservedResource)(ID3D12Device *, const D3D12_RESOURCE_DESC *, D3D12_RESOURCE_STATES, const D3D12_CLEAR_VALUE *, REFIID, void **);
static HRESULT STDMETHODCALLTYPE hook_CreateReservedResource(ID3D12Device *dev, const D3D12_RESOURCE_DESC *d, D3D12_RESOURCE_STATES st, const D3D12_CLEAR_VALUE *cv, REFIID riid, void **out)
{
    if (dbg) { log_resource("CreateReservedResource begin", d, 0); }
    HRESULT hr = real_CreateReservedResource(dev, d, st, cv, riid, out);
    if (dbg) { log_resource("CreateReservedResource done", d, hr); if (d && d->Dimension != D3D12_RESOURCE_DIMENSION_BUFFER) LOG("  -> res=%p", out ? *out : NULL); }
    return hr;
}
// Views: a cast over a typeless texture is where a format D3DMetal cannot map would surface.
static void (STDMETHODCALLTYPE *real_CreateShaderResourceView)(ID3D12Device *, ID3D12Resource *, const D3D12_SHADER_RESOURCE_VIEW_DESC *, D3D12_CPU_DESCRIPTOR_HANDLE);
static void (STDMETHODCALLTYPE *real_CreateUnorderedAccessView)(ID3D12Device *, ID3D12Resource *, ID3D12Resource *, const D3D12_UNORDERED_ACCESS_VIEW_DESC *, D3D12_CPU_DESCRIPTOR_HANDLE);
static void (STDMETHODCALLTYPE *real_CreateRenderTargetView)(ID3D12Device *, ID3D12Resource *, const D3D12_RENDER_TARGET_VIEW_DESC *, D3D12_CPU_DESCRIPTOR_HANDLE);
static void (STDMETHODCALLTYPE *real_CreateDepthStencilView)(ID3D12Device *, ID3D12Resource *, const D3D12_DEPTH_STENCIL_VIEW_DESC *, D3D12_CPU_DESCRIPTOR_HANDLE);
static void STDMETHODCALLTYPE hook_CreateShaderResourceView(ID3D12Device *dev, ID3D12Resource *res, const D3D12_SHADER_RESOURCE_VIEW_DESC *d, D3D12_CPU_DESCRIPTOR_HANDLE h)
{
    if (dbg && d) LOG("SRV begin res=%p format=%u viewdim=%d", res, (unsigned)d->Format, (int)d->ViewDimension);
    real_CreateShaderResourceView(dev, res, d, h);
    if (dbg && d) LOG("SRV done res=%p", res);
}
static void STDMETHODCALLTYPE hook_CreateUnorderedAccessView(ID3D12Device *dev, ID3D12Resource *res, ID3D12Resource *counter, const D3D12_UNORDERED_ACCESS_VIEW_DESC *d, D3D12_CPU_DESCRIPTOR_HANDLE h)
{
    if (dbg && d) LOG("UAV begin res=%p format=%u viewdim=%d", res, (unsigned)d->Format, (int)d->ViewDimension);
    real_CreateUnorderedAccessView(dev, res, counter, d, h);
    if (dbg && d) LOG("UAV done res=%p", res);
}
static void STDMETHODCALLTYPE hook_CreateRenderTargetView(ID3D12Device *dev, ID3D12Resource *res, const D3D12_RENDER_TARGET_VIEW_DESC *d, D3D12_CPU_DESCRIPTOR_HANDLE h)
{
    if (dbg && d) LOG("RTV begin res=%p format=%u viewdim=%d", res, (unsigned)d->Format, (int)d->ViewDimension);
    real_CreateRenderTargetView(dev, res, d, h);
    if (dbg && d) LOG("RTV done res=%p", res);
}
static void STDMETHODCALLTYPE hook_CreateDepthStencilView(ID3D12Device *dev, ID3D12Resource *res, const D3D12_DEPTH_STENCIL_VIEW_DESC *d, D3D12_CPU_DESCRIPTOR_HANDLE h)
{
    if (dbg && d) LOG("DSV begin res=%p format=%u viewdim=%d", res, (unsigned)d->Format, (int)d->ViewDimension);
    real_CreateDepthStencilView(dev, res, d, h);
    if (dbg && d) LOG("DSV done res=%p", res);
}

// ---- feature queries -------------------------------------------------------------------------

// A program asks for the highest shader model it understands and the runtime answers with the
// highest one it supports at or below that. D3DMetal only accepts the values it knows (up to 0x68,
// SM 6.8, on 4.0b2, answering 0x66) and returns E_INVALIDARG for a newer one, which a program built
// against a newer SDK takes as "no usable shader model": Forza Horizon 6's 461.691 build asks with
// 0x6a and stopped at "Your graphics card is not supported (FH206)" (M4, macOS 27.0, D3DMetal 4.0b2).
// The Windows runtime the game ships knows its own enum, so on Windows the question never fails.
static HRESULT (STDMETHODCALLTYPE *real_CheckFeatureSupport)(ID3D12Device *, D3D12_FEATURE, void *, UINT);
static HRESULT STDMETHODCALLTYPE hook_CheckFeatureSupport(ID3D12Device *dev, D3D12_FEATURE feature, void *data, UINT size)
{
    HRESULT hr;
    if (feature == D3D12_FEATURE_SHADER_MODEL && sm_clamp && data && size >= sizeof(D3D12_FEATURE_DATA_SHADER_MODEL)) {
        D3D12_FEATURE_DATA_SHADER_MODEL *sm = data; UINT asked = sm->HighestShaderModel;
        if (asked > sm_clamp) sm->HighestShaderModel = sm_clamp;
        hr = real_CheckFeatureSupport(dev, feature, data, size);
        DLOG("shader model query for 0x%x clamped to 0x%x, the real d3d12 answered 0x%x (hr 0x%08lx)", asked, sm_clamp, (unsigned)sm->HighestShaderModel, (unsigned long)hr);
        if (SUCCEEDED(hr)) return hr;
        sm->HighestShaderModel = asked;
    }
    hr = real_CheckFeatureSupport(dev, feature, data, size);
    if (feature == D3D12_FEATURE_SHADER_MODEL && (dbg || diag) && data && size >= sizeof(D3D12_FEATURE_DATA_SHADER_MODEL))
        DLOG("shader model query: the real d3d12 answered 0x%x (hr 0x%08lx)", (unsigned)((D3D12_FEATURE_DATA_SHADER_MODEL *)data)->HighestShaderModel, (unsigned long)hr);
    if (feature == D3D12_FEATURE_SHADER_MODEL && hr == E_INVALIDARG && data && size >= sizeof(D3D12_FEATURE_DATA_SHADER_MODEL)) {
        D3D12_FEATURE_DATA_SHADER_MODEL *sm = data;
        UINT asked = sm->HighestShaderModel, v;
        for (v = asked - 1; v >= D3D_SHADER_MODEL_6_0 && v < asked; v--) {
            sm->HighestShaderModel = v;
            if (SUCCEEDED(hr = real_CheckFeatureSupport(dev, feature, data, size))) break;
        }
        if (FAILED(hr)) sm->HighestShaderModel = asked;
        LOG("shader model query for 0x%x answered as 0x%x (hr 0x%08lx)", asked, (unsigned)sm->HighestShaderModel, (unsigned long)hr);
    } else if (dbg) LOG("CheckFeatureSupport feature=%d size=%u hr=0x%08lx", (int)feature, size, (unsigned long)hr);
    if (SUCCEEDED(hr) && data && feature == D3D12_FEATURE_D3D12_OPTIONS3 && size >= sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS3)) {
        D3D12_FEATURE_DATA_D3D12_OPTIONS3 *o = data; static LONG once;
        if (!InterlockedExchange(&once, 1)) DLOG("OPTIONS3: real CopyQueueTimestampQueriesSupported %d", o->CopyQueueTimestampQueriesSupported);
        if (ts_mode) o->CopyQueueTimestampQueriesSupported = FALSE;
    }
    if (SUCCEEDED(hr) && data && feature == D3D12_FEATURE_D3D12_OPTIONS6 && size >= sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS6)) {
        D3D12_FEATURE_DATA_D3D12_OPTIONS6 *o = data; static LONG once;
        if (!InterlockedExchange(&once, 1))
            DLOG("OPTIONS6: real VariableShadingRateTier %d, AdditionalShadingRates %d, PerPrimitiveWithViewportIndexing %d, TileSize %u%s",
                 (int)o->VariableShadingRateTier, o->AdditionalShadingRatesSupported, o->PerPrimitiveShadingRateSupportedWithViewportIndexing,
                 o->ShadingRateImageTileSize, vrs_off ? "; reporting NOT_SUPPORTED" : "");
        if (vrs_off) {
            o->VariableShadingRateTier = D3D12_VARIABLE_SHADING_RATE_TIER_NOT_SUPPORTED;
            o->AdditionalShadingRatesSupported = FALSE;
            o->PerPrimitiveShadingRateSupportedWithViewportIndexing = FALSE;
            o->ShadingRateImageTileSize = 0;
        }
    }
    return hr;
}

// ---- depth resolve ---------------------------------------------------------------------------

// ResolveSubresource(colour texture, multisampled depth texture, R32_FLOAT): Windows drivers take
// it. D3DMetal 4.0b2 builds a Metal texture view of the source in the destination's format, and
// Metal has no cast between depth and colour formats:
//   _mtlValidateArgumentsForTextureViewOnDevice: failed assertion `Texture Creation
//   source texture pixelFormat (MTLPixelFormatDepth32Float) not compatible with texture view
//   pixelFormat (MTLPixelFormatR32Float)'
// The assertion aborts D3DMetal's thread with the game's locks held, so the game stands still for
// good. Forza Horizon 6 issues the call every frame in its Character Select scene (960x544, 4
// samples) and froze there on every load; with the call dropped the scene draws and the game goes
// on (820 calls dropped in 36 s, M4, macOS 27.0). The destination keeps whatever it held, so an
// effect that reads it may be off; nothing visible in that scene.
static int pass_depth_resolve;
static void (STDMETHODCALLTYPE *real_ResolveSubresource)(ID3D12GraphicsCommandList *, ID3D12Resource *, UINT, ID3D12Resource *, UINT, DXGI_FORMAT);
static int is_depth_texture(ID3D12Resource *res, D3D12_RESOURCE_DESC *d)
{
    memset(d, 0, sizeof *d);
    if (!res) return 0;
    res->lpVtbl->GetDesc(res, d);
    return d->Dimension != D3D12_RESOURCE_DIMENSION_BUFFER && (d->Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) != 0;
}
static void STDMETHODCALLTYPE hook_ResolveSubresource(ID3D12GraphicsCommandList *list, ID3D12Resource *dst, UINT dst_sub, ID3D12Resource *src, UINT src_sub, DXGI_FORMAT format)
{
    D3D12_RESOURCE_DESC dd, sd;
    if (!pass_depth_resolve && is_depth_texture(src, &sd) && sd.SampleDesc.Count > 1 && !is_depth_texture(dst, &dd)) {
        static LONG told;
        if (!InterlockedExchange(&told, 1))
            LOG("multisampled depth resolve into a colour texture dropped (source format %u, %u samples, destination format %u %llux%u, format %u)",
                (unsigned)sd.Format, (unsigned)sd.SampleDesc.Count, (unsigned)dd.Format, (unsigned long long)dd.Width, (unsigned)dd.Height, (unsigned)format);
        return;
    }
    real_ResolveSubresource(list, dst, dst_sub, src, src_sub, format);
}

// Input-layout backport from Highball PR 403, head 4cb329372d30572d29c959655999677764f7983b
// ---- input layouts ---------------------------------------------------------------------------

// D3D12_APPEND_ALIGNED_ELEMENT puts an element directly after the previous element of the same input
// slot, in declaration order, aligned to the smaller of 4 bytes and its own size. D3DMetal (4.0b2 and
// 4.0b3) places such elements elsewhere. Forza Horizon 6 draws its livery vinyls with a slot of nine
// appended elements (NORMAL R16G16_SNORM, TEXCOORD0-3 R16G16_UNORM, TANGENT0-2 R10G10B10A2_UNORM,
// COLOR0 R8G8B8A8_UNORM, stride 36) of which the vertex shader reads TEXCOORD0 and COLOR0: the vinyl
// meshes carry COLOR alpha 255 at every vertex, yet the shapes came out as gradients from 0 to 1 across
// each quad, the finished livery had alpha 0.25 to 0.7 inside solid shapes, and a liveried car showed
// mostly its base paint; the vinyl picker's shape thumbnails were gradients too. Other meshes read
// shifted normals and tangents (blotchy, faceted reflections on factory paint) and the trees' wind
// data. With the offsets written out the livery reaches alpha 1 and the car, the thumbnails, the
// reflections and the trees draw right (M4 Max, macOS 27.0.1, D3DMetal 4.0b3). Equivalent by D3D12's
// rules, so it is applied to every pipeline; HB_TSSHIM_APPEND_OFFSETS=0 turns it off.
static int keep_append_offsets;
static UINT vertex_format_size(DXGI_FORMAT f)
{
    if (f >= DXGI_FORMAT_R32G32B32A32_TYPELESS && f <= DXGI_FORMAT_R32G32B32A32_SINT) return 16;
    if (f >= DXGI_FORMAT_R32G32B32_TYPELESS && f <= DXGI_FORMAT_R32G32B32_SINT) return 12;
    if (f >= DXGI_FORMAT_R16G16B16A16_TYPELESS && f <= DXGI_FORMAT_X32_TYPELESS_G8X24_UINT) return 8;
    if (f >= DXGI_FORMAT_R10G10B10A2_TYPELESS && f <= DXGI_FORMAT_X24_TYPELESS_G8_UINT) return 4;
    if (f >= DXGI_FORMAT_R8G8_TYPELESS && f <= DXGI_FORMAT_R16_SINT) return 2;
    if (f >= DXGI_FORMAT_R8_TYPELESS && f <= DXGI_FORMAT_A8_UNORM) return 1;
    if (f == DXGI_FORMAT_B5G6R5_UNORM || f == DXGI_FORMAT_B5G5R5A1_UNORM || f == DXGI_FORMAT_B4G4R4A4_UNORM) return 2;
    if (f >= DXGI_FORMAT_B8G8R8A8_UNORM && f <= DXGI_FORMAT_B8G8R8X8_UNORM_SRGB) return 4;
    return 0;
}
// Writes the layout with every appended offset resolved into out (room for in->NumElements); returns
// FALSE when nothing was appended or an element's format is one this table does not size.
static BOOL resolve_append_offsets(const D3D12_INPUT_LAYOUT_DESC *in, D3D12_INPUT_ELEMENT_DESC *out)
{
    UINT end[D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT] = { 0 }, i; BOOL appended = FALSE;
    if (keep_append_offsets || !in || !in->pInputElementDescs || !in->NumElements
        || in->NumElements > D3D12_IA_VERTEX_INPUT_STRUCTURE_ELEMENT_COUNT) return FALSE;
    for (i = 0; i < in->NumElements; i++) {
        D3D12_INPUT_ELEMENT_DESC e = in->pInputElementDescs[i]; UINT size = vertex_format_size(e.Format);
        if (!size || e.InputSlot >= D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT) return FALSE;
        if (e.AlignedByteOffset == D3D12_APPEND_ALIGNED_ELEMENT) {
            UINT align = size < 4 ? size : 4;
            e.AlignedByteOffset = (end[e.InputSlot] + align - 1) / align * align;
            appended = TRUE;
        }
        end[e.InputSlot] = e.AlignedByteOffset + size;
        out[i] = e;
    }
    if (appended) {
        static LONG told;
        if (!InterlockedExchange(&told, 1)) DLOG("input layouts with appended elements are given explicit offsets");
    }
    return appended;
}

// A pipeline state stream is a run of subobjects, each a D3D12_PIPELINE_STATE_SUBOBJECT_TYPE followed by
// its description at that description's alignment, the pair padded to pointer size. Only the input layout
// is rewritten; a stream with a subobject type this table does not know is passed through untouched.
static SIZE_T stream_inner_size(UINT type, SIZE_T *inner_align)
{
    static const struct { UINT size, align; } t[] = {
        { 8, 8 },      // ROOT_SIGNATURE
        { 16, 8 }, { 16, 8 }, { 16, 8 }, { 16, 8 }, { 16, 8 }, { 16, 8 },   // VS PS DS HS GS CS
        { 32, 8 },     // STREAM_OUTPUT
        { 328, 4 },    // BLEND
        { 4, 4 },      // SAMPLE_MASK
        { 44, 4 },     // RASTERIZER
        { 52, 4 },     // DEPTH_STENCIL
        { 16, 8 },     // INPUT_LAYOUT
        { 4, 4 },      // IB_STRIP_CUT_VALUE
        { 4, 4 },      // PRIMITIVE_TOPOLOGY
        { 36, 4 },     // RENDER_TARGET_FORMATS
        { 4, 4 },      // DEPTH_STENCIL_FORMAT
        { 8, 4 },      // SAMPLE_DESC
        { 4, 4 },      // NODE_MASK
        { 16, 8 },     // CACHED_PSO
        { 4, 4 },      // FLAGS
        { 56, 4 },     // DEPTH_STENCIL1
        { 24, 8 },     // VIEW_INSTANCING
        { 0, 0 },      // 23 unused
        { 16, 8 }, { 16, 8 },   // AS MS
        { 60, 4 },     // DEPTH_STENCIL2
        { 44, 4 },     // RASTERIZER1
        { 40, 4 },     // RASTERIZER2
    };
    if (type >= sizeof t / sizeof *t || !t[type].size) return 0;
    *inner_align = t[type].align;
    return t[type].size;
}
static BOOL rewrite_stream(const D3D12_PIPELINE_STATE_STREAM_DESC *in, D3D12_PIPELINE_STATE_STREAM_DESC *out, void **copy, D3D12_INPUT_ELEMENT_DESC *elems)
{
    const UINT8 *s; SIZE_T o = 0, inner_at = 0;
    if (keep_append_offsets || !in || !in->pPipelineStateSubobjectStream || !in->SizeInBytes) return FALSE;
    s = in->pPipelineStateSubobjectStream;
    while (o + sizeof(UINT) <= in->SizeInBytes) {
        UINT type; SIZE_T align, size, at;
        memcpy(&type, s + o, sizeof type);
        if (!(size = stream_inner_size(type, &align))) return FALSE;
        at = (o + sizeof(UINT) + align - 1) & ~(align - 1);
        if (at + size > in->SizeInBytes) return FALSE;
        if (type == D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_INPUT_LAYOUT) inner_at = at;
        o = (at + size + sizeof(void *) - 1) & ~(sizeof(void *) - 1);
    }
    if (!inner_at) return FALSE;
    {
        D3D12_INPUT_LAYOUT_DESC layout; UINT8 *buf;
        memcpy(&layout, s + inner_at, sizeof layout);
        if (!resolve_append_offsets(&layout, elems) || !(buf = malloc(in->SizeInBytes))) return FALSE;
        memcpy(buf, s, in->SizeInBytes);
        layout.pInputElementDescs = elems;
        memcpy(buf + inner_at, &layout, sizeof layout);
        out->SizeInBytes = in->SizeInBytes; out->pPipelineStateSubobjectStream = buf; *copy = buf;
    }
    return TRUE;
}

// With HB_TSSHIM_DEBUG every graphics pipeline is logged under the number D3DMetal gives it (one counter
// for graphics and compute pipelines, counted as they are created, so creation is serialised here while
// debugging), which ties D3DMetal's "marking PSO(N) as no-op" errors to the shaders behind them.
static CRITICAL_SECTION pso_lock; static unsigned pso_seq;
static UINT64 bytecode_hash(D3D12_SHADER_BYTECODE b)
{
    UINT64 h = 0;
    if (b.pShaderBytecode && b.BytecodeLength >= 32 && !memcmp(b.pShaderBytecode, "DXBC", 4)) memcpy(&h, (const UINT8 *)b.pShaderBytecode + 4, 8);
    return h;
}
static HRESULT (STDMETHODCALLTYPE *real_CreateGraphicsPipelineState)(ID3D12Device *, const D3D12_GRAPHICS_PIPELINE_STATE_DESC *, REFIID, void **);
static HRESULT (STDMETHODCALLTYPE *real_CreateComputePipelineState)(ID3D12Device *, const D3D12_COMPUTE_PIPELINE_STATE_DESC *, REFIID, void **);
static HRESULT STDMETHODCALLTYPE hook_CreateGraphicsPipelineState(ID3D12Device *dev, const D3D12_GRAPHICS_PIPELINE_STATE_DESC *d, REFIID riid, void **out)
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC copy; D3D12_INPUT_ELEMENT_DESC elems[D3D12_IA_VERTEX_INPUT_STRUCTURE_ELEMENT_COUNT]; HRESULT hr; unsigned n;
    if (d && resolve_append_offsets(&d->InputLayout, elems)) { copy = *d; copy.InputLayout.pInputElementDescs = elems; d = &copy; }
    if (!dbg) return real_CreateGraphicsPipelineState(dev, d, riid, out);
    EnterCriticalSection(&pso_lock);
    hr = real_CreateGraphicsPipelineState(dev, d, riid, out);
    n = pso_seq++;
    LeaveCriticalSection(&pso_lock);
    if (d) LOG("pipeline %u: vs %016llx ps %016llx ds %016llx hs %016llx gs %016llx, %u targets (format %u), %u samples, hr 0x%08lx", n,
               (unsigned long long)bytecode_hash(d->VS), (unsigned long long)bytecode_hash(d->PS), (unsigned long long)bytecode_hash(d->DS),
               (unsigned long long)bytecode_hash(d->HS), (unsigned long long)bytecode_hash(d->GS), d->NumRenderTargets, (unsigned)d->RTVFormats[0],
               d->SampleDesc.Count, (unsigned long)hr);
    return hr;
}
static HRESULT STDMETHODCALLTYPE hook_CreateComputePipelineState(ID3D12Device *dev, const D3D12_COMPUTE_PIPELINE_STATE_DESC *d, REFIID riid, void **out)
{
    HRESULT hr; unsigned n;
    EnterCriticalSection(&pso_lock);
    hr = real_CreateComputePipelineState(dev, d, riid, out);
    n = pso_seq++;
    LeaveCriticalSection(&pso_lock);
    if (d) LOG("pipeline %u: cs %016llx, hr 0x%08lx", n, (unsigned long long)bytecode_hash(d->CS), (unsigned long)hr);
    return hr;
}
static HRESULT (STDMETHODCALLTYPE *real_CreatePipelineState)(ID3D12Device2 *, const D3D12_PIPELINE_STATE_STREAM_DESC *, REFIID, void **);
static HRESULT STDMETHODCALLTYPE hook_CreatePipelineState(ID3D12Device2 *dev, const D3D12_PIPELINE_STATE_STREAM_DESC *d, REFIID riid, void **out)
{
    D3D12_PIPELINE_STATE_STREAM_DESC copy; D3D12_INPUT_ELEMENT_DESC elems[D3D12_IA_VERTEX_INPUT_STRUCTURE_ELEMENT_COUNT]; void *buf = NULL; HRESULT hr;
    if (rewrite_stream(d, &copy, &buf, elems)) d = &copy;
    hr = real_CreatePipelineState(dev, d, riid, out);
    free(buf);
    return hr;
}
// A pipeline library hands back a stored pipeline only for the description it was stored with, and the
// stored one went to D3DMetal rewritten, so loads are rewritten the same way.
static HRESULT (STDMETHODCALLTYPE *real_LoadGraphicsPipeline)(ID3D12PipelineLibrary *, LPCWSTR, const D3D12_GRAPHICS_PIPELINE_STATE_DESC *, REFIID, void **);
static HRESULT STDMETHODCALLTYPE hook_LoadGraphicsPipeline(ID3D12PipelineLibrary *lib, LPCWSTR name, const D3D12_GRAPHICS_PIPELINE_STATE_DESC *d, REFIID riid, void **out)
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC copy; D3D12_INPUT_ELEMENT_DESC elems[D3D12_IA_VERTEX_INPUT_STRUCTURE_ELEMENT_COUNT];
    if (d && resolve_append_offsets(&d->InputLayout, elems)) { copy = *d; copy.InputLayout.pInputElementDescs = elems; d = &copy; }
    return real_LoadGraphicsPipeline(lib, name, d, riid, out);
}
static HRESULT (STDMETHODCALLTYPE *real_LoadPipeline)(ID3D12PipelineLibrary1 *, LPCWSTR, const D3D12_PIPELINE_STATE_STREAM_DESC *, REFIID, void **);
static HRESULT STDMETHODCALLTYPE hook_LoadPipeline(ID3D12PipelineLibrary1 *lib, LPCWSTR name, const D3D12_PIPELINE_STATE_STREAM_DESC *d, REFIID riid, void **out)
{
    D3D12_PIPELINE_STATE_STREAM_DESC copy; D3D12_INPUT_ELEMENT_DESC elems[D3D12_IA_VERTEX_INPUT_STRUCTURE_ELEMENT_COUNT]; void *buf = NULL; HRESULT hr;
    if (rewrite_stream(d, &copy, &buf, elems)) d = &copy;
    hr = real_LoadPipeline(lib, name, d, riid, out);
    free(buf);
    return hr;
}
static HRESULT (STDMETHODCALLTYPE *real_CreatePipelineLibrary)(ID3D12Device1 *, const void *, SIZE_T, REFIID, void **);
static HRESULT STDMETHODCALLTYPE hook_CreatePipelineLibrary(ID3D12Device1 *dev, const void *blob, SIZE_T len, REFIID riid, void **out)
{
    HRESULT hr = real_CreatePipelineLibrary(dev, blob, len, riid, out);
    if (SUCCEEDED(hr) && out && *out) {
        IUnknown *u = *out; ID3D12PipelineLibrary *lib = NULL; ID3D12PipelineLibrary1 *lib1 = NULL;
        if (SUCCEEDED(u->lpVtbl->QueryInterface(u, &IID_ID3D12PipelineLibrary, (void **)&lib)) && lib) {
            ID3D12PipelineLibraryVtbl *vt = (ID3D12PipelineLibraryVtbl *)lib->lpVtbl;
            patch_slot((void **)&vt->LoadGraphicsPipeline, (void *)hook_LoadGraphicsPipeline, (void **)&real_LoadGraphicsPipeline);
            lib->lpVtbl->Release(lib);
        }
        if (SUCCEEDED(u->lpVtbl->QueryInterface(u, &IID_ID3D12PipelineLibrary1, (void **)&lib1)) && lib1) {
            ID3D12PipelineLibrary1Vtbl *vt = (ID3D12PipelineLibrary1Vtbl *)lib1->lpVtbl;
            patch_slot((void **)&vt->LoadGraphicsPipeline, (void *)hook_LoadGraphicsPipeline, (void **)&real_LoadGraphicsPipeline);
            patch_slot((void **)&vt->LoadPipeline, (void *)hook_LoadPipeline, (void **)&real_LoadPipeline);
            lib1->lpVtbl->Release(lib1);
        }
    }
    return hr;
}
// The two calls below exist only from ID3D12Device1 and ID3D12Device2 on, so they are patched only in
// vtables reached through those interfaces.
static void patch_device1_vtbl(void *iface)
{
    ID3D12Device1Vtbl *vt = *(ID3D12Device1Vtbl **)iface;
    patch_slot((void **)&vt->CreatePipelineLibrary, (void *)hook_CreatePipelineLibrary, (void **)&real_CreatePipelineLibrary);
}
static void patch_device2_vtbl(void *iface)
{
    ID3D12Device2Vtbl *vt = *(ID3D12Device2Vtbl **)iface;
    patch_slot((void **)&vt->CreatePipelineState, (void *)hook_CreatePipelineState, (void **)&real_CreatePipelineState);
}

static void patch_device_vtbl(void *iface)
{
    ID3D12DeviceVtbl *vt = *(ID3D12DeviceVtbl **)iface;
    patch_slot((void **)&vt->CreateQueryHeap, (void *)hook_CreateQueryHeap, (void **)&real_CreateQueryHeap);
    patch_slot((void **)&vt->CheckFeatureSupport, (void *)hook_CheckFeatureSupport, (void **)&real_CheckFeatureSupport);
    patch_slot((void **)&vt->CreateGraphicsPipelineState, (void *)hook_CreateGraphicsPipelineState, (void **)&real_CreateGraphicsPipelineState);
    if (dbg) patch_slot((void **)&vt->CreateComputePipelineState, (void *)hook_CreateComputePipelineState, (void **)&real_CreateComputePipelineState);
    if (sampler_pad) patch_slot((void **)&vt->CreateDescriptorHeap, (void *)hook_CreateDescriptorHeap, (void **)&real_CreateDescriptorHeap);
    if (dbg) {
        patch_slot((void **)&vt->CreateCommittedResource, (void *)hook_CreateCommittedResource, (void **)&real_CreateCommittedResource);
        patch_slot((void **)&vt->CreatePlacedResource, (void *)hook_CreatePlacedResource, (void **)&real_CreatePlacedResource);
        patch_slot((void **)&vt->CreateReservedResource, (void *)hook_CreateReservedResource, (void **)&real_CreateReservedResource);
        patch_slot((void **)&vt->CreateShaderResourceView, (void *)hook_CreateShaderResourceView, (void **)&real_CreateShaderResourceView);
        patch_slot((void **)&vt->CreateUnorderedAccessView, (void *)hook_CreateUnorderedAccessView, (void **)&real_CreateUnorderedAccessView);
        patch_slot((void **)&vt->CreateRenderTargetView, (void *)hook_CreateRenderTargetView, (void **)&real_CreateRenderTargetView);
        patch_slot((void **)&vt->CreateDepthStencilView, (void *)hook_CreateDepthStencilView, (void **)&real_CreateDepthStencilView);
    }
}
static void patch_list_vtbl(void *iface)
{
    ID3D12GraphicsCommandListVtbl *vt = *(ID3D12GraphicsCommandListVtbl **)iface;
    patch_slot((void **)&vt->EndQuery, (void *)hook_EndQuery, (void **)&real_EndQuery);
    patch_slot((void **)&vt->BeginQuery, (void *)hook_BeginQuery, (void **)&real_BeginQuery);
    patch_slot((void **)&vt->ResolveQueryData, (void *)hook_ResolveQueryData, (void **)&real_ResolveQueryData);
    patch_slot((void **)&vt->ResolveSubresource, (void *)hook_ResolveSubresource, (void **)&real_ResolveSubresource);
}
static void patch_queue_vtbl(void *iface)
{
    ID3D12CommandQueueVtbl *vt = *(ID3D12CommandQueueVtbl **)iface;
    patch_slot((void **)&vt->GetTimestampFrequency, (void *)hook_GetTimestampFrequency, (void **)&real_GetTimestampFrequency);
    patch_slot((void **)&vt->GetClockCalibration, (void *)hook_GetClockCalibration, (void **)&real_GetClockCalibration);
}

// Every interface generation may carry its own vtable copy, so patch each one the object answers to.
static void patch_all(IUnknown *obj, const IID *const *ids, int n, void (*patch)(void *))
{
    int i; void *p;
    patch(obj);
    for (i = 0; i < n; i++) {
        if (FAILED(obj->lpVtbl->QueryInterface(obj, ids[i], &p)) || !p) continue;
        patch(p); ((IUnknown *)p)->lpVtbl->Release((IUnknown *)p);
    }
}

static void patch_device(ID3D12Device *dev)
{
    static const IID *const dev_ids[] = {
        &IID_ID3D12Device1, &IID_ID3D12Device2, &IID_ID3D12Device3, &IID_ID3D12Device4, &IID_ID3D12Device5,
#ifdef __ID3D12Device6_INTERFACE_DEFINED__
        &IID_ID3D12Device6,
#endif
#ifdef __ID3D12Device7_INTERFACE_DEFINED__
        &IID_ID3D12Device7,
#endif
#ifdef __ID3D12Device8_INTERFACE_DEFINED__
        &IID_ID3D12Device8,
#endif
#ifdef __ID3D12Device9_INTERFACE_DEFINED__
        &IID_ID3D12Device9,
#endif
#ifdef __ID3D12Device10_INTERFACE_DEFINED__
        &IID_ID3D12Device10,
#endif
    };
    static const IID *const list_ids[] = {
        &IID_ID3D12GraphicsCommandList1, &IID_ID3D12GraphicsCommandList2, &IID_ID3D12GraphicsCommandList3,
        &IID_ID3D12GraphicsCommandList4, &IID_ID3D12GraphicsCommandList5, &IID_ID3D12GraphicsCommandList6,
#ifdef __ID3D12GraphicsCommandList7_INTERFACE_DEFINED__
        &IID_ID3D12GraphicsCommandList7,
#endif
    };
    static const D3D12_COMMAND_LIST_TYPE types[] = { D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_LIST_TYPE_COMPUTE, D3D12_COMMAND_LIST_TYPE_COPY };
    int t, i;
    patch_all((IUnknown *)dev, dev_ids, sizeof dev_ids / sizeof *dev_ids, patch_device_vtbl);
    for (i = 0; i < (int)(sizeof dev_ids / sizeof *dev_ids); i++) {
        void *p = NULL;
        if (FAILED(dev->lpVtbl->QueryInterface(dev, dev_ids[i], &p)) || !p) continue;
        patch_device1_vtbl(p);
        if (i >= 1) patch_device2_vtbl(p);
        ((IUnknown *)p)->lpVtbl->Release((IUnknown *)p);
    }
    for (t = 0; t < 3; t++) {
        ID3D12CommandAllocator *alloc = NULL; ID3D12GraphicsCommandList *list = NULL; ID3D12CommandQueue *queue = NULL;
        D3D12_COMMAND_QUEUE_DESC qd = { types[t], 0, D3D12_COMMAND_QUEUE_FLAG_NONE, 0 };
        if (SUCCEEDED(dev->lpVtbl->CreateCommandQueue(dev, &qd, &IID_ID3D12CommandQueue, (void **)&queue)) && queue) {
            patch_queue_vtbl(queue); queue->lpVtbl->Release(queue);
        }
        if (FAILED(dev->lpVtbl->CreateCommandAllocator(dev, types[t], &IID_ID3D12CommandAllocator, (void **)&alloc)) || !alloc) continue;
        if (SUCCEEDED(dev->lpVtbl->CreateCommandList(dev, 0, types[t], alloc, NULL, &IID_ID3D12GraphicsCommandList, (void **)&list)) && list) {
            patch_all((IUnknown *)list, list_ids, sizeof list_ids / sizeof *list_ids, patch_list_vtbl);
            if (rtas_barrier && types[t] != D3D12_COMMAND_LIST_TYPE_COPY) {
                ID3D12GraphicsCommandList4 *l4 = NULL;
                if (SUCCEEDED(list->lpVtbl->QueryInterface(list, &IID_ID3D12GraphicsCommandList4, (void **)&l4)) && l4) {
                    patch_slot((void **)&l4->lpVtbl->BuildRaytracingAccelerationStructure, (void *)hook_BuildRTAS, (void **)&real_BuildRTAS);
                    l4->lpVtbl->Release(l4);
                }
            }
            list->lpVtbl->Close(list); list->lpVtbl->Release(list);
        }
        alloc->lpVtbl->Release(alloc);
    }
    LOG("device %p patched (timestamp frequency %llu Hz)", (void *)dev, (unsigned long long)qpc_freq);
}

// ---- exports -------------------------------------------------------------------------------

HRESULT WINAPI D3D12CreateDevice(IUnknown *adapter, D3D_FEATURE_LEVEL level, REFIID riid, void **out)
{
    typedef HRESULT (WINAPI *PFN)(IUnknown *, D3D_FEATURE_LEVEL, REFIID, void **);
    PFN fn = real_dll ? (PFN)GetProcAddress(real_dll, "D3D12CreateDevice") : NULL;
    HRESULT hr;
    if (!fn) return E_FAIL;
    hr = fn(adapter, level, riid, out);
    if (SUCCEEDED(hr) && out && *out && !idle) {
        ID3D12Device *dev = NULL; IUnknown *u = (IUnknown *)*out;
        if (SUCCEEDED(u->lpVtbl->QueryInterface(u, &IID_ID3D12Device, (void **)&dev)) && dev) { patch_device(dev); dev->lpVtbl->Release(dev); }
    }
    return hr;
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
    if (reason == DLL_PROCESS_ATTACH) {
        LARGE_INTEGER f;
        self = inst;
        DisableThreadLibraryCalls(inst);
        InitializeCriticalSection(&ring_lock);
        InitializeCriticalSection(&pso_lock);
        QueryPerformanceFrequency(&f); qpc_freq = (UINT64)f.QuadPart;
        dbg = GetEnvironmentVariableA("HB_TSSHIM_DEBUG", NULL, 0) > 0;
        { char v[8]; idle = GetEnvironmentVariableA("HB_D3D12_TSSHIM", v, sizeof v) == 1 && v[0] == '0'; }   // kill switch
        { char v[8]; pass_depth_resolve = GetEnvironmentVariableA("HB_TSSHIM_DEPTHRESOLVE", v, sizeof v) == 1 && v[0] == '1'; }
        diag = GetEnvironmentVariableA("FH6SHIM_DIAG", NULL, 0) > 0;
        { char v[8]; keep_append_offsets = GetEnvironmentVariableA("HB_TSSHIM_APPEND_OFFSETS", v, sizeof v) == 1 && v[0] == '0'; }
        { char v[32]; DWORD n = GetEnvironmentVariableA("FH6SHIM_TS", v, sizeof v);
          if (n && n < sizeof v) ts_mode = !strcmp(v, "unsupported") ? 1 : !strcmp(v, "unsupported-noheap") ? 2 : 0; }
        { char v[8]; prefill = GetEnvironmentVariableA("FH6SHIM_PREFILL", v, sizeof v) == 1 && v[0] == '1'; }
        { char v[8]; vrs_off = GetEnvironmentVariableA("FH6SHIM_VRS_OFF", v, sizeof v) == 1 && v[0] == '1'; }
        { char v[8]; sampler_pad = GetEnvironmentVariableA("FH6SHIM_SAMPLER_PAD", v, sizeof v) == 1 && v[0] == '1'; }
        { char v[8]; rtas_barrier = GetEnvironmentVariableA("FH6SHIM_RTAS_BARRIER", v, sizeof v) == 1 && v[0] == '1'; }
        { char v[16]; DWORD n = GetEnvironmentVariableA("FH6SHIM_SM_CLAMP", v, sizeof v); if (n && n < sizeof v) sm_clamp = (UINT)strtoul(v, NULL, 0); }
        if (!load_real()) return FALSE;
        RESOLVE(D3D12CoreCreateLayeredDevice); RESOLVE(D3D12CoreGetLayeredDeviceSize); RESOLVE(D3D12CoreRegisterLayers);
        RESOLVE(D3D12CreateRootSignatureDeserializer); RESOLVE(D3D12CreateVersionedRootSignatureDeserializer);
        RESOLVE(D3D12EnableExperimentalFeatures); RESOLVE(D3D12GetDebugInterface); RESOLVE(D3D12SerializeRootSignature);
        RESOLVE(D3D12SerializeVersionedRootSignature); RESOLVE(GetBehaviorValue);
        DLOG("loaded in front of the real d3d12.dll (fh6 diag build; depthresolve=%s, idle=%d, sm_clamp=0x%x, diag=%d, debug=%d, ts=%d, prefill=%d, vrs_off=%d, sampler_pad=%d, rtas_barrier=%d, append_offsets=%d, qpc=%llu Hz)",
            pass_depth_resolve ? "passed" : "dropped", idle, sm_clamp, diag, dbg, ts_mode, prefill, vrs_off, sampler_pad, rtas_barrier, !keep_append_offsets, (unsigned long long)qpc_freq);
    }
    return TRUE;
}
