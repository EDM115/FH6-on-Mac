// Diagnostic only: bounded metadata/backtrace for Depth32Float -> R32Float.
// Forward every texture request and assertion unchanged. No texture contents,
// labels, account information, or network traffic are inspected.
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <objc/runtime.h>
#include <atomic>
#include <dlfcn.h>
#include <execinfo.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static IMP original_view;
static pthread_mutex_t installation = PTHREAD_MUTEX_INITIALIZER;
static std::atomic<unsigned> captures{0};

static void frames(void) {
    void *addresses[32];
    int count = backtrace(addresses, 32);
    for (int i=1; i<count; ++i) {
        Dl_info info = {};
        if (dladdr(addresses[i], &info)) {
            const char *file = info.dli_fname ? strrchr(info.dli_fname, '/') : nullptr;
            fprintf(stderr, "FH6_TEXTURE_FRAME pid=%d n=%d address=%p module=%s offset=0x%llx symbol=%.300s\n",
                    getpid(), i, addresses[i], file ? file+1 : "unknown",
                    (unsigned long long)((uintptr_t)addresses[i]-(uintptr_t)info.dli_fbase),
                    info.dli_sname ? info.dli_sname : "unknown");
        } else {
            fprintf(stderr, "FH6_TEXTURE_FRAME pid=%d n=%d address=%p module=unknown\n",
                    getpid(), i, addresses[i]);
        }
    }
}

static id capture_view(id<MTLTexture> texture, SEL selector,
                       MTLPixelFormat format, MTLTextureType type,
                       NSRange levels, NSRange slices) {
    if (format==MTLPixelFormatR32Float && texture.pixelFormat==MTLPixelFormatDepth32Float &&
        captures.fetch_add(1, std::memory_order_relaxed)<8) {
        flockfile(stderr);
        fprintf(stderr, "FH6_TEXTURE_INVALID_VIEW pid=%d source_format=%lu requested_format=%lu "
                "source_type=%lu requested_type=%lu width=%lu height=%lu depth=%lu "
                "samples=%lu mip_count=%lu array_length=%lu usage=0x%lx storage=%lu "
                "levels=%lu,%lu slices=%lu,%lu\n", getpid(),
                (unsigned long)texture.pixelFormat, (unsigned long)format,
                (unsigned long)texture.textureType, (unsigned long)type,
                (unsigned long)texture.width, (unsigned long)texture.height,
                (unsigned long)texture.depth, (unsigned long)texture.sampleCount,
                (unsigned long)texture.mipmapLevelCount, (unsigned long)texture.arrayLength,
                (unsigned long)texture.usage, (unsigned long)texture.storageMode,
                (unsigned long)levels.location, (unsigned long)levels.length,
                (unsigned long)slices.location, (unsigned long)slices.length);
        frames();
        fflush(stderr);
        funlockfile(stderr);
    }
    using Function = id (*)(id, SEL, MTLPixelFormat, MTLTextureType, NSRange, NSRange);
    return ((Function)original_view)(texture, selector, format, type, levels, slices);
}

static void install(id<MTLDevice> device) {
    if (!device) return;
    pthread_mutex_lock(&installation);
    if (!original_view) {
        @autoreleasepool {
            MTLTextureDescriptor *desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:
                MTLPixelFormatDepth32Float width:1 height:1 mipmapped:NO];
            desc.usage = MTLTextureUsageRenderTarget|MTLTextureUsageShaderRead|MTLTextureUsagePixelFormatView;
            desc.storageMode = MTLStorageModePrivate;
            id<MTLTexture> texture = [device newTextureWithDescriptor:desc];
            SEL selector = @selector(newTextureViewWithPixelFormat:textureType:levels:slices:);
            Method method = texture ? class_getInstanceMethod(object_getClass(texture), selector) : nullptr;
            if (method) {
                original_view = method_getImplementation(method);
                method_setImplementation(method, (IMP)capture_view);
                fprintf(stderr, "FH6_TEXTURE_HOOK_INSTALLED pid=%d class=%s\n",
                        getpid(), class_getName(object_getClass(texture)));
            }
            [texture release];
        }
    }
    pthread_mutex_unlock(&installation);
}

static id<MTLDevice> capture_device(void) {
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    install(device);
    return device;
}
static NSArray<id<MTLDevice>> *capture_devices(void) {
    NSArray<id<MTLDevice>> *devices = MTLCopyAllDevices();
    for (id<MTLDevice> device in devices) install(device);
    return devices;
}
#define INTERPOSE(replacement, original) \
    __attribute__((used, section("__DATA,__interpose"))) \
    static const struct { const void *r; const void *o; } pair_##replacement = \
      {(const void *)&replacement,(const void *)&original}
INTERPOSE(capture_device,MTLCreateSystemDefaultDevice);
INTERPOSE(capture_devices,MTLCopyAllDevices);
__attribute__((constructor)) static void loaded(void) {
    fprintf(stderr,"FH6_TEXTURE_DIAGNOSTIC_LOADED pid=%d\n",getpid());
}
