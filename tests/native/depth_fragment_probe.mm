// Synthetic render resolve: legal Depth32Float view read by a float fragment shader.
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <cmath>
#include <cstdio>
#include <dlfcn.h>
int main(int argc, char **) {
  @autoreleasepool {
    id<MTLDevice> device=MTLCreateSystemDefaultDevice();
    if (!device || ![device supportsTextureSampleCount:4]) return 2;
    MTLTextureDescriptor *d=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:
                            MTLPixelFormatDepth32Float width:8 height:8 mipmapped:NO];
    d.textureType=MTLTextureType2DMultisample; d.sampleCount=4;
    d.storageMode=MTLStorageModePrivate;
    d.usage=MTLTextureUsageRenderTarget|MTLTextureUsageShaderRead|MTLTextureUsagePixelFormatView;
    id<MTLTexture> source=[device newTextureWithDescriptor:d];
    MTLTextureDescriptor *o=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:
                            MTLPixelFormatR32Float width:8 height:8 mipmapped:NO];
    o.storageMode=MTLStorageModeShared; o.usage=MTLTextureUsageRenderTarget;
    id<MTLTexture> target=[device newTextureWithDescriptor:o];
    if (!source || !target) return 3;
    NSString *code=@"#include <metal_stdlib>\nusing namespace metal;\n"
      "vertex float4 fullscreen(uint i [[vertex_id]]) {float2 p[3]={float2(-1,-1),float2(3,-1),float2(-1,3)};return float4(p[i],0,1);}\n"
      "struct DepthOut { float depth [[depth(any)]]; };\n"
      "fragment DepthOut write_depth(uint sample [[sample_id]]) {return {0.1f*(sample+1)};}\n"
      "fragment float resolve_depth(texture2d_ms<float,access::read> input [[texture(0)]], "
      "float4 position [[position]]) { uint2 p=uint2(position.xy); "
      "float sum=0; for(uint i=0;i<input.get_num_samples();++i) sum+=input.read(p,i).x; "
      "return sum/input.get_num_samples(); }";
    NSError *error=nil;
    id<MTLLibrary> lib=[device newLibraryWithSource:code options:nil error:&error];
    if (!lib) {fprintf(stderr,"LIBRARY_ERROR %s\n",error.description.UTF8String);return 4;}
    id<MTLFunction> fn=[lib newFunctionWithName:@"resolve_depth"];
    MTLRenderPipelineDescriptor *resolveDesc=[MTLRenderPipelineDescriptor new];
    resolveDesc.vertexFunction=[lib newFunctionWithName:@"fullscreen"];
    resolveDesc.fragmentFunction=fn;
    resolveDesc.colorAttachments[0].pixelFormat=MTLPixelFormatR32Float;
    id<MTLRenderPipelineState> pipeline=[device newRenderPipelineStateWithDescriptor:resolveDesc error:&error];
    if (!pipeline) {fprintf(stderr,"PIPELINE_ERROR %s\n",error.description.UTF8String);return 5;}
    MTLRenderPipelineDescriptor *renderDesc=[MTLRenderPipelineDescriptor new];
    renderDesc.vertexFunction=[lib newFunctionWithName:@"fullscreen"];
    renderDesc.fragmentFunction=[lib newFunctionWithName:@"write_depth"];
    renderDesc.rasterSampleCount=4;renderDesc.depthAttachmentPixelFormat=MTLPixelFormatDepth32Float;
    id<MTLRenderPipelineState> renderPipeline=[device newRenderPipelineStateWithDescriptor:renderDesc error:&error];
    if (!renderPipeline) {fprintf(stderr,"RENDER_PIPELINE_ERROR %s\n",error.description.UTF8String);return 8;}
    MTLDepthStencilDescriptor *depthDesc=[MTLDepthStencilDescriptor new];
    depthDesc.depthCompareFunction=MTLCompareFunctionAlways;depthDesc.depthWriteEnabled=YES;
    id<MTLDepthStencilState> depthState=[device newDepthStencilStateWithDescriptor:depthDesc];
    id<MTLCommandQueue> queue=[device newCommandQueue];
    id<MTLCommandBuffer> commands=[queue commandBuffer];
    MTLRenderPassDescriptor *pass=[MTLRenderPassDescriptor renderPassDescriptor];
    pass.depthAttachment.texture=source;pass.depthAttachment.clearDepth=0.375;
    pass.depthAttachment.loadAction=MTLLoadActionClear;pass.depthAttachment.storeAction=MTLStoreActionStore;
    id<MTLRenderCommandEncoder> render=[commands renderCommandEncoderWithDescriptor:pass];
    [render setRenderPipelineState:renderPipeline];[render setDepthStencilState:depthState];
    [render drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
    [render endEncoding];
    id<MTLTexture> sourceView=nil;
    if (argc>1) {
      using TestView=id (*)(id<MTLTexture>);
      auto test=(TestView)dlsym(RTLD_DEFAULT,"fh6_test_depth_source_view");
      if (!test) return 10;
      sourceView=test(source);
    } else sourceView=[source newTextureViewWithPixelFormat:MTLPixelFormatDepth32Float
      textureType:MTLTextureType2DMultisample levels:NSMakeRange(0,1) slices:NSMakeRange(0,1)
      swizzle:MTLTextureSwizzleChannelsDefault];
    if (!sourceView) return 9;
    MTLRenderPassDescriptor *resolvePass=[MTLRenderPassDescriptor renderPassDescriptor];
    resolvePass.colorAttachments[0].texture=target;
    resolvePass.colorAttachments[0].loadAction=MTLLoadActionClear;
    resolvePass.colorAttachments[0].storeAction=MTLStoreActionStore;
    id<MTLRenderCommandEncoder> resolve=[commands renderCommandEncoderWithDescriptor:resolvePass];
    [resolve setRenderPipelineState:pipeline];[resolve setFragmentTexture:sourceView atIndex:0];
    [resolve drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
    [resolve endEncoding];[commands commit];[commands waitUntilCompleted];
    if (commands.status!=MTLCommandBufferStatusCompleted) {
      fprintf(stderr,"COMMAND_ERROR %s\n",commands.error.description.UTF8String);return 6;
    }
    float pixels[64]={};
    [target getBytes:pixels bytesPerRow:8*sizeof(float) fromRegion:MTLRegionMake2D(0,0,8,8) mipmapLevel:0];
    // Average must differ from sample0/min=0.1 and max=0.4.
    for (int i=0;i<64;++i) if (std::fabs(pixels[i]-0.25f)>1e-6f) {
      fprintf(stderr,"READBACK_MISMATCH pixel=%d value=%g\n",i,pixels[i]);return 7;
    }
    printf("DEPTH_FRAGMENT_RESOLVE_PASSED samples=4 pixels=64 depth=%g source_format=%lu target_format=%lu\n",
           pixels[0],(unsigned long)source.pixelFormat,(unsigned long)target.pixelFormat);
    return 0;
  }
}
