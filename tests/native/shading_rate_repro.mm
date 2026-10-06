// Self-contained shader linkage probe. Uses no game files or account state.
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
// DXC's non-Windows adapter defines BOOL as bool; keep it distinct from ObjC BOOL.
#define BOOL DXC_BOOL
#include "dxc/dxcapi.h"
#undef BOOL
#include <dlfcn.h>
#include <fstream>
#include <string>
#include <vector>

static void save(const std::string &path, const void *data, size_t size) {
    std::ofstream f(path, std::ios::binary);
    f.write((const char *)data, size);
}
template<typename T> static T symbol(void *handle, const char *name) {
    void *p = dlsym(handle, name);
    if (!p) { fprintf(stderr, "Missing symbol %s: %s\n", name, dlerror()); exit(2); }
    return reinterpret_cast<T>(p);
}

int main(int argc, char **argv) {
    if (argc != 3 && argc != 4) { fprintf(stderr, "usage: repro resources-directory output-directory [fallback.ll]\n"); return 1; }
    @autoreleasepool {
        std::string resources = argv[1], output = argv[2];
        void *dxc = dlopen((resources + "/libdxcompiler.dylib").c_str(), RTLD_NOW|RTLD_LOCAL);
        if (!dxc) { fprintf(stderr, "DXC: %s\n", dlerror()); return 2; }
        auto create = symbol<DxcCreateInstanceProc>(dxc, "DxcCreateInstance");
        IDxcCompiler3 *compiler = nullptr;
        HRESULT hr = create(CLSID_DxcCompiler, __uuidof(IDxcCompiler3), (void **)&compiler);
        if (FAILED(hr) || !compiler) { fprintf(stderr, "DXC factory: 0x%x\n", (unsigned)hr); return 3; }
        void *ir = dlopen((resources + "/libmetalirconverter.dylib").c_str(), RTLD_NOW|RTLD_LOCAL);
        if (!ir) { fprintf(stderr, "MSC: %s\n", dlerror()); return 4; }
        auto irCreate = symbol<void *(*)()>(ir, "IRCompilerCreate");
        auto irDestroy = symbol<void (*)(void *)>(ir, "IRCompilerDestroy");
        auto irSetEntry = symbol<void (*)(void *, const char *)>(ir, "IRCompilerSetEntryPointName");
        auto irCreateDXIL = symbol<void *(*)(const char *, size_t)>(ir, "_Z22IRObjectCreateFromDXILPKcm");
        auto irCompile = symbol<void *(*)(void *, void *, const void *, void **)>(ir, "IRCompilerAllocCompileAndLink");
        auto irStage = symbol<unsigned (*)(const void *)>(ir, "IRObjectGetMetalIRShaderStage");
        auto irObjectDestroy = symbol<void (*)(void *)>(ir, "IRObjectDestroy");
        auto libCreate = symbol<void *(*)()>(ir, "IRMetalLibBinaryCreate");
        auto libDestroy = symbol<void (*)(void *)>(ir, "IRMetalLibBinaryDestroy");
        auto getLib = symbol<bool (*)(const void *, unsigned, void *)>(ir, "IRObjectGetMetalLibBinary");
        auto libSize = symbol<size_t (*)(const void *)>(ir, "IRMetalLibGetBytecodeSize");
        auto libBytes = symbol<void (*)(const void *, void *)>(ir, "IRMetalLibGetBytecode");
        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        if (!device) return 5;
        struct Shader { const char *name, *source; const wchar_t *target; } shaders[] = {
            {"vertex", "float4 main(uint id : SV_VertexID) : SV_Position { float2 p[3]={float2(-1,-1),float2(3,-1),float2(-1,3)}; return float4(p[id],0,1); }", L"vs_6_4"},
            {"fragment_control", "float4 main() : SV_Target { return float4(1,0,0,1); }", L"ps_6_4"},
            {"fragment_shading_rate", "float4 main(uint rate : SV_ShadingRate) : SV_Target { return rate == 0 ? float4(1,0,0,1) : float4(0,1,0,1); }", L"ps_6_4"},
            {"fragment_fallback", nullptr, nullptr},
        };
        NSMutableArray *results = [NSMutableArray new];
        NSMutableArray *functions = [NSMutableArray new];
        for (auto &s : shaders) {
            IDxcBlob *blob = nullptr;
            if (!s.source) {
                if (argc != 4) continue;
                std::ifstream file(argv[3], std::ios::binary);
                std::string text((std::istreambuf_iterator<char>(file)), {});
                if (text.empty()) return 14;
                IDxcUtils *utils = nullptr;
                IDxcAssembler *assembler = nullptr;
                hr = create(CLSID_DxcUtils, __uuidof(IDxcUtils), (void **)&utils);
                if (FAILED(hr) || !utils) return 15;
                hr = create(CLSID_DxcAssembler, __uuidof(IDxcAssembler), (void **)&assembler);
                if (FAILED(hr) || !assembler) return 16;
                IDxcBlobEncoding *inputText = nullptr;
                utils->CreateBlob(text.data(), (UINT32)text.size(), DXC_CP_UTF8, &inputText);
                IDxcOperationResult *assembled = nullptr;
                hr = assembler->AssembleToContainer(inputText, &assembled);
                if (FAILED(hr) || !assembled) return 17;
                HRESULT status; assembled->GetStatus(&status);
                if (FAILED(status)) {
                    IDxcBlobEncoding *errors = nullptr; assembled->GetErrorBuffer(&errors);
                    fprintf(stderr, "assembler: %.*s\n", errors ? (int)errors->GetBufferSize() : 0,
                            errors ? (const char *)errors->GetBufferPointer() : "");
                    return 18;
                }
                assembled->GetResult(&blob);
                assembled->Release(); inputText->Release(); assembler->Release(); utils->Release();
            } else {
            DxcBuffer source = {s.source, strlen(s.source), DXC_CP_UTF8};
            const wchar_t *args[] = {L"-E", L"main", L"-T", s.target, L"-O3"};
            IDxcResult *result = nullptr;
            hr = compiler->Compile(&source, args, 5, nullptr, __uuidof(IDxcResult), (void **)&result);
            if (FAILED(hr) || !result) return 6;
            HRESULT status;
            result->GetStatus(&status);
            IDxcBlobUtf8 *errors = nullptr;
            result->GetOutput(DXC_OUT_ERRORS, __uuidof(IDxcBlobUtf8), (void **)&errors, nullptr);
            if (errors && errors->GetStringLength()) fprintf(stderr, "%s DXC: %s\n", s.name, errors->GetStringPointer());
            if (FAILED(status)) return 7;
            result->GetResult(&blob);
            if (errors) errors->Release(); result->Release();
            }
            save(output + "/" + s.name + ".dxil", blob->GetBufferPointer(), blob->GetBufferSize());
            DxcBuffer binarySource = {blob->GetBufferPointer(), blob->GetBufferSize(), 0};
            IDxcResult *disassembly = nullptr;
            hr = compiler->Disassemble(&binarySource, __uuidof(IDxcResult), (void **)&disassembly);
            if (FAILED(hr) || !disassembly) return 12;
            IDxcBlob *assembly = nullptr;
            disassembly->GetResult(&assembly);
            if (!assembly) return 13;
            save(output + "/" + s.name + ".ll", assembly->GetBufferPointer(), assembly->GetBufferSize());
            assembly->Release(); disassembly->Release();
            void *c = irCreate();
            irSetEntry(c, "main");
            void *input = irCreateDXIL((const char *)blob->GetBufferPointer(), blob->GetBufferSize());
            void *error = nullptr;
            void *converted = irCompile(c, nullptr, input, &error);
            if (!converted) { fprintf(stderr, "%s MSC compile failed (error object %p)\n", s.name, error); return 8; }
            unsigned stage = irStage(converted);
            void *binary = libCreate();
            getLib(converted, stage, binary);
            size_t size = libSize(binary);
            if (!size || size > 16*1024*1024) { fprintf(stderr, "Invalid metallib size %zu\n", size); return 9; }
            std::vector<uint8_t> bytes(size);
            libBytes(binary, bytes.data());
            save(output + "/" + s.name + ".metallib", bytes.data(), bytes.size());
            dispatch_data_t data = dispatch_data_create(bytes.data(), bytes.size(), nullptr, DISPATCH_DATA_DESTRUCTOR_DEFAULT);
            NSError *metalError = nil;
            id<MTLLibrary> library = [device newLibraryWithData:data error:&metalError];
            if (!library) { fprintf(stderr, "%s Metal library: %s\n", s.name, metalError.description.UTF8String); return 10; }
            id<MTLFunction> fn = [library newFunctionWithName:library.functionNames.firstObject];
            if (!fn) return 11;
            [functions addObject:fn];
            [results addObject:@{@"shader":@(s.name), @"dxil_size":@(blob->GetBufferSize()), @"metallib_size":@(size), @"stage":@(stage), @"functions":library.functionNames}];
            libDestroy(binary); irObjectDestroy(converted); irObjectDestroy(input); irDestroy(c);
            blob->Release();
        }
        NSMutableArray *pipelines = [NSMutableArray new];
        for (NSUInteger i = 1; i < functions.count; ++i) {
            MTLRenderPipelineDescriptor *p = [MTLRenderPipelineDescriptor new];
            p.vertexFunction = functions[0]; p.fragmentFunction = functions[i];
            p.colorAttachments[0].pixelFormat = MTLPixelFormatRGBA8Unorm;
            NSError *error = nil;
            id<MTLRenderPipelineState> state = [device newRenderPipelineStateWithDescriptor:p error:&error];
            NSMutableDictionary *record = [@{@"fragment":@(shaders[i].name), @"success":@(state != nil),
                                            @"error":error ? error.description : @"none"} mutableCopy];
            if (state) {
                // A native full-screen vertex shader avoids introducing unrelated
                // converter-specific draw-argument bindings into the pixel check.
                NSString *vs = @"#include <metal_stdlib>\nusing namespace metal;\nvertex float4 v(uint id [[vertex_id]]) { float2 p[3]={float2(-1,-1),float2(3,-1),float2(-1,3)};return float4(p[id],0,1);}";
                id<MTLLibrary> vl = [device newLibraryWithSource:vs options:nil error:&error];
                p.vertexFunction = [vl newFunctionWithName:@"v"];
                id<MTLRenderPipelineState> drawState = [device newRenderPipelineStateWithDescriptor:p error:&error];
                if (!drawState) return 19;
                MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:8 height:8 mipmapped:NO];
                td.usage = MTLTextureUsageRenderTarget; td.storageMode = MTLStorageModeShared;
                id<MTLTexture> texture = [device newTextureWithDescriptor:td];
                MTLRenderPassDescriptor *pass = [MTLRenderPassDescriptor renderPassDescriptor];
                pass.colorAttachments[0].texture = texture;
                pass.colorAttachments[0].loadAction = MTLLoadActionClear;
                pass.colorAttachments[0].storeAction = MTLStoreActionStore;
                pass.colorAttachments[0].clearColor = MTLClearColorMake(0,0,1,1);
                id<MTLCommandQueue> queue = [device newCommandQueue];
                id<MTLCommandBuffer> command = [queue commandBuffer];
                id<MTLRenderCommandEncoder> encoder = [command renderCommandEncoderWithDescriptor:pass];
                [encoder setRenderPipelineState:drawState];
                [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
                [encoder endEncoding]; [command commit]; [command waitUntilCompleted];
                unsigned char pixel[4] = {};
                [texture getBytes:pixel bytesPerRow:4 fromRegion:MTLRegionMake2D(4,4,1,1) mipmapLevel:0];
                record[@"render_status"] = @((unsigned long)command.status);
                record[@"render_error"] = command.error ? command.error.description : @"none";
                record[@"center_rgba"] = @[@(pixel[0]),@(pixel[1]),@(pixel[2]),@(pixel[3])];
            }
            [pipelines addObject:record];
        }
        NSDictionary *report = @{@"device":device.name, @"shaders":results, @"pipelines":pipelines};
        NSData *json = [NSJSONSerialization dataWithJSONObject:report options:NSJSONWritingPrettyPrinted error:nil];
        fwrite(json.bytes, 1, json.length, stdout); fputc('\n', stdout);
        compiler->Release();
    }
    return 0;
}
