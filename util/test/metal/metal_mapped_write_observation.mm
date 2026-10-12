// SPDX-License-Identifier: MIT
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include "renderdoc/api/app/renderdoc_app.h"
#include <dlfcn.h>
#include <cstdio>
#include <vector>
#include <cstdlib>
#include <cstring>
#include <signal.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cstdint>
#include "renderdoc/driver/metal/metal_cpu_write_watch.h"
static volatile sig_atomic_t priorCalls=0,replacementCalls=0;
static void previousFault(int){priorCalls++;}
static void replacementFault(int){replacementCalls++;}

int main(int argc,char **argv){if(argc!=4)return 1;@autoreleasepool{
 auto d=MTLCreateSystemDefaultDevice();NSError *e=nil;
 auto l=[d newLibraryWithSource:[NSString stringWithContentsOfFile:[NSString stringWithUTF8String:argv[1]] encoding:NSUTF8StringEncoding error:&e] options:nil error:&e];
 auto a=[d newComputePipelineStateWithFunction:[l newFunctionWithName:@"produce"] error:&e];auto b=[d newComputePipelineStateWithFunction:[l newFunctionWithName:@"consume"] error:&e];if(!a||!b){fprintf(stderr,"%s\n",e.description.UTF8String);return 2;}
 const unsigned mode=getenv("TEST_PUBLICATION_CPU_MODE")?atoi(getenv("TEST_PUBLICATION_CPU_MODE")):0;const unsigned N=8192;std::vector<unsigned> zero(N,0);unsigned z=0;
 auto count=[d newBufferWithBytes:&z length:4 options:MTLResourceStorageModeShared];
 auto hd=[MTLHeapDescriptor new];hd.storageMode=MTLStorageModeShared;hd.type=MTLHeapTypePlacement;auto layout=[d heapBufferSizeAndAlignWithLength:N*4 options:MTLResourceStorageModeShared];hd.size=layout.size+layout.align;auto heap=[d newHeapWithDescriptor:hd];if(!heap)return 12;auto records=[heap newBufferWithLength:N*4 options:MTLResourceStorageModeShared offset:0];if(!records)return 13;
 auto map=[d newBufferWithLength:N*4 options:MTLResourceStorageModePrivate];
 auto out=[d newBufferWithBytes:zero.data() length:N*4 options:MTLResourceStorageModeShared];out.label=@"Shared publication invariant";records.label=@"GPU only shared records";
 id<MTLBuffer> alias=nil;unsigned *aliasCPU=nullptr;if(mode==2){alias=[heap newBufferWithLength:N*4 options:MTLResourceStorageModeShared offset:0];if(!alias)return 15;aliasCPU=(unsigned*)alias.contents;}
 id<MTLBuffer> salt;void *allocation=nullptr;unsigned *cpu=nullptr;
 if(mode==8){if(posix_memalign(&allocation,16384,16384))return 10;memset(allocation,0,16384);((unsigned*)allocation)[0]=1;salt=[d newBufferWithBytesNoCopy:allocation length:16384 options:MTLResourceStorageModeShared deallocator:nil];cpu=(unsigned*)allocation;}
 else {unsigned one=1;salt=[d newBufferWithBytes:&one length:4 options:MTLResourceStorageModeShared];}
 auto get=(pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT,"RENDERDOC_GetAPI");RENDERDOC_API_1_7_0 *api=nullptr;if(get&&get(eRENDERDOC_API_Version_1_7_0,(void **)&api)!=1)return 3;
 auto initialQueue=[d newCommandQueue];auto initial=[initialQueue commandBuffer];auto initBlit=[initial blitCommandEncoder];[initBlit fillBuffer:records range:NSMakeRange(0,N*4) value:0];[initBlit endEncoding];[initial commit];[initial waitUntilCompleted];if(initial.status!=MTLCommandBufferStatusCompleted||initial.error)return 14;
 unsigned *watched=(unsigned*)records.contents;
 int owner=0;auto &observer=MetalDiagnosticCPUWriteWatch::Get();
 struct sigaction previous={};previous.sa_handler=previousFault;sigemptyset(&previous.sa_mask);struct sigaction original={};sigaction(SIGSEGV,&previous,&original);
 if(!observer.Begin(&owner))return 16;
 void *retained=(__bridge void*)records;CFRetain((__bridge CFTypeRef)records);
 if(!observer.Track(&owner,"Native-records",retained,[](void *p){CFRelease((CFTypeRef)p);},watched,N*4))return 17;
 raise(SIGSEGV);if(priorCalls!=1)return 18;

 if(api){RENDERDOC_AnnotationValue v={};v.uint32=67;if(api->SetObjectAnnotation((__bridge void *)d,(__bridge void *)d,"metal.descriptorCoverage",eRENDERDOC_UInt32,0,&v))return 4;api->SetCaptureFilePathTemplate(argv[2]);api->StartFrameCapture(nullptr,nullptr);}
 auto q=[d newCommandQueue];auto c=[q commandBuffer];auto x=[c computeCommandEncoder];[x setComputePipelineState:a];[x setBuffer:count offset:0 atIndex:0];[x setBuffer:records offset:0 atIndex:1];[x setBuffer:map offset:0 atIndex:2];[x setBuffer:salt offset:0 atIndex:4];[x dispatchThreadgroups:MTLSizeMake(N/32,1,1) threadsPerThreadgroup:MTLSizeMake(32,1,1)];[x endEncoding];[c commit];[c waitUntilCompleted];if(c.status!=MTLCommandBufferStatusCompleted||c.error)return 5;
 // No CPU read or write of records/map; an independent mapped input may change.
 observer.Report(&owner,"Native-after-GPU-producer",watched,0,N*4,false);
 if(mode==1){unsigned value=watched[0];(void)value;}
 if(aliasCPU)for(unsigned i=0;i<N;i++)aliasCPU[i]=0xffffffffu;
 observer.Report(&owner,"Native-after-CPU-read-or-alias-write",watched,0,N*4,false);
 c=[q commandBuffer];x=[c computeCommandEncoder];[x setComputePipelineState:b];[x setBuffer:records offset:0 atIndex:1];[x setBuffer:map offset:0 atIndex:2];[x setBuffer:out offset:0 atIndex:3];[x setBuffer:salt offset:0 atIndex:4];[x dispatchThreadgroups:MTLSizeMake(N/32,1,1) threadsPerThreadgroup:MTLSizeMake(32,1,1)];[x endEncoding];[c commit];[c waitUntilCompleted];if(c.status!=MTLCommandBufferStatusCompleted||c.error)return 5;
 observer.Report(&owner,"Native-after-GPU-consumer",watched,0,N*4,false);
 if(mode==1){struct sigaction replacement={};replacement.sa_handler=replacementFault;sigemptyset(&replacement.sa_mask);sigaction(SIGSEGV,&replacement,nullptr);}
 observer.End(&owner);
 watched[0]=123; // All owned protections have been restored.
 if(mode==1){raise(SIGSEGV);if(replacementCalls!=1)return 20;}else{raise(SIGSEGV);if(priorCalls!=2)return 21;}
 sigaction(SIGSEGV,&original,nullptr);
 // Re-arm the same physical pages; stale dirty history must not survive a scope.
 if(!observer.Begin(&owner))return 22;
 CFRetain((__bridge CFTypeRef)records);if(!observer.Track(&owner,"Native-records-second-scope",retained,[](void *p){CFRelease((CFTypeRef)p);},watched,N*4))return 23;
 observer.Report(&owner,"Native-second-scope",watched,0,N*4,false);observer.End(&owner);

 if(api&&!api->EndFrameCapture(nullptr,nullptr))return 6;
 unsigned bad=0;for(unsigned i=0;i<N;i++)bad+=((unsigned*)out.contents)[i]!=(mode==2?0u:1u);printf("Native invariant bad=%u/%u observer mode=%u\n",bad,N,mode);if(bad)return 7;auto f=fopen(argv[3],"wb");if(!f)return 8;bool saved=fwrite(out.contents,1,N*4,f)==N*4;fclose(f);return saved?0:8;
}}
