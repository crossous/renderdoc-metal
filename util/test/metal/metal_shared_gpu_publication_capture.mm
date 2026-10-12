// SPDX-License-Identifier: MIT
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include "renderdoc/api/app/renderdoc_app.h"
#include <dlfcn.h>
#include <cstdio>
#include <vector>
#include <cstdlib>
#include <cstring>
int main(int argc,char **argv){if(argc!=4)return 1;@autoreleasepool{
 auto d=MTLCreateSystemDefaultDevice();NSError *e=nil;
 auto l=[d newLibraryWithSource:[NSString stringWithContentsOfFile:[NSString stringWithUTF8String:argv[1]] encoding:NSUTF8StringEncoding error:&e] options:nil error:&e];
 auto a=[d newComputePipelineStateWithFunction:[l newFunctionWithName:@"produce"] error:&e];auto b=[d newComputePipelineStateWithFunction:[l newFunctionWithName:@"consume"] error:&e];if(!a||!b){fprintf(stderr,"%s\n",e.description.UTF8String);return 2;}
 const unsigned mode=getenv("TEST_PUBLICATION_CPU_MODE")?atoi(getenv("TEST_PUBLICATION_CPU_MODE")):0;const unsigned N=mode?4096:8192;std::vector<unsigned> zero(N,0);unsigned z=0;
 auto count=[d newBufferWithBytes:&z length:4 options:MTLResourceStorageModeShared];
 auto records=[d newBufferWithBytes:zero.data() length:N*4 options:MTLResourceStorageModeShared];
 auto map=[d newBufferWithLength:N*4 options:MTLResourceStorageModePrivate];
 auto out=[d newBufferWithBytes:zero.data() length:N*4 options:MTLResourceStorageModeShared];out.label=@"Shared publication invariant";records.label=@"GPU only shared records";
 id<MTLBuffer> salt;void *allocation=nullptr;unsigned *cpu=nullptr;
 if(mode==2){if(posix_memalign(&allocation,16384,16384))return 10;memset(allocation,0,16384);((unsigned*)allocation)[0]=1;salt=[d newBufferWithBytesNoCopy:allocation length:16384 options:MTLResourceStorageModeShared deallocator:nil];cpu=(unsigned*)allocation;}
 else {unsigned one=1;salt=[d newBufferWithBytes:&one length:4 options:MTLResourceStorageModeShared];if(mode==1)cpu=(unsigned*)salt.contents;}
 auto get=(pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT,"RENDERDOC_GetAPI");RENDERDOC_API_1_7_0 *api=nullptr;if(get&&get(eRENDERDOC_API_Version_1_7_0,(void **)&api)!=1)return 3;
 if(api){RENDERDOC_AnnotationValue v={};v.uint32=67;if(api->SetObjectAnnotation((__bridge void *)d,(__bridge void *)d,"metal.descriptorCoverage",eRENDERDOC_UInt32,0,&v))return 4;api->SetCaptureFilePathTemplate(argv[2]);api->StartFrameCapture(nullptr,nullptr);}
 auto q=[d newCommandQueue];auto c=[q commandBuffer];auto x=[c computeCommandEncoder];[x setComputePipelineState:a];[x setBuffer:count offset:0 atIndex:0];[x setBuffer:records offset:0 atIndex:1];[x setBuffer:map offset:0 atIndex:2];[x setBuffer:salt offset:0 atIndex:4];[x dispatchThreadgroups:MTLSizeMake(N/32,1,1) threadsPerThreadgroup:MTLSizeMake(32,1,1)];[x endEncoding];[c commit];[c waitUntilCompleted];if(c.status!=MTLCommandBufferStatusCompleted||c.error)return 5;
 // No CPU read or write of records/map; an independent mapped input may change.
 if(cpu)*cpu=17;
 c=[q commandBuffer];x=[c computeCommandEncoder];[x setComputePipelineState:b];[x setBuffer:records offset:0 atIndex:1];[x setBuffer:map offset:0 atIndex:2];[x setBuffer:out offset:0 atIndex:3];[x setBuffer:salt offset:0 atIndex:4];[x dispatchThreadgroups:MTLSizeMake(N/32,1,1) threadsPerThreadgroup:MTLSizeMake(32,1,1)];[x endEncoding];[c commit];[c waitUntilCompleted];if(c.status!=MTLCommandBufferStatusCompleted||c.error)return 5;
 if(api&&!api->EndFrameCapture(nullptr,nullptr))return 6;
 unsigned bad=0;for(unsigned i=0;i<N;i++)bad+=((unsigned*)out.contents)[i]!=(mode?17u:1u);printf("Native invariant bad=%u/%u\n",bad,N);if(bad)return 7;auto f=fopen(argv[3],"wb");if(!f)return 8;bool saved=fwrite(out.contents,1,N*4,f)==N*4;fclose(f);return saved?0:8;
}}
