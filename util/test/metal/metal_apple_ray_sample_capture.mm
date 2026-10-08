// SPDX-License-Identifier: MIT
// Builds against the unmodified, MIT-licensed Apple sample's Renderer/Scene/Transforms.
#import "Renderer.h"
#import <objc/runtime.h>
#import <QuartzCore/CAMetalLayer.h>
#include "renderdoc/api/app/renderdoc_app.h"
#include <dlfcn.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static NSString *libraryPath;

// Renderer only asks its view for currentDrawable. Supply an offscreen layer
// without opening a window, so controlled capture has a real presentation target.
@interface HeadlessAppleView : NSObject
@property(nonatomic, strong) id<CAMetalDrawable> currentDrawable;
@end
@implementation HeadlessAppleView
@end

// An explicit library path makes the official renderer usable without an application bundle.
@interface HeadlessAppleRenderer : Renderer
@end
@implementation HeadlessAppleRenderer
- (void)loadMetal
{
  id<MTLDevice> device = [self valueForKey:@"device"];
  NSError *error = nil;
  id<MTLLibrary> library = [device newLibraryWithURL:[NSURL fileURLWithPath:libraryPath]
                                             error:&error];
  if(!library) { fprintf(stderr, "Library: %s\n", error.description.UTF8String); exit(2); }
  [self setValue:library forKey:@"library"];
  [self setValue:[device newCommandQueue] forKey:@"queue"];
}
@end

static bool Drain(id<MTLCommandQueue> queue)
{
  id<MTLCommandBuffer> command = [queue commandBuffer];
  [command commit];
  [command waitUntilCompleted];
  return command.status == MTLCommandBufferStatusCompleted && !command.error;
}

int main(int argc, char **argv)
{
  if((argc != 5 && argc != 6) || (argc == 6 && strcmp(argv[5], "binding-variant")) || (strcmp(argv[3], "triangles") && strcmp(argv[3], "procedural")) ||
     (strcmp(argv[4], "native") && strcmp(argv[4], "capture-probe") &&
      strcmp(argv[4], "capture-production")))
  {
    fprintf(stderr, "Usage: runner metallib output-prefix triangles|procedural native|capture-probe|capture-production [binding-variant]\n");
    return 1;
  }
  @autoreleasepool
  {
    const bool variant = argc == 6;
    const unsigned width = variant ? 80 : 64, height = variant ? 48 : 64;
    const unsigned seed = variant ? 7 : 1;
    const bool probe = !strcmp(argv[4], "capture-probe");
    const bool capture = strcmp(argv[4], "native") != 0;
    RENDERDOC_API_1_7_0 *api = nullptr;
    auto get = (pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT, "RENDERDOC_GetAPI");
    if(get) get(eRENDERDOC_API_Version_1_7_0, (void **)&api);
    if(capture != (api != nullptr))
    { fprintf(stderr, "Capture requires injection; native requires no injection\n"); return 2; }
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    if(!device) return 2;
    fprintf(stdout, "device=%s published-raytracing=%d mode=%s frames=4 seed=%u size=%ux%u nullable-variant=%d\n",
            device.name.UTF8String, device.supportsRaytracing, argv[3], seed, width, height, variant);
    fflush(stdout);
    // The development probe calls existing interfaces directly. It never changes the device flag.
    if(!probe && !device.supportsRaytracing) return 3;
    libraryPath = [NSString stringWithUTF8String:argv[1]];
    srand(seed);
    Scene *scene = [Scene newInstancedCornellBoxSceneWithDevice:device
                                    useIntersectionFunctions:!strcmp(argv[3], "procedural")];
    fprintf(stdout, "geometries=%lu instances=%lu\n",
            (unsigned long)scene.geometries.count, (unsigned long)scene.instances.count);
    fflush(stdout);
    Renderer *renderer = [[HeadlessAppleRenderer alloc] initWithDevice:device scene:scene];
    HeadlessAppleView *view = [HeadlessAppleView new];
    [renderer mtkView:(MTKView *)view drawableSizeWillChange:CGSizeMake(width, height)];
    CAMetalLayer *layer = [CAMetalLayer layer];
    layer.device = device;
    layer.pixelFormat = MTLPixelFormatRGBA16Float;
    layer.drawableSize = CGSizeMake(width, height);
    layer.framebufferOnly = NO;
    id<MTLCommandQueue> queue = [renderer valueForKey:@"queue"];
    if(!Drain(queue)) return 4;
    for(unsigned frame = 0; frame < 4; frame++)
    {
      if(frame == 3 && api)
      {
        api->SetCaptureFilePathTemplate(argv[2]);
        api->StartFrameCapture(nullptr, nullptr);
      }
      if(frame == 3)
      {
        view.currentDrawable = [layer nextDrawable];
        if(!view.currentDrawable) return 4;
      }
      if(frame == 3 && variant)
      {
        // Legal explicit unbinds in an independent API command buffer. They do
        // not participate in the official renderer's shader bindings. Capture
        // must retain the nil distinction for both scalar and array APIs.
        id<MTLCommandBuffer> unbind = [queue commandBuffer];
        id<MTLComputeCommandEncoder> encoder = [unbind computeCommandEncoder];
        [encoder setAccelerationStructure:nil atBufferIndex:23];
        [encoder setVisibleFunctionTable:nil atBufferIndex:13];
        [encoder setIntersectionFunctionTable:nil atBufferIndex:21];
        id<MTLVisibleFunctionTable> visible[2] = {nil, nil};
        id<MTLIntersectionFunctionTable> intersection[2] = {nil, nil};
        [encoder setVisibleFunctionTables:visible withBufferRange:NSMakeRange(19, 2)];
        [encoder setIntersectionFunctionTables:intersection withBufferRange:NSMakeRange(26, 2)];
        [encoder endEncoding]; [unbind commit]; [unbind waitUntilCompleted];
        if(unbind.status != MTLCommandBufferStatusCompleted || unbind.error) return 4;
      }
      [renderer drawInMTKView:(MTKView *)view];
      if(!Drain(queue)) return 4;
    }
    // Read the renderer's swapped destination texture without modifying the official sources.
    Ivar targets = class_getInstanceVariable([Renderer class], "_accumulationTargets");
    if(!targets) return 5;
    void *texturePointer = nullptr;
    memcpy(&texturePointer, (char *)(__bridge void *)renderer + ivar_getOffset(targets),
           sizeof(texturePointer));
    id<MTLTexture> texture = (__bridge id<MTLTexture>)texturePointer;
    texture.label = @"Apple ray sample output";
    const size_t rowBytes = width * 4 * sizeof(float), byteCount = rowBytes * height;
    id<MTLBuffer> output = [device newBufferWithLength:byteCount options:MTLResourceStorageModeShared];
    output.label = @"Apple ray sample readback";
    id<MTLCommandBuffer> command = [queue commandBuffer];
    command.label = @"Apple ray sample readback command";
    id<MTLBlitCommandEncoder> blit = [command blitCommandEncoder];
    [blit copyFromTexture:texture sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0)
              sourceSize:MTLSizeMake(width,height,1) toBuffer:output destinationOffset:0
   destinationBytesPerRow:rowBytes destinationBytesPerImage:byteCount];
    [blit endEncoding];
    [command commit];
    [command waitUntilCompleted];
    if(command.status != MTLCommandBufferStatusCompleted || command.error) return 6;
    const float *pixels = (const float *)output.contents;
    double energy = 0;
    for(size_t i = 0; i < byteCount / sizeof(float); i++)
    {
      if(!std::isfinite(pixels[i]) || pixels[i] < 0) return 7;
      if(i % 4 != 3) energy += pixels[i];
    }
    if(energy <= 0) return 7;
    NSString *result = [[NSString stringWithUTF8String:argv[2]] stringByAppendingString:@".rgba32f"];
    if(![[NSData dataWithBytes:output.contents length:byteCount] writeToFile:result atomically:YES]) return 8;
    if(api && !api->EndFrameCapture(nullptr, nullptr)) return 9;
    fprintf(stdout, "PASS finite nonnegative ray output energy=%.9g bytes=%zu\n", energy, byteCount);
    return 0;
  }
}
