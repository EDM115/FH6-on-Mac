// Isolated GPTK4.0b2 MPL resolve-source trial. Only the verified GetView caller
// and legal multisample depth-read shape may retain the depth view format.
// All other requests are forwarded unchanged; Metal assertions remain enabled.
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
#include <mach-o/loader.h>

static IMP original_view;
static IMP original_swizzle;
static IMP original_simple;
static pthread_mutex_t installation = PTHREAD_MUTEX_INITIALIZER;
static std::atomic<unsigned> captures{0};
static std::atomic<unsigned> repairs{0};

static bool legal_source(id<MTLTexture> t, MTLTextureType type, NSRange levels,
                         NSRange slices, MTLTextureSwizzleChannels s) {
    const auto identity=MTLTextureSwizzleChannelsDefault;
    return t.pixelFormat==MTLPixelFormatDepth32Float &&
      t.textureType==MTLTextureType2DMultisample && type==MTLTextureType2DMultisample &&
      t.sampleCount==4 && t.mipmapLevelCount==1 && t.arrayLength==1 &&
      (t.usage & MTLTextureUsageShaderRead) && levels.location==0 && levels.length==1 &&
      slices.location==0 && slices.length==1 && !memcmp(&s,&identity,sizeof(s));
}

// Read only a return slot whose existence/layout follows from this exact
// verified GetView prologue: six pushes and sub rsp,0x88 (0xb8 bytes total).
// view_frame is capture_swizzle's own frame; +16 is the caller's pre-call RSP.
static bool mpl_source_call(void *return_address, void *view_frame) {
    Dl_info info={};
    if (!dladdr(return_address,&info) || !info.dli_fbase) return false;
    const auto base=(const unsigned char *)info.dli_fbase;
    if ((uintptr_t)return_address-(uintptr_t)base!=0x3fbc4) return false;
    const auto h=(const mach_header_64 *)base;
    if (h->magic!=MH_MAGIC_64 || h->ncmds>100 || h->sizeofcmds>32768) return false;
    const unsigned char uuid[16]={0x67,0x4e,0x66,0x2b,0x6b,0x5c,0x3f,0xd9,0x9a,0x8a,0xf4,0x15,0x60,0x9d,0x2f,0x6a};
    bool matched=false;
    auto c=(const load_command *)(h+1);
    const auto end=base+sizeof(*h)+h->sizeofcmds;
    for (unsigned i=0;i<h->ncmds;++i) {
      if ((const unsigned char *)c+sizeof(*c)>end || c->cmdsize<sizeof(*c) ||
          (const unsigned char *)c+c->cmdsize>end) return false;
      if (c->cmd==LC_UUID && c->cmdsize>=sizeof(uuid_command))
        matched=!memcmp(((const uuid_command *)c)->uuid,uuid,16);
      c=(const load_command *)((const unsigned char *)c+c->cmdsize);
    }
    const unsigned char prologue[]={0x55,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x53,0x48,0x81,0xec,0x88,0,0,0};
    const unsigned char call[]={0xff,0x15,0x64,0xe7,0x46,0};
    if (!matched || memcmp(base+0x3f9dc,prologue,sizeof(prologue)) ||
        memcmp(base+0x3fbbe,call,sizeof(call))) return false;
    uintptr_t caller=0;
    memcpy(&caller,(const unsigned char *)view_frame+16+0xb8,sizeof(caller));
    uintptr_t offset=caller-(uintptr_t)base;
    if (captures.load(std::memory_order_relaxed)<=8)
      fprintf(stderr,"FH6_TEXTURE_GETVIEW_CALLER pid=%d offset=0x%llx source_match=%d\n",
              getpid(),(unsigned long long)offset,offset==0xdceda);
    return offset==0xdceda; // source GetView, never destination GetView at 0xdcf67
}

static id legal_view(id<MTLTexture> texture, MTLTextureType type, NSRange levels,
                    NSRange slices, MTLTextureSwizzleChannels swizzle) {
    using Function=id (*)(id,SEL,MTLPixelFormat,MTLTextureType,NSRange,NSRange,MTLTextureSwizzleChannels);
    return ((Function)original_swizzle)(texture,
      @selector(newTextureViewWithPixelFormat:textureType:levels:slices:swizzle:),
      MTLPixelFormatDepth32Float,type,levels,slices,swizzle);
}

// Standalone pixel-readback test entry; never used by the game hook's caller guard.
extern "C" id fh6_test_depth_source_view(id<MTLTexture> texture) {
    const auto s=MTLTextureSwizzleChannelsDefault;
    const auto r=NSMakeRange(0,1);
    if (!original_swizzle || !legal_source(texture,MTLTextureType2DMultisample,r,r,s)) return nil;
    return legal_view(texture,MTLTextureType2DMultisample,r,r,s);
}

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

static void capture(id<MTLTexture> texture, SEL selector,
                       MTLPixelFormat format, MTLTextureType type,
                       NSRange levels, NSRange slices, MTLTextureSwizzleChannels swizzle) {
    if (format==MTLPixelFormatR32Float && texture.pixelFormat==MTLPixelFormatDepth32Float &&
        captures.fetch_add(1, std::memory_order_relaxed)<8) {
        flockfile(stderr);
        fprintf(stderr, "FH6_TEXTURE_INVALID_VIEW pid=%d method=%s class=%s source_format=%lu requested_format=%lu "
                "source_type=%lu requested_type=%lu width=%lu height=%lu depth=%lu "
                "samples=%lu mip_count=%lu array_length=%lu usage=0x%lx storage=%lu "
                "levels=%lu,%lu slices=%lu,%lu swizzle=%u,%u,%u,%u\n", getpid(),
                sel_getName(selector), class_getName(object_getClass(texture)),
                (unsigned long)texture.pixelFormat, (unsigned long)format,
                (unsigned long)texture.textureType, (unsigned long)type,
                (unsigned long)texture.width, (unsigned long)texture.height,
                (unsigned long)texture.depth, (unsigned long)texture.sampleCount,
                (unsigned long)texture.mipmapLevelCount, (unsigned long)texture.arrayLength,
                (unsigned long)texture.usage, (unsigned long)texture.storageMode,
                (unsigned long)levels.location, (unsigned long)levels.length,
                (unsigned long)slices.location, (unsigned long)slices.length,
                swizzle.red, swizzle.green, swizzle.blue, swizzle.alpha);
        frames();
        fflush(stderr);
        funlockfile(stderr);
    }
}

static id capture_view(id<MTLTexture> texture, SEL selector,
                       MTLPixelFormat format, MTLTextureType type, NSRange levels, NSRange slices) {
    capture(texture, selector, format, type, levels, slices, MTLTextureSwizzleChannelsDefault);
    using Function = id (*)(id, SEL, MTLPixelFormat, MTLTextureType, NSRange, NSRange);
    return ((Function)original_view)(texture, selector, format, type, levels, slices);
}

__attribute__((noinline)) static id capture_swizzle(id<MTLTexture> texture, SEL selector,
                       MTLPixelFormat format, MTLTextureType type, NSRange levels, NSRange slices,
                       MTLTextureSwizzleChannels swizzle) {
    capture(texture, selector, format, type, levels, slices, swizzle);
    if (format==MTLPixelFormatR32Float && legal_source(texture,type,levels,slices,swizzle) &&
        mpl_source_call(__builtin_return_address(0),__builtin_frame_address(0))) {
      id view=legal_view(texture,type,levels,slices,swizzle);
      if (view) {
        unsigned n=repairs.fetch_add(1,std::memory_order_relaxed)+1;
        if (n<=8 || !(n%1000))
          fprintf(stderr,"FH6_TEXTURE_SOURCE_REPAIRED pid=%d count=%u width=%lu height=%lu samples=%lu\n",
                  getpid(),n,(unsigned long)texture.width,(unsigned long)texture.height,(unsigned long)texture.sampleCount);
        return view;
      }
    }
    using Function = id (*)(id, SEL, MTLPixelFormat, MTLTextureType, NSRange, NSRange, MTLTextureSwizzleChannels);
    return ((Function)original_swizzle)(texture, selector, format, type, levels, slices, swizzle);
}

static id capture_simple(id<MTLTexture> texture, SEL selector, MTLPixelFormat format) {
    capture(texture, selector, format, texture.textureType, NSMakeRange(0,texture.mipmapLevelCount),
            NSMakeRange(0,texture.arrayLength), MTLTextureSwizzleChannelsDefault);
    using Function = id (*)(id, SEL, MTLPixelFormat);
    return ((Function)original_simple)(texture, selector, format);
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
                Method swizzled = class_getInstanceMethod(object_getClass(texture),
                    @selector(newTextureViewWithPixelFormat:textureType:levels:slices:swizzle:));
                Method simple = class_getInstanceMethod(object_getClass(texture),
                    @selector(newTextureViewWithPixelFormat:));
                if (swizzled) {
                    original_swizzle = method_getImplementation(swizzled);
                    method_setImplementation(swizzled, (IMP)capture_swizzle);
                }
                if (simple) {
                    original_simple = method_getImplementation(simple);
                    method_setImplementation(simple, (IMP)capture_simple);
                }
                fprintf(stderr, "FH6_TEXTURE_HOOK_INSTALLED pid=%d class=%s version=3 trial=mpl-source methods=%d\n",
                        getpid(), class_getName(object_getClass(texture)), 1+!!swizzled+!!simple);
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
