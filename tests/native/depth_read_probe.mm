// Synthetic check: can a float MSAA shader read Depth32Float without a color view?
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <cmath>
#include <cstdio>
int main() {
  @autoreleasepool {
    id<MTLDevice> device=MTLCreateSystemDefaultDevice();
    if (!device || ![device supportsTextureSampleCount:4]) return 2;
    MTLTextureDescriptor *d=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:
                            MTLPixelFormatDepth32Float width:8 height:8 mipmapped:NO];
    d.textureType=MTLTextureType2DMultisample; d.sampleCount=4;
    d.storageMode=MTLStorageModePrivate;
    d.usage=MTLTextureUsageRenderTarget|MTLTextureUsageShaderRead;
    id<MTLTexture> source=[device newTextureWithDescriptor:d];
    MTLTextureDescriptor *o=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:
                            MTLPixelFormatR32Float width:8 height:8 mipmapped:NO];
    o.storageMode=MTLStorageModeShared; o.usage=MTLTextureUsageShaderWrite;
    id<MTLTexture> target=[device newTextureWithDescriptor:o];
    if (!source || !target) return 3;
    NSString *code=@"#include <metal_stdlib>\nusing namespace metal;\n"
      "vertex float4 fullscreen(uint i [[vertex_id]]) {float2 p[3]={float2(-1,-1),float2(3,-1),float2(-1,3)};return float4(p[i],0,1);}\n"
      "struct DepthOut { float depth [[depth(any)]]; };\n"
      "fragment DepthOut write_depth(uint sample [[sample_id]]) {return {0.1f*(sample+1)};}\n"
      "kernel void resolve_depth(texture2d_ms<float,access::read> input [[texture(0)]], "
      "texture2d<float,access::write> output [[texture(1)]], uint2 p [[thread_position_in_grid]]) { "
      "float sum=0; for(uint i=0;i<input.get_num_samples();++i) sum+=input.read(p,i).x; "
      "output.write(float4(sum/input.get_num_samples(),0,0,1),p); }";
    NSError *error=nil;
    id<MTLLibrary> lib=[device newLibraryWithSource:code options:nil error:&error];
    if (!lib) {fprintf(stderr,"LIBRARY_ERROR %s\n",error.description.UTF8String);return 4;}
    id<MTLFunction> fn=[lib newFunctionWithName:@"resolve_depth"];
    id<MTLComputePipelineState> pipeline=[device newComputePipelineStateWithFunction:fn error:&error];
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
    id<MTLComputeCommandEncoder> compute=[commands computeCommandEncoder];
    [compute setComputePipelineState:pipeline];
    [compute setTexture:source atIndex:0];[compute setTexture:target atIndex:1];
    [compute dispatchThreads:MTLSizeMake(8,8,1) threadsPerThreadgroup:MTLSizeMake(8,8,1)];
    [compute endEncoding];[commands commit];[commands waitUntilCompleted];
    if (commands.status!=MTLCommandBufferStatusCompleted) {
      fprintf(stderr,"COMMAND_ERROR %s\n",commands.error.description.UTF8String);return 6;
    }
    float pixels[64]={};
    [target getBytes:pixels bytesPerRow:8*sizeof(float) fromRegion:MTLRegionMake2D(0,0,8,8) mipmapLevel:0];
    // Average must differ from sample0/min=0.1 and max=0.4.
    for (int i=0;i<64;++i) if (std::fabs(pixels[i]-0.25f)>1e-6f) {
      fprintf(stderr,"READBACK_MISMATCH pixel=%d value=%g\n",i,pixels[i]);return 7;
    }
    printf("DEPTH_FLOAT_READ_PASSED samples=4 pixels=64 depth=%g source_format=%lu target_format=%lu\n",
           pixels[0],(unsigned long)source.pixelFormat,(unsigned long)target.pixelFormat);
    return 0;
  }
}
