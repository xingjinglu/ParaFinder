//===- LDDDAShadowRT.cpp - LDDDA based Shadow  --*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// \file  
// \brief This file defines main funnctions called by DDDA in runtime, which is 
// based Shadow memory technique.
//
//
//===----------------------------------------------------------------------===//
//

#include "LDDShadowCommonRT.h"
#include "LDDLightShadowRT.h"
#include "rdtscc.h"

#ifdef _DDA_PA
#include "LDDParallelAnalysis.h"
#endif

#include <fcntl.h> 
#include <sched.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <sys/mman.h>

#include<fstream>
#include<iostream>
#include<iomanip>
#include<unordered_set>

#include<stdio.h>
#include<stdlib.h>

#include"RedBlackTree.h"
#include  "stl_interval_set.h"

//
//using namespace llvm;

// Commnunicate with instrumentation.
// Labeled entering the profiling scope and current prof loop ID. 
 int LDDProfLoopID = 0; 
 int LDDProfLoopIter = 0; // The iteration of loop.
 int LDDProfCurIter = 0;

//
//LSDepInfo LDDLSResult;
//std::set<long> LDDRWTrace;
std::set<long> LDDLSResultTrace; // The pointer to Trace, not the shadow addr.

RBTree *LDDRWTrace;
LSPresentRWAddrID *LSPresentRWBuf; 

// Get the address scope of each section: global(0x7cf000000000 ,0x7cf0,0040,0000), 
// heap(0x7cf000400000 , 0x7fff,0000,0000 ), stack(0x7fff00000000,0x7fffffffffff ), 
//  Not thread safe.
long LDDStackMax, LDDStackMin, LDDHeapMax, LDDHeapMin, LDDGlobalMax, LDDGlobalMin;
long LSLIDepSinkPCMax, LSLIDepSinkPCMin, LSLCDepSinkPCMax, LSLCDepSinkPCMin;
long LDDLSResultMax, LDDLSResultMin;

struct PageVal{
  long Num;
};

namespace std{

  template <>
    struct hash<PageVal>{
      std::size_t operator()(const PageVal &t) const{
        std::size_t val = t.Num % 10000;                                                                                                                                    
      }
    };
}

std::unordered_set<long> ShdwPageSet(1000);
// Cache buffer to optmized ShdwPageSet performance.
long CacheBuf[CBUFLEN];
int CacheBufNum;
int TotalPageNum;


// Record memory scope accessed by the app.
long ShadowLowBound, ShadowUpBound;

int isStackVar = 0;

// == 1 for single loop; == LDDProfLoopID for multi-loop prof..
int CurLoopID = 1; //
// == 1 for Serial version; == LDDProfLoopIter for par version.
//int CurLoopIter = 1; // Not used now.

#ifdef _OVERHEAD_PROF
//std::interval_set<long>  AddrIntervalSet(8);
std::set<long> AddrSet;
long AddrSetNum;
std::set<long>PageSet;
#endif

#ifdef _OVERHEAD_PROF
long LSPresentTraceMallocTime, LSPrecedeTraceMallocTime,  LSPresentBufFreeTime,LSBufReuseTime; 
long LSAddRWToPrecedeTraceTime;
long LCDepInfoMallocTime, LCDepMallocTime, LCDepDistMallocTime,  LIDepMallocTime, LIDepInfoMallocTime;
long LSAddrShadowTime, LSPCShadowTime;
unsigned long OFSCheckLoad, OFECheckLoad, OFSCheckStore, OFECheckStore;
unsigned long TotalUpWrite, NWTotalUpWrite ;

//
long TraceByteNum, TraceRangeNum, TracePCNum, LCDepNum, LIDepNum, DepRangeNum;
#endif


#undef __ADDRDEF

#if 0
// Not used now.
void __initPlatform()
{
#if 0
  void *p = 0;
  if( sizeof(p) == 8 ){
    volatile rlimit lim;
    lim.rlim_cur = 0;
    lim.rlim_max = 0;
    setlimit(RLIMIT_CORE, (rlimit*)&lim);

  }
#endif
  // If stack size is set unlimited, set a limited size for it.
  // Todo..
  return;
}


// Not used now.
void __die()
{
  // Add some call back function specified by _initLDDALightShadow
  _exit(1);

}


void * __internal_mmap(void *addr, uptr length, int prot, int flags,
                        int fd, u64 offset)
{
  // This is Linux 64-bit specific.
  return (void*) syscall(__NR_mmap, addr, length, prot, flags, fd, offset);

}



int __internal_munmap(void *addr, uptr length)
{
  // This is Linux 64-bit specific.
  return syscall( __NR_munmap, addr, length );
}


int __internal_close(fd_t fd)
{
  return syscall( __NR_close, fd);
}



void * __mmapFixedReserveVMA(uptr fixedAddr, uptr size)
{
  return __internal_mmap( (void*)fixedAddr, size, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANON | MAP_FIXED | MAP_NORESERVE,
                        -1, 0);

}
#endif

void __initLightShadowMemory()
{

  std::cout<< std::setbase(16)  << "InitShadowBeg = "<< LinuxLShadowBeg <<" , InitShadowEnd = " 
          << LinuxLShadowEnd << "\n";
  uptr Shadow = (uptr) __mmapFixedReserveVMA( LinuxLShadowBeg, LinuxLShadowEnd - LinuxLShadowBeg );
  if( Shadow != LinuxLShadowBeg ){
    std::cout << "Error: LDDDALShadow cannot mmap the shadow memory \n";
    std::cout << "Error: Make sure to compile with -fPIE and  "
              << "to link with -pie \n";

    //__die();
    _exit(1);
  }

}


// Reserve virtual memory space for shadow.
//
//
int global;
extern char etext, edata, end;
void __initLDDLightShadow()
{
  static bool is_initialized = false; 

  if( is_initialized )
    return;
#if defined(_OVERHEAD_PROF) && defined(_DDA_PA)
  MasterThrStart = rdtsc();
#endif

  is_initialized = true;

  // Not used now.
  //__initPlatform();

  __initLightShadowMemory();

  // Init the label of memory scope. 
  //AppBeg = LinuxLShadowAppEnd;
  //AppEnd = LinuxLShadowAppBeg;

  ShadowLowBound = LinuxLShadowEnd; // Max
  ShadowUpBound = LinuxLShadowBeg; // Min

  // 20140108
  // init for global variables: LDDRWTrace, LDDLSResultTrace.
  LDDRWTrace = RBTreeInit( LDDRWTrace );

  LSPresentRWBuf = NULL;

  printf("etext = %p, edata = %p, end = %p global = %p\n", &etext, &edata, &end, &global);

// Get the address scope of each section: global(0x7cf000000000 ,0x7cf0,0040,0000), 
// heap(0x7cf000400000 , 0x7fff,0000,0000 ), stack(0x7fff00000000,0x7fffffffffff ), 
/*
 *    |             |
 *    |             | high address.
 *    |-------------| 
 *    |             | heap 
 *    |             |   v
 *    |             |   
 *    |             |   ^ 
 *    |             | stack
 *    |-------------|
 *    |             | .bss
 *    |-------------|
 *    |             | .data
 *    |-------------|
 *    |             | .text
 *    |-------------| 
 *    |             | low address.
 *    |             | 
 *
 *
 */
  LDDGlobalMax = 0x6cf000000000;
  LDDGlobalMin = 0x6cf000400000;
  LDDStackMin = 0x6fffffffffff;
  LDDStackMax = 0x6fff00000000;
 // GlobalMax<= <= StackMin
  LDDHeapMin = 0x6fff00000000;
  LDDHeapMax = 0x6cf000400000;

// 
// Get the PC scope that has LI/LC data dependence.
  LDDLSResultMax = 0x6cf000000000;
  LDDLSResultMin = LDDGlobalMin;

#ifdef _DDA_PA
  LSLIDepSinkPCMax = 0x6cf000000000;
  LSLIDepSinkPCMin = LDDGlobalMin;

  LSLCDepSinkPCMax = 0x6cf000000000;
  LSLCDepSinkPCMin = LDDGlobalMin;

  LSOutputLCDepFinish = 0;
#endif

#ifdef _OVERHEAD_PROF
  LSAddrShadowTime = 0; 
  LSPrecedeTraceMallocTime = 0;
  LSPresentTraceMallocTime = 0;

  LSBufReuseTime = 0;
  LSPresentBufFreeTime = 0;
  LSAddRWToPrecedeTraceTime = 0;

  LSPCShadowTime = 0;
  LCDepInfoMallocTime = 0;
  LCDepMallocTime = 0;
  LIDepInfoMallocTime = 0;
  LIDepMallocTime = 0;

  LCDepDistMallocTime = 0;

  TraceByteNum = 0;
  TraceRangeNum = 0;
  TracePCNum =0;

  LCDepNum = 0;
  LIDepNum = 0;
  DepRangeNum = 0;
  
#endif
#if _DDA_PA
  LDDPASetUpPiepline( );
#endif
  return ;
}

void __finiLightShadowMemory()
{
  __internal_munmap( (void*) LinuxLShadowBeg, 
                    LinuxLShadowEnd - LinuxLShadowBeg);
 
  return ;
}

void __finiLDDLightShadow()
{
  __finiLightShadowMemory();

  return;

}


#if 0
inline bool operator< (const LSPrecedeRWAddrID & Lhs, const LSPrecedeRWAddrID & Rhs)
{
  return ( (Lhs.PC <  Rhs.PC) || ((char)Lhs.RWMask <  (char)Rhs.RWMask) );
}

inline bool operator< (const LSDep & Lhs, const LSDep & Rhs)
{
  return ( (Lhs.SrcPC <  Rhs.SrcPC) || (Lhs.SinkPC <  Rhs.SinkPC) );
}
#endif


int __maskToByteNum( unsigned char mask )
{
  int Num =0;
  while( mask > 0 ){
    if( mask & 1  ){
      Num++;
    }
    mask = mask >> 1;
  }
  return Num;
}

//
//
//
//inline
int __determinInner8bytesInfo(void *ptr, long AddrLen, 
    //long *AddrInnerOffset,
    //long *Len1, 
    unsigned char* ReadMask1, 
    //long *Len2, 
    unsigned char * ReadMask2 )
{
  long AddrInnerOffset = (long) ptr & 0x0000000000000007;
  long Len1, Len2, Len3;
  Len3 = AddrInnerOffset + AddrLen; 
  if( Len3 <= 8 ) {
    //Len1 =  AddrLen;
    *ReadMask1 = (1<<Len3) - (1<<AddrInnerOffset); 
    //printf( "Offset = %d, AddrLen = %d, ReadMask1 =  %u \n", *AddrInnerOffset, AddrLen, *ReadMask1);
    return 1;
  }else{
    //Len1 =   8 - AddrInnerOffset;
    *ReadMask1 = (1<<(8)) - (1<<AddrInnerOffset); 
    //Len2 = AddrLen - Len1;
    Len2 = Len3 - 8;
    *ReadMask2 = (1<< Len2) - 1;   // bug?
    //printf("Inner = %d, AddrLen = %p, ReadMask1 =  %u , ReadMask2 = %u \n", *AddrInnerOffset, AddrLen, *ReadMask1, *ReadMask2);
    return 2;
  }

}

#ifndef _DDA_PA
//
// We only record the new-read bytes that are not read  and written before. 
//
inline 
void __updatePresentRTrace(void *PC, void **ShadowAddr, unsigned char *ReadMask,
                            LSPresentRWTrace &PRWTraceRef, int WTEmptyFlag )
{

// printf("insert load addr = %p pc = %p\n",ShadowAddr, (void*) PC );
  unsigned char NewReadMask;
  LSPresentRWAddrID *RIDPtr;

  // No Write before within cur iteration.
  unsigned char FirstNewReadMask;
  unsigned char OldGlobalReadMask = PRWTraceRef.GlobalReadMask; 
  unsigned char NewGlobalReadMask = OldGlobalReadMask | *ReadMask;

  // 1) Having New-Read bytes.
  if( ( NewReadMask = (NewGlobalReadMask ^ OldGlobalReadMask) ) ) {
    // 2) The New-Read bytes have not been written before.
    if( (FirstNewReadMask = (NewReadMask & (NewReadMask ^ PRWTraceRef.GlobalWriteMask))) ){

    //3) Get the RWTrace buf to insert the New-Read-Not-Written bytes. 
    // 3.1) There are free RWTrace buf. NewBuf->next = RTrace; RTrace = NewBuf;
      if( LSPresentRWBuf != NULL ){
        LSPresentRWAddrID *NextBuf = LSPresentRWBuf->Next;
        LSPresentRWBuf->Next = PRWTraceRef.RTrace;
        PRWTraceRef.RTrace = LSPresentRWBuf; 
        RIDPtr = LSPresentRWBuf;
        LSPresentRWBuf = NextBuf;
#ifdef _OVERHEAD_PROF
        LSBufReuseTime++;
#endif
      }
    // 3.2) No free RWTrace buf.
      else{
#ifdef _OVERHEAD_PROF
        LSPresentTraceMallocTime++;
#endif
        RIDPtr = (LSPresentRWAddrID*) malloc(sizeof(LSPresentRWAddrID));
        RIDPtr->Next = PRWTraceRef.RTrace;
        PRWTraceRef.RTrace = RIDPtr;
      }      
    // 4) Fill content.
    #ifdef _OVERHEAD_PROF
      TracePCNum++;
      TraceByteNum += __maskToByteNum(FirstNewReadMask);            
    #endif
      RIDPtr->PC = (long)PC;
      RIDPtr->RWMask = FirstNewReadMask;
      PRWTraceRef.GlobalReadMask = NewGlobalReadMask;
    }
  }

  return;
}

// Address as Key.
// Todo:
// 1) Addr1: W R R R R ===> not need to check True-dep every time.
// LITD.
#ifdef __ADDRKEY
inline void __doReadLIDWithAddrAsKey(void * PC, unsigned char *ReadMask,
    LSPresentRWTrace &PRWTraceRef, Trace &TraceRef)
{
  unsigned char RWMask;

  if( (RWMask = (*ReadMask & PRWTraceRef.GlobalWriteMask)) ) {
    int EleNum = 0, EleTotal = PRWTraceRef.WriteNum;
    LSPresentRWAddrID * WIDPtr = PRWTraceRef.WTrace;

    unsigned char NewRWMask;
    for( ; EleNum < EleTotal && RWMask; ++EleNum, WIDPtr = WIDPtr->Next ){
      NewRWMask = RWMask;
      RWMask &= RWMask ^ WIDPtr->RWMask;

      // LI True Dep?
      if( RWMask != NewRWMask ){
        // Opt me?

        LSLIDep *LIDepTrue = TraceRef.LIDep->TrueDep; 
        while( LIDepTrue != NULL ){
          if(LIDepTrue->SrcPC == WIDPtr->PC && LIDepTrue->SinkPC == (long)PC){
            break;
          }
          LIDepTrue = LIDepTrue->next;
        }
        // Not find in LID set, add the new LID-True.
        if( LIDepTrue == NULL ){
        #ifdef __OVERHEAD_PROF
        LIDepMallocTime++;
        #endif
          LIDepTrue = (LSLIDep*) malloc( sizeof(LSLIDep));
          LIDepTrue->next =  TraceRef.LIDep->TrueDep;
          TraceRef.LIDep->TrueDep = LIDepTrue; 
          LIDepTrue->SrcPC =(long) WIDPtr->PC;
          LIDepTrue->SinkPC =(long) PC;

          //LDDLSResultTrace.insert((long)&TraceRef);

        }
      }

    }
  }
  return;
}
#endif

/* Dep store with SinkPC as key.
 * LIDep and LCDep store in different struct.
 */
inline void __doReadLIDWithSinkPCAsKey(void * PC, unsigned char *ReadMask,
    LSPresentRWTrace &PRWTraceRef, Trace &TraceRef)
{
  unsigned char RWMask;
  printf("here __doReadLIDWithSinkPCAsKey \n");
  Deps **DepShadowPtr;
  Deps *DepsPtr;
  LSLIDepInfo *LIDepPtr;
  DepShadowPtr = (Deps**) AppAddrToShadow((long)PC);
  if( *DepShadowPtr == NULL ){
  #ifdef _OVERHEAD_PROF
  LSPCShadowTime++;  // x * ( 8 + );
  LIDepInfoMallocTime++;
  #endif
    printf("should not run there \n");
    DepsPtr = (Deps*) malloc(sizeof(Deps));
    *DepShadowPtr = DepsPtr;
    DepsPtr->LIDep = (LSLIDepInfo*) malloc(sizeof( LSLIDepInfo));
    DepsPtr->LIDep->TrueDep = NULL;
    DepsPtr->LIDep->OutDep = NULL;
    DepsPtr->LIDep->AntiDep = NULL;
  } 
  LIDepPtr = (*DepShadowPtr)->LIDep;

  if( (RWMask = (*ReadMask & PRWTraceRef.GlobalWriteMask)) ) {
    int EleNum = 0, EleTotal = 0; //PRWTraceRef.WriteNum;
    LSPresentRWAddrID * WIDPtr = PRWTraceRef.WTrace;

    unsigned char NewRWMask;
    for( ; (WIDPtr != NULL) && RWMask; WIDPtr = WIDPtr->Next ){
      NewRWMask = RWMask;
      RWMask &= RWMask ^ WIDPtr->RWMask;

      // LI True Dep?
      if( RWMask != NewRWMask ){
        // Opt me?

        LSLIDep *LIDepTrue = LIDepPtr->TrueDep; //TraceRef.LIDep->TrueDep; 
        while( LIDepTrue != NULL ){
          if(LIDepTrue->SrcPC == WIDPtr->PC && LIDepTrue->SinkPC == (long)PC){
            break;
          }
          LIDepTrue = LIDepTrue->next;
        }
        // Not find in LID set, add the new LID-True.
        if( LIDepTrue == NULL ){
        #ifdef _OVERHEAD_PROF
        LIDepMallocTime++;
        #endif
          LIDepTrue = (LSLIDep*) malloc( sizeof(LSLIDep));
          LIDepTrue->next =  LIDepPtr->TrueDep;
          LIDepPtr->TrueDep = LIDepTrue; 
          LIDepTrue->SrcPC =(long) WIDPtr->PC;
          LIDepTrue->SinkPC =(long) PC;
          LDDLSResultTrace.insert((long)&TraceRef);
        }
      }

    }
  }

  return ;
}

// 20140127.
// LIDep/LCDep store in the same struct with different Dist;
// Dist[0] = 0, keep the LID Dist whose value is 0.
//
inline void __doReadLIDWithSinkPCAsKeyLICDep(void * PC, unsigned char *ReadMask,
    LSPresentRWTrace &PRWTraceRef, Trace &TraceRef)
{
  unsigned char RWMask;

  LSDepInfo **DepShadowPtr;
  LSDepInfo *LIDepPtr;
  DepShadowPtr = (LSDepInfo**) AppAddrToShadow((long)PC);
  if( *DepShadowPtr == NULL ){
  #ifdef _OVERHEAD_PROF
  LSPCShadowTime++;  // x * ( 8 + );
  LIDepInfoMallocTime++;

  DepRangeNum++;
  #endif
    LIDepPtr = (LSDepInfo*) malloc(sizeof(LSDepInfo));
    *DepShadowPtr = LIDepPtr;
    LIDepPtr->TrueDep = NULL;
    LIDepPtr->OutDep = NULL;
    LIDepPtr->AntiDep = NULL;
//#ifdef _DDA_PA
#if 0
    if( (long)DepShadowPtr > LSLIDepSinkPCMax )
      LSLIDepSinkPCMax = (long)DepShadowPtr;
    else if( (long)DepShadowPtr < LSLIDepSinkPCMin )
      LSLIDepSinkPCMin =(long) DepShadowPtr;
#endif
    if( (long)DepShadowPtr > LDDLSResultMax )
      LDDLSResultMax = (long)DepShadowPtr;
    else if( (long)DepShadowPtr < LDDLSResultMin )
      LDDLSResultMin =(long) DepShadowPtr;

  } 
  else
    LIDepPtr = *DepShadowPtr;

  if( (RWMask = (*ReadMask & PRWTraceRef.GlobalWriteMask)) ) {
    LSPresentRWAddrID * WIDPtr = PRWTraceRef.WTrace;

    unsigned char NewRWMask;
    for( ; (WIDPtr != NULL) && RWMask; WIDPtr = WIDPtr->Next ){
      NewRWMask = RWMask;
      RWMask &= RWMask ^ WIDPtr->RWMask;

      // LI True Dep?
      if( RWMask != NewRWMask ){
        // Opt me?
        LSDep *LIDepTrue = LIDepPtr->TrueDep; //TraceRef.LIDep->TrueDep; 
        while( LIDepTrue != NULL ){
          if(LIDepTrue->SrcPC == WIDPtr->PC && LIDepTrue->SinkPC == (long)PC){
            if( LIDepTrue->Dist[0] != 0 )
#ifdef _OVERHEAD_PROF
  LIDepNum++; 
#endif
              LIDepTrue->Dist[0] = 0;
            break;
          }
          LIDepTrue = LIDepTrue->next;
        }
        // Not find in LID set, add the new LID-True.
        if( LIDepTrue == NULL ){
#ifdef _OVERHEAD_PROF
  LIDepMallocTime++;
  LIDepNum++; 
#endif
          LIDepTrue = (LSDep*) malloc( sizeof(LSDep));
          LIDepTrue->next =  LIDepPtr->TrueDep;
          LIDepPtr->TrueDep = LIDepTrue; 
          LIDepTrue->SrcPC =(long) WIDPtr->PC;
          LIDepTrue->SinkPC =(long) PC;
          LIDepTrue->Dist[0] = LIDepTrue->Dist[1] = 0;
        }
      }

    }
  }

  return ;
}


// Todo:
// 1) Addr1: W R R R R ===> not need to check True-dep every time.
// LITD.
//inline 
void __determinReadLID(void * PC, unsigned char *ReadMask,
    LSPresentRWTrace &PRWTraceRef, Trace &TraceRef)
{

  // 1)
  #ifdef __ADDRKEY
 __doReadLIDWithAddrAsKey( PC, ReadMask, PRWTraceRef, TraceRef);
  #endif
  // 2) 
 //__doReadLIDWithSinkPCAsKey( PC, ReadMask, PRWTraceRef, TraceRef);
 
  // 3) LI/LC Store together. 
 __doReadLIDWithSinkPCAsKeyLICDep( PC, ReadMask, PRWTraceRef, TraceRef);
  
  return;
}

// Present[1] when serial execution; Present[IterationNO] when parallel 
// execution. 
//void __checkLIDandUpdatePresentRTrace(void *PC, void **ShadowAddr, 
 //                    long* AddrInnerOffset, long* Len, unsigned char * ReadMask, int LoopIter)
void __checkLIDandUpdatePresentRTrace(void *PC, void **ShadowAddr, 
                     unsigned char * ReadMask, int LoopIter)
{
  //  printf("insert load addr = %p pc = %p\n",ShadowAddr, (void*) PC );
  // 1) Get the right RWTrace Buf.
  // 1.1) First access the address, no buffer for Trace.
  if( *ShadowAddr == NULL ){
    Trace *TracePtr;
    //printf("insert load addr = %p pc = %p\n",ShadowAddr, (void*) PC );
#ifdef _OVERHEAD_PROF
    LSAddrShadowTime++;

    TraceRangeNum++;
#endif
    TracePtr   = (Trace*) malloc( sizeof(Trace) );
    *ShadowAddr = TracePtr;

#if 0
#ifdef __ADDRKEY
    TracePtr->LCDep = (LSLCDepInfo*) malloc (sizeof(LSLCDepInfo) );
    TracePtr->LCDep->AntiDep = NULL;
    TracePtr->LCDep->OutDep = NULL;
    TracePtr->LCDep->TrueDep = NULL;

    TracePtr->LIDep = (LSLIDepInfo*) malloc (sizeof(LSLIDepInfo) );
    TracePtr->LIDep->AntiDep = NULL;
    TracePtr->LIDep->OutDep = NULL;
    TracePtr->LIDep->TrueDep = NULL;
#endif
#endif
    // bug?
    TracePtr->Precede  = (LSPrecedeRWTrace*) malloc(sizeof(LSPrecedeRWTrace) );
    TracePtr->Precede->RTrace = NULL;
    TracePtr->Precede->WTrace = NULL;
    TracePtr->Precede->GlobalReadMask = TracePtr->Precede->GlobalWriteMask = 0;

    TracePtr->Present = (LSPresentRWTrace*) malloc( sizeof(LSPresentRWTrace) );
    TracePtr->Present->WTrace = NULL;
    TracePtr->Present->RTrace = NULL;
    TracePtr->Present->GlobalWriteMask = 0;
    TracePtr->Present->GlobalReadMask = 0;
    TracePtr->Present->RWIter =  LoopIter; // bug?
  }


    LSPresentRWTrace & PRWTraceRef = *((Trace*)(*ShadowAddr))->Present;
    Trace  &TraceRef = *((Trace*)(*ShadowAddr));

    // 1.2) App has read/written the address before.  
    // 1.2.1) TraceRef.Present keeps the precede-iteraiton's trace.
    // The present buffer has been released after addReadToPrecedeTrace.
    if(  PRWTraceRef.RWIter != LoopIter )  {
      // Opt me?
      PRWTraceRef.RWIter = LoopIter;
      // Not delete the precede iterations's PresentWTrace buffer. free?
      PRWTraceRef.GlobalWriteMask =  0;
      PRWTraceRef.GlobalReadMask =  0; 
    }
    // 1.2.2) Read/Write in the same iteration. 
    // do-nothing.

    // 2) Do LID Analysis and update PresentRTrace table. Opt ?
  if( PRWTraceRef.GlobalWriteMask )
    __determinReadLID(PC, ReadMask, PRWTraceRef, TraceRef);

    // 3) Setup PresentTrace table.
    __updatePresentRTrace(PC, ShadowAddr, ReadMask, PRWTraceRef, 0);
    //__determinReadLID(PC, ReadMask, PRWTraceRef);

  return;
}

void __checkLIDandUpdatePresentRTrace2(void *pc, void** ShadowAddr,
                                       unsigned char * ReadMask1, 
                                       unsigned char *ReadMask2, int LoopIter)
{
  __checkLIDandUpdatePresentRTrace(pc, ShadowAddr,ReadMask1, LoopIter);
  //__updatepresenttraceread(pc, ShadowAddr, AddrInnerOffset, Len1, ReadMask1);
  ShadowAddr++; // Fix me?
  //__updatepresenttraceread(pc, ShadowAddr, 0, Len2, ReadMask2);
  __checkLIDandUpdatePresentRTrace(pc, ShadowAddr,ReadMask2, LoopIter);
}

#endif

//
//
//extern inline
//void __checkLoadLShadow(int *Ptr, long AddrLen);
//extern inline
void __checkLoadLShadow(int *Ptr, long AddrLen)
{
#ifdef _DDA_PP
  if( !LDDProfCurIter ) 
    return;
#else
  if( !LDDProfLoopID ) 
    return;
#endif
  void *pc;
  pc  = __builtin_return_address(0);  

#ifdef _DDA_PA
  GenerateAddrStream( Ptr, AddrLen , LDDProfLoopIter, 0, pc);
  return; // OPT-0-1

#ifndef _DDA_PP
  //int LoopIter = LDDProfLoopIter;  
  //short int Flag = 0;

#ifdef _TIME_PROF
  tbb::tick_count tstart, tend, pststart;
  pststart = tbb::tick_count::now();
#endif
  //std::cout<<"GeneratingAddrStream: LoopIter = " << LoopIter <<std::endl; 

  // case 1: A new iteration.
  // 1.1 Label the Streams[] is full;
  // 1.2 Sent StartLCDA = 1;
  // 1.3 Create new Streams[] to insert the new iteration's trace.
  if( LDDProfLoopIter != CurLoopIter){
    // The old stream is not full.
    if( WItemNum ){
      // Set the end flag of old stream.
      #ifndef _DDA_PP
      Streams[WStreamNum].LoopIter = CurLoopIter;
      Streams[WStreamNum].End = WItemNum;
      StreamIsFull[WStreamNum] = 1;
      #endif
#ifdef _DEBUGINFO
      std::cerr<<std::setbase(10)<<"GenerateAddrStream WStreamNum =  " << WStreamNum  << " is full"<< "\n";
#endif
      // 1.1) New stream to start LCDA. 
      WStreamNum = (WStreamNum+1) % STREAMNUM;
    }
    // else The old stream is empty;

    // Create the new Streams for keep LCDA info. 
#ifdef _TIME_PROF
    tstart = tbb::tick_count::now();
#endif
    while( StreamIsFull[WStreamNum] ) ;
#ifdef _TIME_PROF
    GPTime += (tbb::tick_count::now() - tstart).seconds();
#endif

#ifndef _DDA_PP
    Streams[WStreamNum].Items[0].RWFlag = 5; // To enable LCDA.  
    Streams[WStreamNum].LoopIter = CurLoopIter;
    StreamIsFull[WStreamNum] = 1;
#endif
    //std::cerr<<std::setbase(10)<<"GenerateAddrStream CurLoopIter =  " << CurLoopIter << ", LoopIter = "<< LDDProfLoopIter << " is full. doLCDA"<< "\n";
#ifdef _DEBUGINFO
    //std::cerr<<std::setbase(10)<<"GenerateAddrStream WStreamNum =  " << WStreamNum  << " is full. doLCDA"<< "\n";
#endif

    // 1.2) New stream to store RWTrace.
    WStreamNum = (WStreamNum+1) % STREAMNUM;
    WItemNum = 0;
    CurLoopIter = LDDProfLoopIter;
  }

  // Case2: put more data into the Streams[].
  //  Wait until the buf is not full. Need?
  // Case2.1: WItemNum == 0, insert the first element into the Streams[].
  if( !WItemNum  ){

#ifdef _TIME_PROF
    tstart = tbb::tick_count::now();
#endif
    while( StreamIsFull[WStreamNum] ) ; // Maybe not need?
#ifdef _TIME_PROF
    GPTime += (tbb::tick_count::now() - tstart).seconds();
#endif

  }
#ifndef _DDA_PP
  Streams[WStreamNum].Items[WItemNum].Addr = Ptr; 
  Streams[WStreamNum].Items[WItemNum].AddrLen = AddrLen;
  Streams[WStreamNum].Items[WItemNum].RWFlag = 0;
  Streams[WStreamNum].Items[WItemNum].PC = pc;
#endif
  WItemNum++;

  // case 2.2: The stream is full. 
  if( WItemNum == STREAMITEMNUM ){
#ifndef _DDA_PP
    Streams[WStreamNum].LoopIter = CurLoopIter;
    Streams[WStreamNum].End = WItemNum;
    StreamIsFull[WStreamNum] = 1;
#endif

#ifdef _DEBUGINFO
    std::cerr<<std::setbase(10)<<"GenerateAddrStream WStreamNum =  " << WStreamNum  << " is full"<< "\n";
#endif
    WStreamNum = (WStreamNum + 1)% STREAMNUM;
    WItemNum = 0;
  }


#ifdef _TIME_PROF
  GTime += (tbb::tick_count::now() - pststart).seconds();
#endif


  return;
#endif // end #ifndef _DDA_PP
#endif

  int retval;
  long AddrInnerOffset = 0, Len1 = 0,  Len2 = 0;
  unsigned char ReadMask1 = 0, ReadMask2 = 0;
  void **ShadowAddr = 0;



  // Compute the shadow cell address. 
  // The accessed space crosses the bounder of 8bytes.
  ShadowAddr = (void**)AppAddrToShadow((long)Ptr);

  // RWTrace-3:
#if 0
  long Addr = (long) ShadowAddr;
  if ( Addr > StackBeg ){
    if( Addr < LDDStackMin )
      LDDStackMin = Addr;
    else if ( Addr > LDDStackMax )
      LDDStackMax = Addr;
  }
  else if( Addr > GlobalEnd ){
    if( Addr > LDDHeapMax )
      LDDHeapMax = Addr;
    else if( Addr < LDDHeapMin )
      LDDHeapMin = Addr;
  }
  else{
    if( Addr > LDDGlobalMax)
      LDDGlobalMax = Addr;
    else if( Addr < LDDGlobalMin )
      LDDGlobalMin = Addr;
  }
#endif

  
 


  retval = __determinInner8bytesInfo( Ptr, AddrLen,  
      &ReadMask1, &ReadMask2 );

  // RWTrace-4. ShadowAddr range. And ShadowAddr won't cross Virtual pages.
 
  long PageNum = (long)ShadowAddr/PageSize;
#if 1
  if( CacheBuf[0] == PageNum || CacheBuf[1] == PageNum || 
      CacheBuf[2] == PageNum  || CacheBuf[3] == PageNum || 
      CacheBuf[4] == PageNum  || CacheBuf[5] == PageNum ||
      CacheBuf[6] == PageNum  || CacheBuf[7] == PageNum  ||
      CacheBuf[8] == PageNum )
    goto NEXT;
#endif
#if 0
  if( ShdwPageSet.find(PageNum) == ShdwPageSet.end() ){
    TotalPageNum++;
    printf("TotalPageNum = %d num = %ld\n", TotalPageNum, PageNum);
  }
#endif
  ShdwPageSet.insert( PageNum );
  if( 2 == retval  )
    ShdwPageSet.insert( PageNum );
  CacheBuf[CacheBufNum] = PageNum;
  CacheBufNum = (++CacheBufNum) >= CBUFLEN ? 0 : CacheBufNum;

NEXT:

  //retval = __determinInner8bytesInfo( Ptr, AddrLen, &AddrInnerOffset, 
  //                                  &Len1, &ReadMask1, &Len2, &ReadMask2 );

#ifdef _OVERHEAD_PROF
  //for(int i = 0; i < AddrLen; i++)
  {
    //PageSet.insert((long)ShadowAddr/PageSize);    
    //AddrSetNum += AddrLen;
    ; //AddrSet.insert((long)ShadowAddr+i);
  }
#endif

  //  if( LDDProfLoopIter == 9 )
#ifdef _DEBUGINFO
  printf("__checkLoadLShadow: ReadAddr = %p, ShadowAddr = %p pc = %p iter = %d\n", Ptr, ShadowAddr, pc, LDDProfLoopIter);
#endif

  if( 1 == retval ){
    __checkLIDandUpdatePresentRTrace(pc, ShadowAddr,  
        &ReadMask1, LDDProfLoopIter); 
  }
  else{
    __checkLIDandUpdatePresentRTrace2(pc, ShadowAddr,  
        &ReadMask1, &ReadMask2, LDDProfLoopIter); 
  }

  return ;
}

//extern inline void  __checkLoadLShadow(int *Ptr, long AddrLen); 

void __checkLoadLShadowDebug(int *ptr, long AddrLen, char *AddrPos, char *VarName)
{

  if ( (long)ptr > 0x7fffffffffff || (long)ptr < 0x70000000000 )
    std::cout<<"checkLoad(): VarName = "<< VarName << ", AddrPos = " << AddrPos << "\n";
  return;
}



#ifndef _DDA_PA
//
// FirstRead: 
// It keep the latest written-iteration to the same bytes of the same PC.
// Thread safe.

inline
void __addReadToPrecedeTrace(Trace* TPtr)
{
  //return;
  LSPresentRWAddrID * PresentRIDPtr = TPtr->Present->RTrace; 
  LSPrecedeRWTrace *PrecedePtr = TPtr->Precede;
  LSPrecedeRWAddrID *PrecedeRIDPtr; 

#ifdef _OVERHEAD_PROF
      LSAddRWToPrecedeTraceTime++; 
#endif
  while( PresentRIDPtr != NULL )
{
    PrecedeRIDPtr = PrecedePtr->RTrace; 
    // Look up for the <RWMask, PC> of present RTrace.
    while(PrecedeRIDPtr){
      if( (PrecedeRIDPtr->PC == PresentRIDPtr->PC) && (PrecedeRIDPtr->RWMask == PresentRIDPtr->RWMask) ){
        break;
      }
      PrecedeRIDPtr = PrecedeRIDPtr->Next;
    }
    // Find.
    if( PrecedeRIDPtr ){
      PrecedeRIDPtr->RWIter = LDDProfLoopIter; // Thread Safe.
    }else{

#ifdef _OVERHEAD_PROF
      LSPrecedeTraceMallocTime++; 
#endif
      PrecedeRIDPtr = (LSPrecedeRWAddrID*) malloc(sizeof(LSPrecedeRWAddrID));
      PrecedeRIDPtr->PC = PresentRIDPtr->PC;
      PrecedeRIDPtr->RWMask = PresentRIDPtr->RWMask;
      PrecedeRIDPtr->RWIter = LDDProfLoopIter;
      PrecedePtr->GlobalReadMask |= PresentRIDPtr->RWMask;
      PrecedeRIDPtr->Next = PrecedePtr->RTrace;
      TPtr->Precede->RTrace = PrecedeRIDPtr;
#ifdef _DEBUGINFO
      printf("insert Precede->RTrace  Iter = %d \n", LDDProfLoopIter   );
#endif
    }
#ifdef _OVERHEAD_PROF
  LSPresentBufFreeTime++;
#endif
    // Add the PresentRIDPtr to LSPresentRWBuf.
    // Get Next Present->Read.
    TPtr->Present->RTrace = PresentRIDPtr->Next;
    PresentRIDPtr->Next = LSPresentRWBuf;
    LSPresentRWBuf = PresentRIDPtr;
    PresentRIDPtr = TPtr->Present->RTrace;
  }
#ifdef _DEBUGINFO
  if( TPtr->Present->RTrace != NULL )
    printf("error in __addReadToPrecedeTrace \n");
#endif

  // Delete the present R-Trace table.
  // TPtr->Present[1].RTrace.clear();
  return;
}

inline
void __addWriteToPrecedeTrace(Trace* TPtr)
{
  LSPresentRWAddrID * PresentWIDPtr = TPtr->Present->WTrace;
  LSPrecedeRWAddrID * WIDPtr; 

  //return ;
#ifdef _OVERHEAD_PROF
      LSAddRWToPrecedeTraceTime++; 
#endif
  for( ; PresentWIDPtr != NULL; ){
    WIDPtr = TPtr->Precede->WTrace;
    // bug ? cannot find < pc = 0x7cf000000c0a Mask = 240> 
    while( WIDPtr ){
      if( WIDPtr->PC == PresentWIDPtr->PC && WIDPtr->RWMask == PresentWIDPtr->RWMask){
          WIDPtr->RWIter = LDDProfLoopIter;
          break;
      }
      WIDPtr = WIDPtr->Next;
    }
    if(WIDPtr == NULL){
#ifdef _OVERHEAD_PROF
      LSPrecedeTraceMallocTime++; 
#endif
      WIDPtr = (LSPrecedeRWAddrID *) malloc (sizeof(LSPrecedeRWAddrID));
      WIDPtr->Next = TPtr->Precede->WTrace;
      TPtr->Precede->WTrace = WIDPtr;
      TPtr->Precede->GlobalWriteMask |= PresentWIDPtr->RWMask;
      WIDPtr->PC = PresentWIDPtr->PC;
      WIDPtr->RWMask = PresentWIDPtr->RWMask;
      WIDPtr->RWIter = LDDProfLoopIter;
    }
    // Add PresentWIDPtr to LSPresentRWBuf.
#ifdef _OVERHEAD_PROF
  LSPresentBufFreeTime++;
#endif
    TPtr->Present->WTrace = PresentWIDPtr->Next;
    PresentWIDPtr->Next = LSPresentRWBuf; 
    LSPresentRWBuf = PresentWIDPtr;
    PresentWIDPtr = TPtr->Present->WTrace;
  }

#ifdef _DEBUGINFO
  if( TPtr->Present->WTrace != NULL )
    printf("error in __addReadToPrecedeTrace \n");
#endif
#if 0
  if( TPtr != NULL ){
    TPtr->Present->GlobalWriteMask = 0;
  }
#endif
  //TPtr->Present[1].WTrace.clear();
  return;
}



#ifdef _ADDRKEY
void __doLCDASetupPrecedeTrace(long Addr)  
{
 Trace *TracePtr = *((Trace**)(Addr)); 

  // Iterate the present trace, and analyze the LCD and add Present to  Precede trace.
  //  No Access within current iteration in the Present Trace.
  if( TracePtr == NULL ){
    return;
  }
  // Not need?
  if ( !TracePtr->Present->ReadNum && !TracePtr->Present->WriteNum ){
    return;
  }
  // Not need?
  if( TracePtr->Present->RWIter != LDDProfLoopIter ){ // Bug?
    return;
  }

  //printf("TracePtr = %p iter = %d\n", TracePtr, LDDProfLoopIter); 
#ifdef _OVERHEAD_PROF   
#endif
  int PresentRBNum, PresentRENum, PrecedeWBNum, PrecedeWENum;
  int PresentWBNum, PresentWENum, PrecedeRBNum, PrecedeRENum;
  LSPresentRWAddrID * PresentRIDPtr, *PresentWIDPtr;
  LSPrecedeRWAddrID * PrecedeWIDPtr, *PrecedeRIDPtr;

  unsigned char ReadMask = 0;

  // 1. Analyzing LCTD.
  if( TracePtr->Present->ReadNum ){
    if ( TracePtr->Precede->WriteNum ){
      PrecedeWENum = TracePtr->Precede->WriteNum;
      PresentRBNum = 0; 
      PresentRENum = TracePtr->Present->ReadNum; 
      PresentRIDPtr = TracePtr->Present->RTrace;
      // Write->Read: Have overlap bytes.
      if( TracePtr->Present->GlobalReadMask & TracePtr->Precede->GlobalWriteMask ){
        for( ; PresentRBNum < PresentRENum; ++PresentRBNum, PresentRIDPtr = PresentRIDPtr->Next){
          ReadMask = PresentRIDPtr->RWMask;
          // Iterate over Preced-write-trace. 
          for( PrecedeWIDPtr = TracePtr->Precede->WTrace, PrecedeWBNum = 0 ; 
              PrecedeWBNum < PrecedeWENum; ++PrecedeWBNum, PrecedeWIDPtr = PrecedeWIDPtr->Next){
            // Exist True dep.
            if( ReadMask & PrecedeWIDPtr->RWMask ) {

              int LCDDist = LDDProfLoopIter - PrecedeWIDPtr->RWIter;

              LSLCDep *LCDTrue = TracePtr->LCDep->TrueDep;
              if( LCDTrue == NULL ){
                LDDLSResultTrace.insert((long)TracePtr); // opt me?
              }else{
                do{
                  if( LCDTrue->SrcPC == (long)PrecedeWIDPtr->PC && LCDTrue->SinkPC == (long) PresentRIDPtr->PC )
                    break;
                  LCDTrue = LCDTrue->next;
                }while( LCDTrue != NULL);
              }
              if( LCDTrue == NULL ){
        #ifdef _OVERHEAD_PROF
          LCDepMallocTime++;  // here?
        #endif
                LCDTrue = (LSLCDep*) malloc( sizeof(LSLCDep) );
                LCDTrue->next = TracePtr->LCDep->TrueDep;
                TracePtr->LCDep->TrueDep = LCDTrue; 
                LCDTrue->SrcPC = (long)PrecedeWIDPtr->PC;
                LCDTrue->SinkPC = (long) PresentRIDPtr->PC; 
                LCDTrue->DepDist = (LSDepDist*) malloc(sizeof(LSDepDist));
                LCDTrue->DepDist->DepDistVal = LCDDist;
                LCDTrue->DepDist->next = NULL;
              }
              else{
                LSDepDist *LCDTrueDist = LCDTrue->DepDist;
                while( LCDTrueDist != NULL ){
                  if( LCDTrueDist->DepDistVal == LCDDist )
                    break;
                  LCDTrueDist = LCDTrueDist->next;
                }

                if( LCDTrueDist == NULL ){
                  LCDTrueDist = (LSDepDist*) malloc(sizeof(LSDepDist));
                  LCDTrueDist->DepDistVal = LCDDist;
                  LCDTrueDist->next = LCDTrue->DepDist;
                  LCDTrue->DepDist = LCDTrueDist; 

                }

              }

              //printf("in compute---  srcpc = %p, sinkpc = %p size = %d\n", (void*)TDep.SrcPC, (void*)TDep.SinkPC, LDDLSResult.TrueDep.size());

            }
          }
        }
      } 
    }
    //__addReadToPrecedeTrace( TracePtr ); 

  }


  // 2. Anzlyzing LCOD.
  if( TracePtr->Present->WriteNum ){
    if ( TracePtr->Precede->WriteNum ){
      //PrecedeWB = TracePtr->Precede.WTrace.begin();
      //
      PrecedeWENum = TracePtr->Precede->WriteNum;
      PresentWBNum = 0; 
      PresentWENum = TracePtr->Present->WriteNum; 
      // Write->Read: Have overlap bytes.
      if( TracePtr->Present->GlobalWriteMask & TracePtr->Precede->GlobalWriteMask ){
        PresentWIDPtr = TracePtr->Present->WTrace;
        for( ; PresentWBNum < PresentWENum; ++PresentWBNum, PresentWIDPtr = PresentWIDPtr->Next){
          ReadMask = PresentWIDPtr->RWMask;
          // Iterate over Preced-write-trace. 
          for(PrecedeWIDPtr = TracePtr->Precede->WTrace, PrecedeWBNum = 0; 
              PrecedeWBNum < PrecedeWENum; ++PrecedeWBNum, PrecedeWIDPtr = PrecedeWIDPtr->Next ){
            // Exist Out dep.
            if( ReadMask & PrecedeWIDPtr->RWMask ) {
              int LCDDist = LDDProfLoopIter - PrecedeWIDPtr->RWIter;

              LSLCDep *LCDOut = TracePtr->LCDep->OutDep;
              if( LCDOut == NULL ) 
                LDDLSResultTrace.insert(((long)TracePtr)); // opt me?
              else{ 
                do{
                  if( LCDOut->SrcPC == (long)PrecedeWIDPtr->PC && LCDOut->SinkPC == (long) PresentWIDPtr->PC )
                    break;
                  LCDOut = LCDOut->next;
                }while( LCDOut != NULL);
              }
              if( LCDOut == NULL ){
                LCDOut = (LSLCDep*) malloc( sizeof(LSLCDep) );
                LCDOut->next = TracePtr->LCDep->OutDep;
                TracePtr->LCDep->OutDep = LCDOut; 
                LCDOut->SrcPC = (long)PrecedeWIDPtr->PC;
                LCDOut->SinkPC = (long) PresentWIDPtr->PC; 
                LCDOut->DepDist = (LSDepDist*) malloc(sizeof(LSDepDist));
                LCDOut->DepDist->DepDistVal = LCDDist;
                LCDOut->DepDist->next = NULL;
              }
              else{
                LSDepDist *LCDOutDist = LCDOut->DepDist;
                while( LCDOutDist != NULL ){
                  if( LCDOutDist->DepDistVal == LCDDist )
                    break;
                  LCDOutDist = LCDOutDist->next;
                }

                if( LCDOutDist == NULL ){
                  LCDOutDist = (LSDepDist*) malloc(sizeof(LSDepDist));
                  LCDOutDist->DepDistVal = LCDDist;
                  LCDOutDist->next = LCDOut->DepDist;
                  LCDOut->DepDist = LCDOutDist; 

                }

              }


            }
          }
        }
      } 
    }
  }

  // 3. Analyzing LCAD.
  if( TracePtr->Present->WriteNum ){
    if ( TracePtr->Precede->ReadNum ){
      //PrecedeRB = TracePtr->Precede.RTrace.begin();
      PrecedeRENum = TracePtr->Precede->ReadNum;
      PresentWBNum = 0; 
      PresentWENum = TracePtr->Present->WriteNum; 
      PresentWIDPtr = TracePtr->Present->WTrace;

      // Write->Read: Have overlap bytes.
      if( TracePtr->Present->GlobalWriteMask & TracePtr->Precede->GlobalReadMask ){
        for( ; PresentWBNum < PresentWENum; ++PresentWBNum, PresentWIDPtr = PresentWIDPtr->Next){
          ReadMask = PresentWIDPtr->RWMask;
          // Iterate over Preced-write-trace. 
          for( PrecedeRBNum = 0, PrecedeRIDPtr = TracePtr->Precede->RTrace; 
              PrecedeRBNum < PrecedeRENum; ++PrecedeRBNum, PrecedeRIDPtr = PrecedeRIDPtr->Next){
            // Exist Anti dep.
            if( ReadMask & PrecedeRIDPtr->RWMask ) {
              int LCDDist = LDDProfLoopIter - PrecedeRIDPtr->RWIter;
              LSLCDep *LCDAnti = TracePtr->LCDep->AntiDep;
              if( LCDAnti == NULL )
                LDDLSResultTrace.insert((long)TracePtr); // opt me?
              else{
                do{
                  if( LCDAnti->SrcPC == (long)PrecedeRIDPtr->PC && LCDAnti->SinkPC == (long) PresentWIDPtr->PC )
                    break;
                  LCDAnti = LCDAnti->next;
                }while( LCDAnti != NULL);
              }
              if( LCDAnti == NULL ){
                LCDAnti = (LSLCDep*) malloc( sizeof(LSLCDep) );
                LCDAnti->next = TracePtr->LCDep->AntiDep;
                TracePtr->LCDep->AntiDep = LCDAnti; 
                LCDAnti->SrcPC = (long)PrecedeRIDPtr->PC;
                LCDAnti->SinkPC = (long) PresentWIDPtr->PC; 
                LCDAnti->DepDist = (LSDepDist*) malloc(sizeof(LSDepDist));
                LCDAnti->DepDist->DepDistVal = LCDDist;
                LCDAnti->DepDist->next = NULL;
              }
              else{
                LSDepDist *LCDAntiDist = LCDAnti->DepDist;
                while( LCDAntiDist != NULL ){
                  if( LCDAntiDist->DepDistVal == LCDDist )
                    break;
                  LCDAntiDist = LCDAntiDist->next;
                }

                if( LCDAntiDist == NULL ){
                  LCDAntiDist = (LSDepDist*) malloc(sizeof(LSDepDist));
                  LCDAntiDist->DepDistVal = LCDDist;
                  LCDAntiDist->next = LCDAnti->DepDist;
                  LCDAnti->DepDist = LCDAntiDist; 

                }

              }


            }
          }
        }
      } 
    }
  }

  __addReadToPrecedeTrace( TracePtr ); 
  __addWriteToPrecedeTrace( TracePtr );
#ifdef _OVERHEAD_PROF   
  //OPNewPrecedeTSize += TracePtr->Precede->WriteNum;
  //OPNewPrecedeTSize += TracePtr->Precede->ReadNum;
#endif

#ifdef _OVERHEAD_PROF


#endif
//  RBClear(LDDRWTrace, LDDRWTrace->Root);
  return ;
}
#endif

// 1) Analyze Loop carried dependence;
// 2) Add the present trace table into the precede trace table.
// 3) Store the data-dependence results in ShadowAdd(SinkPC)-pointed memory.
// 4) LCDep stored in the same struct with LIDep.
int RWNum;
void __doLCDASetupPrecedeTraceWithSinkPCAsKey(long Addr)
{

 Trace *TracePtr = *((Trace**)(Addr)); 

  // Iterate the present trace, and analyze the LCD and add Present to  Precede trace.
  //  No Access within current iteration in the Present Trace.
  if( TracePtr == NULL ){
    return;
  }
  // Not need?
  if ( (TracePtr->Present->RTrace == NULL) && (TracePtr->Present->WTrace == NULL) ){
    return;
  }
  // Not need?
  if( TracePtr->Present->RWIter != LDDProfLoopIter ){ // Bug?
    return;
  }

  LSPresentRWAddrID * PresentRIDPtr, *PresentWIDPtr;
  LSPrecedeRWAddrID * PrecedeWIDPtr, *PrecedeRIDPtr;

  unsigned char ReadMask = 0;
  LSDepInfo **DepShadowPtr;
  LSDepInfo *LCDepPtr;
  
  PresentRIDPtr = TracePtr->Present->RTrace;
  // 1. Analyzing LCTD.
  if( PresentRIDPtr != NULL ){
     PrecedeWIDPtr = TracePtr->Precede->WTrace;
    if ( PrecedeWIDPtr != NULL ){
      // Write->Read: Have overlap bytes.
      if( TracePtr->Present->GlobalReadMask & TracePtr->Precede->GlobalWriteMask ){
        for( ; PresentRIDPtr != NULL;  PresentRIDPtr = PresentRIDPtr->Next){
          ReadMask = PresentRIDPtr->RWMask;
          // Iterate over Preced-write-trace. 
          for( PrecedeWIDPtr = TracePtr->Precede->WTrace ; 
              PrecedeWIDPtr != NULL; PrecedeWIDPtr = PrecedeWIDPtr->Next){
            // Exist True dep.
            if( ReadMask & PrecedeWIDPtr->RWMask ) {
              int LCDDist = LDDProfLoopIter - PrecedeWIDPtr->RWIter;
              // Store true deps. Todo....
              DepShadowPtr =(LSDepInfo**) AppAddrToShadow( (long)PresentRIDPtr->PC );
              if( *DepShadowPtr == NULL ){
#ifdef _OVERHEAD_PROF
  LCDepInfoMallocTime++;
  LSPCShadowTime++;
  
  DepRangeNum++;
#endif
                LCDepPtr = (LSDepInfo*) malloc (sizeof(LSDepInfo));
                *DepShadowPtr = LCDepPtr;
                LCDepPtr->TrueDep = NULL;
                LCDepPtr->AntiDep = NULL;
                LCDepPtr->OutDep = NULL;
                if( (long)DepShadowPtr > LDDLSResultMax )
                  LDDLSResultMax = (long)DepShadowPtr;
                else if( (long)DepShadowPtr < LDDLSResultMin)
                  LDDLSResultMin = (long) DepShadowPtr;
              }
              else
                LCDepPtr = *DepShadowPtr;

              LSDep *LCDTrue = LCDepPtr->TrueDep;
              while( LCDTrue != NULL){
                if( (LCDTrue->SrcPC ==(long)PrecedeWIDPtr->PC) && (LCDTrue->SinkPC == (long)PresentRIDPtr->PC) ){
                  if( 0 == LCDTrue->Dist[1] ){
                    LCDTrue->Dist[1] = LCDDist;
#ifdef _OVERHEAD_PROF
  LCDepNum++;
#endif
                  }
                  else if( LCDTrue->Dist[1] > LCDDist ){
                    LCDTrue->Dist[1] = LCDDist;
                  }
                  break;
                }
                LCDTrue = LCDTrue->next;
              }
              if( LCDTrue == NULL ){
#ifdef _OVERHEAD_PROF
  LCDepMallocTime++;
  LCDepNum++;
#endif
                LCDTrue = (LSDep*) malloc( sizeof(LSDep) );
                LCDTrue->next = LCDepPtr->TrueDep;
                LCDepPtr->TrueDep = LCDTrue; 
                LCDTrue->SrcPC = (long)PrecedeWIDPtr->PC;
                LCDTrue->SinkPC = (long) PresentRIDPtr->PC; 
                LCDTrue->Dist[0] = 1;
                LCDTrue->Dist[1] = LCDDist;
              }

            }
          }
        }
      } 
    }
  }


  // 2. Anzlyzing LCOD.
  PresentWIDPtr = TracePtr->Present->WTrace;
  if( PresentWIDPtr != NULL ){
    PrecedeWIDPtr = TracePtr->Precede->WTrace;
    if ( PrecedeWIDPtr != NULL ){
      // Write->Read: Have overlap bytes.
      if( TracePtr->Present->GlobalWriteMask & TracePtr->Precede->GlobalWriteMask ){
        for( ; PresentWIDPtr != NULL; PresentWIDPtr = PresentWIDPtr->Next){
          ReadMask = PresentWIDPtr->RWMask;
          // Iterate over Preced-write-trace. 
          for(PrecedeWIDPtr = TracePtr->Precede->WTrace; PrecedeWIDPtr != NULL; 
              PrecedeWIDPtr = PrecedeWIDPtr->Next ){
            // Exist Out dep.
            if( ReadMask & PrecedeWIDPtr->RWMask ) {
              int LCDDist = LDDProfLoopIter - PrecedeWIDPtr->RWIter;

              DepShadowPtr =(LSDepInfo**) AppAddrToShadow( (long)PresentWIDPtr->PC );
              if( *DepShadowPtr == NULL ){
#ifdef _OVERHEAD_PROF
  LSPCShadowTime++;
  LCDepInfoMallocTime++;
  DepRangeNum++;
#endif
                LCDepPtr = (LSDepInfo*) malloc (sizeof(LSDepInfo));
                *DepShadowPtr = LCDepPtr;
                LCDepPtr->TrueDep = NULL;
                LCDepPtr->AntiDep = NULL;
                LCDepPtr->OutDep = NULL;
                if( (long)DepShadowPtr > LDDLSResultMax )
                  LDDLSResultMax = (long)DepShadowPtr;
                else if( (long)DepShadowPtr < LDDLSResultMin)
                  LDDLSResultMin = (long) DepShadowPtr;
              }
              else
                LCDepPtr = *DepShadowPtr;

              LSDep *LCDOut = LCDepPtr->OutDep;
              while( LCDOut != NULL){
                if( LCDOut->SrcPC == (long)PrecedeWIDPtr->PC && LCDOut->SinkPC == (long) PresentWIDPtr->PC ){
                  if( LCDOut->Dist[1] == 0 ){
                    LCDOut->Dist[1] = LCDDist; 
#ifdef _OVERHEAD_PROF
  LCDepNum++;
#endif
                  } 
                  else if( LCDOut->Dist[1] > LCDDist ){
                    LCDOut->Dist[1] = LCDDist;
                  }
                  break;
                }
                LCDOut = LCDOut->next;
              }              
              if( LCDOut == NULL ){
#ifdef _OVERHEAD_PROF
  LCDepMallocTime++;
  LCDepNum++;
#endif
                LCDOut = (LSDep*) malloc( sizeof(LSDep) );
                LCDOut->next = LCDepPtr->OutDep;
                LCDepPtr->OutDep = LCDOut; 
                LCDOut->SrcPC = (long)PrecedeWIDPtr->PC;
                LCDOut->SinkPC = (long) PresentWIDPtr->PC; 
                LCDOut->Dist[1] = LCDDist;
                LCDOut->Dist[0] = 1;
                
              }
            }
          }
        }
      } 
    }
  }

  // 3. Analyzing LCAD.
  PresentWIDPtr = TracePtr->Present->WTrace;
  if( PresentWIDPtr != NULL ){
    PrecedeRIDPtr = TracePtr->Precede->RTrace;
    if ( PrecedeRIDPtr != NULL ){
      // Write->Read: Have overlap bytes.
      if( TracePtr->Present->GlobalWriteMask & TracePtr->Precede->GlobalReadMask ){
        for( ; PresentWIDPtr != NULL;  PresentWIDPtr = PresentWIDPtr->Next){
          ReadMask = PresentWIDPtr->RWMask;
          // Iterate over Preced-write-trace. 
          for( PrecedeRIDPtr = TracePtr->Precede->RTrace; PrecedeRIDPtr != NULL; PrecedeRIDPtr = PrecedeRIDPtr->Next){
            // Exist Anti dep.
            if( ReadMask & PrecedeRIDPtr->RWMask ) {
              int LCDDist = LDDProfLoopIter - PrecedeRIDPtr->RWIter;
              DepShadowPtr = (LSDepInfo**) AppAddrToShadow( (long)PresentWIDPtr->PC );
              if( *DepShadowPtr == NULL ){
#ifdef _OVERHEAD_PROF
  LSPCShadowTime++;
  LCDepInfoMallocTime++;
  DepRangeNum++;
#endif
                LCDepPtr = (LSDepInfo*) malloc (sizeof(LSDepInfo));
                *DepShadowPtr = LCDepPtr;
                LCDepPtr->TrueDep = NULL;
                LCDepPtr->AntiDep = NULL;
                LCDepPtr->OutDep = NULL;
                if( (long)DepShadowPtr > LDDLSResultMax )
                  LDDLSResultMax = (long)DepShadowPtr;
                else if( (long)DepShadowPtr < LDDLSResultMin)
                  LDDLSResultMin = (long) DepShadowPtr;
              }
              else
                LCDepPtr = *DepShadowPtr;

              LSDep *LCDAnti = LCDepPtr->AntiDep;
              while( LCDAnti != NULL){
                if( LCDAnti->SrcPC == (long)PrecedeRIDPtr->PC && LCDAnti->SinkPC == (long) PresentWIDPtr->PC ){
                  if( LCDAnti->Dist[1] == 0 ){
#ifdef _OVERHEAD_PROF
  LCDepNum++;
#endif
                    LCDAnti->Dist[1] = LCDDist;
                  }
                  if( LCDAnti->Dist[1] > LCDDist ){
                    LCDAnti->Dist[1] = LCDDist;
                  }
                  break;
                }
                LCDAnti = LCDAnti->next;
              }
              if( LCDAnti == NULL ){
#ifdef _OVERHEAD_PROF
  LCDepMallocTime++;
  LCDepNum++;
#endif
                LCDAnti = (LSDep*) malloc( sizeof(LSDep) );
                LCDAnti->next = LCDepPtr->AntiDep;
                LCDepPtr->AntiDep = LCDAnti; 
                LCDAnti->SrcPC = (long)PrecedeRIDPtr->PC;
                LCDAnti->SinkPC = (long) PresentWIDPtr->PC; 
                LCDAnti->Dist[1] = LCDDist;
                LCDAnti->Dist[0] = 1;
              }

            }
          }
        }
      } 
    }
  }

  __addReadToPrecedeTrace( TracePtr ); 
  __addWriteToPrecedeTrace( TracePtr );

//  RBClear(LDDRWTrace, LDDRWTrace->Root);
  return ;
}


void __traverseRBTreeRWTrace()
{
  // unsigned long B, E;

  unsigned char ReadMask = 0;

  RBTreeNode *StackTop = LDDRWTrace->nil;
  StackTop->Next = LDDRWTrace->nil;
  StackTop->Prev = LDDRWTrace->nil;
  RBTreeNode *Cur = LDDRWTrace->Root;
  int i;
  while( Cur != LDDRWTrace->nil ){

    // Traversal left tree.
    while( Cur->Left != LDDRWTrace->nil ){
      StackTop->Next = Cur;
      Cur->Prev = StackTop;
      StackTop = StackTop->Next;

      Cur = Cur->Left;
    }
    // Visit root.
    //visit(Cur);
    long End = Cur->ValEnd;
    long Beg = Cur->ValBeg;
#ifdef _ADDRKEY
    for( ;Beg <= End;  Beg += 8)
      __doLCDASetupPrecedeTrace(Beg);
#endif

      // Traversal right tree.
    if( Cur->Right != LDDRWTrace->nil ){
      Cur = Cur->Right;
    }
    else{
        Cur = StackTop;
        StackTop = StackTop->Prev; // Pop
      while( Cur != LDDRWTrace->nil) {
        //vist(Cur);
        long End = Cur->ValEnd;
        long Beg = Cur->ValBeg;
#ifdef _ADDRKEY
        for( ;Beg <= End;  Beg += 8)
          __doLCDASetupPrecedeTrace(Beg);
#endif
        if( Cur->Right != LDDRWTrace->nil ){
          Cur = Cur->Right;
          break;
        }
        else{
          Cur = StackTop;
          StackTop = StackTop->Prev; // Pop
        }
      }
    }
  }

  RBClear(LDDRWTrace, LDDRWTrace->Root);
#ifdef _OVERHEAD_PROF
 // printf("After RWClear(): FreeListLen = %ld, RWMallocTime = %ld\n", FreeListLen, RWMallocTime);
#endif
  return;
}

/* Not thread safe.
 *
 */
void __traverseTriSectionRWTrace()
{
#if 0
  LDDGlobalMax = 0x6cf000000000;
  LDDGlobalMin = 0x6cf000400000;
  LDDStackMin = 0x6fffffffffff;
  LDDStackMax = 0x6fff00000000;
  LDDHeapMin = LDDStackMin;
  LDDHeapMax = LDDGlobalMax;
#endif

#if 0
  // 1) stack(0x7fff00000000, 0x7fffffffffff)
  //printf("StackMin = %p, StackMax = %p \n", LDDStackMin, LDDStackMax);
  for( ; LDDStackMin <= LDDStackMax; LDDStackMin += 8 ){
    // 1) TRYOPT__
    //__doLCDASetupPrecedeTrace(LDDStackMin);
    // 2) #ifdef TRYOPT_LICKEY_SINKPC
    __doLCDASetupPrecedeTraceWithSinkPCAsKey(LDDStackMin);
    //#endif
  }

  // 2) heap(0x7cf000400000, 0x7fff00000000)
  //printf("HeapMin = %p, HeapMax = %p \n", LDDHeapMin, LDDHeapMax);
  for( ; LDDHeapMin <= LDDHeapMax; LDDHeapMin += 8 ){
    //__doLCDASetupPrecedeTrace(LDDHeapMin);
    __doLCDASetupPrecedeTraceWithSinkPCAsKey(LDDHeapMin);
  }

  // 3) global(0x7cf000000000, 0x7cf000400000)
  //printf("GlobalMin = %p, GlobalMax = %p \n", LDDGlobalMin, LDDGlobalMax);
  for( ; LDDGlobalMin <= LDDGlobalMax; LDDGlobalMin += 8 ){
    //__doLCDASetupPrecedeTrace( LDDGlobalMin );
    __doLCDASetupPrecedeTraceWithSinkPCAsKey(LDDGlobalMin);
  }
#endif
  std::unordered_set<long>::iterator beg,end;
  beg = ShdwPageSet.begin();
  end = ShdwPageSet.end();
  printf("TotalPageNum = %d \n", ShdwPageSet.size() );
  for( ; beg != end; beg++){
    //printf("PageNum = %ld %10000 = %ld\n", *beg, *beg%10000);
    long Addr = *beg * PageSize;
    int num = PageSize/8;
    for( int i = 0; i < num; i++, Addr += 8){
      __doLCDASetupPrecedeTraceWithSinkPCAsKey(Addr);
    }
  }


  // Clear records.
  LDDStackMin =  0x6fffffffffff;
  LDDStackMax =  0x6fff00000000;
  LDDHeapMin =   0x6fff00000000;
  LDDHeapMax =   0x6cf000400000;
  LDDGlobalMin = 0x6cf000400000;
  LDDGlobalMax = 0x6cf000000000;
  ShdwPageSet.clear();
   CacheBuf[0] = CacheBuf[1] = CacheBuf[2] = CacheBuf[3] = CacheBuf[4] = 0;
   CacheBuf[5] = CacheBuf[6] = CacheBuf[7] = CacheBuf[8] = CacheBuf[9] = 0;

  return;
}

#endif


/* __DDA_DEFAULT_PP_PARLID
 *
 *
 */
void __EndAddPresentToPrecedeTrace()
{

//return;

#ifdef _DDA_DEFAULT
#ifdef _DDA_DEFAULT_PP_PARLIDA
  std::cout<<"__EndAddPresentToPrecedeTrace PROCID = " << PROCID <<", LoopIter " << LDDProfLoopIter << "\n";
#if 0
  // Add iteration-termination signal in the current buffer.  
  //Streams[WStreamNum].Items[0].RWFlag = 1; // new iteration.
  //Streams[WStreamNum].End = WItemNum;
  //Streams[WStreamNum].LoopIter = CurLoopIter;
  GLStreams[WStreamNum].Items[0].RWFlag = 1; // new iteration.
  GLStreams[WStreamNum].End = WItemNum;
  GLStreams[WStreamNum].LoopIter = CurLoopIter;

#ifndef _GEN
  atomic_set(&GLPPStreamIsFull[WStreamNum], 1);
#endif

  // Apply a new buffer for the new iteration.
  CurLoopIter = LDDProfLoopIter;
  WStreamNum = (WStreamNum+1) % STREAMNUM;
  WItemNum = 1;
  PtrItems = &GLStreams[WStreamNum].Items[0];

#ifdef _TIME_PROFx
  tbb::tick_count tstart;
  tstart = tbb::tick_count::now();
#endif
  while( atomic_read(&GLPPStreamIsFull[WStreamNum]) ) ;
#ifdef _TIME_PROFx
  GPTime += (tbb::tick_count::now() - tstart).seconds();
#endif

  PtrItems->RWFlag = 0; // 
  PtrItems++;
#endif
  // RWFlag == 4, This is the End-Loop-Iter.
  GLStreams[WStreamNum].Items[0].RWFlag = 4; // 
  GLStreams[WStreamNum].End = 1;
  GLStreams[WStreamNum].LoopIter = CurLoopIter;
  atomic_set(&GLPPStreamIsFull[WStreamNum], 1);

  return ;
#else

  // Add iteration-termination signal in the current buffer.  
  Streams[WStreamNum].Items[0].RWFlag = 1; // new iteration.
  Streams[WStreamNum].End = WItemNum;
  Streams[WStreamNum].LoopIter = CurLoopIter;

#ifndef _GEN
  StreamIsFull[WStreamNum] = 1;
#endif

  // Apply a new buffer for the new iteration.
  std::cout<<"LDDProfLoopIter = " << LDDProfLoopIter <<"\n";
  CurLoopIter = LDDProfLoopIter;
  WStreamNum = (WStreamNum+1) % STREAMNUM;
  WItemNum = 1;
  PtrItems = &Streams[WStreamNum].Items[0];

#ifdef _TIME_PROFx
  tbb::tick_count tstart;
  tstart = tbb::tick_count::now();
#endif
  while( StreamIsFull[WStreamNum] ) ;
#ifdef _TIME_PROFx
  GPTime += (tbb::tick_count::now() - tstart).seconds();
#endif

  PtrItems->RWFlag = 0; // 
  PtrItems++;

  return ;
#endif
#endif

#ifdef _GAS_SHADOW // #define _GAS_SHADOW
  // Add iteration-termination signal in the current buffer.  
  AccShdAddrStream[WStreamNum][0] = 1; // new iteration.
  AccShdNum[WStreamNum] = AccNum;
  AccShdLoopIter[WStreamNum] = CurLoopIter;
  StreamIsFull[WStreamNum] = 1;
#ifdef _MUTEX
  pthread_mutex_lock(&GRMutex[WStreamNum]);
  pthread_cond_signal(&GRCond[WStreamNum]);
  pthread_mutex_unlock(&GRMutex[WStreamNum]);
#endif

  // Apply a new buffer for the new iteration.
  std::cout<<"LDDProfLoopIter = " << LDDProfLoopIter <<"\n";
  CurLoopIter = LDDProfLoopIter;
  WStreamNum = (WStreamNum+1) % STREAMNUM;
  WItemNum = 1;
  //UseArrayNum = 0;
  AccNum = 1;

#ifdef _TIME_PROFx
  tbb::tick_count tstart;
  tstart = tbb::tick_count::now();
#endif

#ifndef _MUTEX
  while( StreamIsFull[WStreamNum] ) ;
#else
  if( StreamIsFull[WStreamNum] ) {
    pthread_mutex_lock(&GRMutex[WStreamNum]);
    pthread_cond_wait(&GRCond[WStreamNum], &GRMutex[WStreamNum]);
    pthread_mutex_unlock(&GRMutex[WStreamNum]);
  }
#endif

#ifdef _TIME_PROFx
  GPTime += (tbb::tick_count::now() - tstart).seconds();
#endif

  //PtrShdArrayPool = &ShdArrayPool[WStreamNum][1];
  AccShdAddrStream[WStreamNum][0] = 0;

  return ;

#endif // end define _GAS_SHADOW



  std::set<long>::iterator B, E;
  Trace *TracePtr;
  if( LDDProfLoopIter < 3 )
    return;
  //printf("ShdowBeg = %p, ShadowEnd = %p \n", (void*)ShadowLowBound, (void*)ShadowUpBound );
  // printf("Iter = %d, StackMax = %p, StackMin = %p HeapMax = %p HeapMin = %p GlobalMax = %p GlobaMin = %p\n", LDDProfLoopIter, LDDStackMax, LDDStackMin, LDDHeapMax, LDDHeapMin, LDDGlobalMax, LDDGlobalMin);
  printf("__addPresentToPrecedeTrace \n");


  // RB tree store the RW-Addr. 
  //__traverseRBTreeRWTrace(); 

  // Global, Heap, Statck(min, max) maitain the RW-Addr.
#ifndef _DDA_PA
  __traverseTriSectionRWTrace();
#endif

  //#ifdef _OVERHEAD_PROF
#if 0
  float PresentTraceBuf = 0, PrecedeTraceBuf = 0, LCDepBuf = 0, LIDepBuf = 0;
  float AddrShadowBuf = 0, PCShadowBuf = 0;
  float Total;
  printf("after __addPresentToPrecedeTrace \n");
  PresentTraceBuf = (float)(LSPresentTraceMallocTime*sizeof(LSPresentRWAddrID))/(1024.0*1024.0);
  PrecedeTraceBuf = (float)(LSPrecedeTraceMallocTime*sizeof(LSPrecedeRWAddrID))/(1024.0*1024.0); 
  LCDepBuf = (float)(LCDepMallocTime*sizeof(LSDep))/(1024.0*1024.0) ; 
  LIDepBuf = (float)(LIDepMallocTime*sizeof(LSDep))/(1024.0*1024.0) ;
  AddrShadowBuf = (float)(LSAddrShadowTime*(sizeof(Trace)+ 8 + sizeof(LSPresentRWTrace)+sizeof(LSPrecedeRWTrace)))/(1024.0*1024.0);
  PCShadowBuf = (float)( LSPCShadowTime*sizeof(LSDepInfo) + 8 )/(1024.0*1024.0) ;
  Total = PresentTraceBuf + PrecedeTraceBuf + LCDepBuf + LIDepBuf + AddrShadowBuf + PCShadowBuf;

  printf("RWNum = %d \n", RWNum);
  printf("PresentTraceBuf = %f MB, PrecedeTracebuf = %f MB, LCDepBuf = %f MB, LIDepBuf = %f MB \n",
      PresentTraceBuf, PrecedeTraceBuf, LCDepBuf, LIDepBuf );
  printf("AddrShadowBuf = %f MB, PCShadowBuf = %f MB \n", AddrShadowBuf, PCShadowBuf);

  printf("TotalUpdateWrite = %ld, NWUpWrite = %ld \n", TotalUpWrite, NWTotalUpWrite);
  printf( "LSAddrShadowTime = %ld, LSPCShadowTime = %ld \n", LSAddrShadowTime, LSPCShadowTime );
  printf("LSPresentTraceMallocTime = %ld, LSPrecedeTraceMallocTime = %ld \n", LSPresentTraceMallocTime, LSPrecedeTraceMallocTime);
  printf("PresentBufFreeTime = %ld LSBufReuseTime = %ld TotalBuf = %f MB\n", LSPresentBufFreeTime, LSBufReuseTime, Total);
#endif

  return;
}


/*
 * Start LCDA and Setup Precede RWTrace table. 
 * 
 * for.cond
 *  .......
 * for.inc
 *  __addPresentToPrecedeTrace;
 *  br for.cond
 *
 */
void __addPresentToPrecedeTrace()
{



#ifdef _DDA_DEFAULT
#ifdef _DDA_DEFAULT_PP_PARLIDA
  if( !LDDProfCurIter )
    return;
  //std::cout<<std::setbase(10)<<"__addPresentToPrecedeTrace PROCID = " << PROCID <<", LoopIter " << LDDProfLoopIter << ", WStreamNum " << WStreamNum << "\n";
  // Add iteration-termination signal in the current buffer.  
  //Streams[WStreamNum].Items[0].RWFlag = 1; // new iteration.
  //Streams[WStreamNum].End = WItemNum;
  //Streams[WStreamNum].LoopIter = CurLoopIter;
  GLStreams[WStreamNum].Items[0].RWFlag = 1; // new iteration.
  GLStreams[WStreamNum].End = WItemNum;
  GLStreams[WStreamNum].LoopIter = LDDProfLoopIter; // bug
  //GLStreams[WStreamNum].LoopIter = CurLoopIter; //

#ifndef _GEN
  atomic_set(&GLPPStreamIsFull[WStreamNum], 1);
#endif

  // Apply a new buffer for the new iteration.
  //CurLoopIter = LDDProfLoopIter;
  WStreamNum = (WStreamNum+1) % STREAMNUM;
  WItemNum = 1;
  PtrItems = &GLStreams[WStreamNum].Items[0];

#ifdef _TIME_PROF
  tbb::tick_count tstart;
  tstart = tbb::tick_count::now();
#endif
  while( atomic_read(&GLPPStreamIsFull[WStreamNum]) ) ;
#ifdef _TIME_PROF
  GPTime += (tbb::tick_count::now() - tstart).seconds();
#endif

  PtrItems->RWFlag = 0; // 
  PtrItems++;

  return ;
#else

  // Add iteration-termination signal in the current buffer.  
  Streams[WStreamNum].Items[0].RWFlag = 1; // new iteration.
  Streams[WStreamNum].End = WItemNum;
  Streams[WStreamNum].LoopIter = CurLoopIter;

#ifndef _GEN
  StreamIsFull[WStreamNum] = 1;
#endif

  // Apply a new buffer for the new iteration.
  std::cout<<"LDDProfLoopIter = " << LDDProfLoopIter <<"\n";
  CurLoopIter = LDDProfLoopIter;
  WStreamNum = (WStreamNum+1) % STREAMNUM;
  WItemNum = 1;
  PtrItems = &Streams[WStreamNum].Items[0];

#ifdef _TIME_PROFx
  tbb::tick_count tstart;
  tstart = tbb::tick_count::now();
#endif
  while( StreamIsFull[WStreamNum] ) ;
#ifdef _TIME_PROFx
  GPTime += (tbb::tick_count::now() - tstart).seconds();
#endif

  PtrItems->RWFlag = 0; // 
  PtrItems++;

  return ;
#endif
#endif

#ifdef _GAS_SHADOW // #define _GAS_SHADOW
  // Add iteration-termination signal in the current buffer.  
  AccShdAddrStream[WStreamNum][0] = 1; // new iteration.
  AccShdNum[WStreamNum] = AccNum;
  AccShdLoopIter[WStreamNum] = CurLoopIter;
  StreamIsFull[WStreamNum] = 1;
#ifdef _MUTEX
  pthread_mutex_lock(&GRMutex[WStreamNum]);
  pthread_cond_signal(&GRCond[WStreamNum]);
  pthread_mutex_unlock(&GRMutex[WStreamNum]);
#endif

  // Apply a new buffer for the new iteration.
  std::cout<<"LDDProfLoopIter = " << LDDProfLoopIter <<"\n";
  CurLoopIter = LDDProfLoopIter;
  WStreamNum = (WStreamNum+1) % STREAMNUM;
  WItemNum = 1;
  //UseArrayNum = 0;
  AccNum = 1;

#ifdef _TIME_PROFx
  tbb::tick_count tstart;
  tstart = tbb::tick_count::now();
#endif

#ifndef _MUTEX
  while( StreamIsFull[WStreamNum] ) ;
#else
  if( StreamIsFull[WStreamNum] ) {
    pthread_mutex_lock(&GRMutex[WStreamNum]);
    pthread_cond_wait(&GRCond[WStreamNum], &GRMutex[WStreamNum]);
    pthread_mutex_unlock(&GRMutex[WStreamNum]);
  }
#endif

#ifdef _TIME_PROFx
  GPTime += (tbb::tick_count::now() - tstart).seconds();
#endif

  //PtrShdArrayPool = &ShdArrayPool[WStreamNum][1];
  AccShdAddrStream[WStreamNum][0] = 0;

  return ;

#endif // end define _GAS_SHADOW

#ifdef _OVERHEAD_PROF_
  //printf("Iter = %d, StackMax = %p, StackMin = %p HeapMax = %p HeapMin = %p GlobalMax = %p GlobaMin = %p\n", LDDProfLoopIter, LDDStackMax, LDDStackMin, LDDHeapMax, LDDHeapMin, LDDGlobalMax, LDDGlobalMin);
  //printf("StackAddrSize = %ld \n", ((long)LDDStackMax - (long)LDDStackMin)*8 );
  //printf("GlobalAddrSize = %ld \n", ((long)LDDGlobalMax - (long)LDDGlobalMin)*8 );
  //printf("HeapAddrSize = %ld \n", ((long)LDDHeapMax - (long)LDDHeapMin)*8 );
  //printf("RealAddrSize = %ld \n", AddrSet.size());
  //printf("AddrSetNum = %ld \n", AddrSetNum);
  printf("TraceByteNum = %ld TraceRangeNum = %ld TracePCNum = %ld \n", TraceByteNum, TraceRangeNum, TracePCNum);
  printf("ShdwPageSetNum = %ld \n", ShdwPageSet.size());
  //AddrSet.clear();
  //PageSet.clear();
  //AddrSetNum = 0;
#endif


  std::set<long>::iterator B, E;
  Trace *TracePtr;
  if( LDDProfLoopIter < 3 )
    return;

  //printf("ShdowBeg = %p, ShadowEnd = %p \n", (void*)ShadowLowBound, (void*)ShadowUpBound );
 // printf("Iter = %d, StackMax = %p, StackMin = %p HeapMax = %p HeapMin = %p GlobalMax = %p GlobaMin = %p\n", LDDProfLoopIter, LDDStackMax, LDDStackMin, LDDHeapMax, LDDHeapMin, LDDGlobalMax, LDDGlobalMin);
  printf("__addPresentToPrecedeTrace \n");


  // RB tree store the RW-Addr. 
  //__traverseRBTreeRWTrace(); 

  // Global, Heap, Statck(min, max) maitain the RW-Addr.
#ifndef _DDA_PA
  __traverseTriSectionRWTrace();
#endif

//#ifdef _OVERHEAD_PROF
#if 0
  float PresentTraceBuf = 0, PrecedeTraceBuf = 0, LCDepBuf = 0, LIDepBuf = 0;
  float AddrShadowBuf = 0, PCShadowBuf = 0;
  float Total;
  printf("after __addPresentToPrecedeTrace \n");
  PresentTraceBuf = (float)(LSPresentTraceMallocTime*sizeof(LSPresentRWAddrID))/(1024.0*1024.0);
  PrecedeTraceBuf = (float)(LSPrecedeTraceMallocTime*sizeof(LSPrecedeRWAddrID))/(1024.0*1024.0); 
  LCDepBuf = (float)(LCDepMallocTime*sizeof(LSDep))/(1024.0*1024.0) ; 
  LIDepBuf = (float)(LIDepMallocTime*sizeof(LSDep))/(1024.0*1024.0) ;
  AddrShadowBuf = (float)(LSAddrShadowTime*(sizeof(Trace)+ 8 + sizeof(LSPresentRWTrace)+sizeof(LSPrecedeRWTrace)))/(1024.0*1024.0);
  PCShadowBuf = (float)( LSPCShadowTime*sizeof(LSDepInfo) + 8 )/(1024.0*1024.0) ;
  Total = PresentTraceBuf + PrecedeTraceBuf + LCDepBuf + LIDepBuf + AddrShadowBuf + PCShadowBuf;

  printf("RWNum = %d \n", RWNum);
  printf("PresentTraceBuf = %f MB, PrecedeTracebuf = %f MB, LCDepBuf = %f MB, LIDepBuf = %f MB \n",
          PresentTraceBuf, PrecedeTraceBuf, LCDepBuf, LIDepBuf );
  printf("AddrShadowBuf = %f MB, PCShadowBuf = %f MB \n", AddrShadowBuf, PCShadowBuf);
  
  printf("TotalUpdateWrite = %ld, NWUpWrite = %ld \n", TotalUpWrite, NWTotalUpWrite);
  printf( "LSAddrShadowTime = %ld, LSPCShadowTime = %ld \n", LSAddrShadowTime, LSPCShadowTime );
  printf("LSPresentTraceMallocTime = %ld, LSPrecedeTraceMallocTime = %ld \n", LSPresentTraceMallocTime, LSPrecedeTraceMallocTime);
  printf("PresentBufFreeTime = %ld LSBufReuseTime = %ld TotalBuf = %f MB\n", LSPresentBufFreeTime, LSBufReuseTime, Total);
#endif
 
  return;
}

#ifndef _DDA_PA

// PresentWTrace: Last Write.
// 1) Addr->(PC, RWMask), every item maintains 
//
inline void __updatePresentWTrace(void *PC, void **ShadowAddr, unsigned char WriteMask,
                            LSPresentRWTrace &PRWTraceRef, int WTEmptyFlag )
{
  //return;
  unsigned char DifWriteMask, NewWriteMask, RemainWriteMask;
  unsigned char OldGlobalWriteMask = PRWTraceRef.GlobalWriteMask; 
  unsigned char NewGlobalWriteMask = PRWTraceRef.GlobalWriteMask = OldGlobalWriteMask | WriteMask;
  LSPresentRWAddrID *WIDPtr, *PrevWIDPtr;
  WIDPtr = PRWTraceRef.WTrace;

  // 1) Having New-Written bytes..
  if( WriteMask == (OldGlobalWriteMask ^ NewGlobalWriteMask) ){
    // 2 Opt me? Need merged with other ReadMask?
    if( LSPresentRWBuf != NULL ){
#ifdef _OVERHEAD_PROF
      LSBufReuseTime++;
#endif
      LSPresentRWAddrID *CurBuf;  
      CurBuf = LSPresentRWBuf->Next;
      LSPresentRWBuf->Next = PRWTraceRef.WTrace;
      PRWTraceRef.WTrace = LSPresentRWBuf;
      WIDPtr = LSPresentRWBuf;
      LSPresentRWBuf = CurBuf;  
    }
    else{
#ifdef _OVERHEAD_PROF
      LSPresentTraceMallocTime++;
#endif
      WIDPtr = (LSPresentRWAddrID*) malloc(sizeof(LSPresentRWAddrID));
      WIDPtr->Next = PRWTraceRef.WTrace;
      PRWTraceRef.WTrace = WIDPtr;
    }
#ifdef _OVERHEAD_PROF
    TraceByteNum += __maskToByteNum(WriteMask);
#endif
    WIDPtr->PC = (long)PC;
    WIDPtr->RWMask = WriteMask;  // bug?
  }

  // 2) Check overlap with all preceding write. FRLW
  // 1100->1111: del preceding write;
  // 1100->0110: del preced and add (1000, prev-pc), (0110, cur-pc);
  else{
    PrevWIDPtr = WIDPtr;
    NewWriteMask = WriteMask;
    for( ; WIDPtr != NULL; ){

      DifWriteMask = WIDPtr->RWMask ^ NewWriteMask;
      RemainWriteMask =  WIDPtr->RWMask & DifWriteMask; // WIDPtr Write-bytes before.
      // New Write-Mask, including written before part. 
      //NewWriteMask &= DifWriteMask;
      
      // case 1) No overlap: 1100->0011
      if( RemainWriteMask == WIDPtr->RWMask ){
        PrevWIDPtr = WIDPtr;
        WIDPtr = WIDPtr->Next;
        continue;
      }
      // bug? 20140123 Delete the cur node.
      // case 2) W1:0110->W2:0110, =
      // case 3) 0110->1110, <
      if( !(RemainWriteMask) ){
        // Not the first node.
        if( WIDPtr != PrevWIDPtr ){

          PrevWIDPtr->Next = WIDPtr->Next; // Delelete WIDPtr.
          // Add WIDPtr to the free list of present trace.
          #ifdef _OVERHEAD_PROF
          LSPresentBufFreeTime++;
          TraceByteNum -=__maskToByteNum(WIDPtr->RWMask);
          #endif
          WIDPtr->Next = LSPresentRWBuf;
          LSPresentRWBuf = WIDPtr;

          WIDPtr = PrevWIDPtr->Next; // fix me?
        }else{
          PRWTraceRef.WTrace = WIDPtr->Next; // Delete WIDPtr.
          #ifdef _OVERHEAD_PROF
          LSPresentBufFreeTime++;
          TraceByteNum -=__maskToByteNum(WIDPtr->RWMask);
          #endif
          WIDPtr->Next = LSPresentRWBuf;
          LSPresentRWBuf = WIDPtr;
        
          WIDPtr = PRWTraceRef.WTrace;
          PrevWIDPtr = WIDPtr;
        }
      }
      // case 4) 1100->0111, ><
      else{
#ifdef _OVERHEAD_PROF
        TraceByteNum -=__maskToByteNum(WIDPtr->RWMask ^ RemainWriteMask);
#endif
        WIDPtr->RWMask = RemainWriteMask;
      
        PrevWIDPtr = WIDPtr;
        WIDPtr = WIDPtr->Next;
      }
    }

    if ( NewWriteMask ){
      // 2) Get present WTrace buf.
      // 2.1) There are free WTrace bufs.
      if( LSPresentRWBuf != NULL ){
#ifdef _OVERHEAD_PROF
        LSBufReuseTime++; // prof TraceByteNum Bug?
#endif
        LSPresentRWAddrID *NextBuf = LSPresentRWBuf->Next;
        LSPresentRWBuf->Next = PRWTraceRef.WTrace;
        PRWTraceRef.WTrace = LSPresentRWBuf;
        WIDPtr = LSPresentRWBuf;
        LSPresentRWBuf = NextBuf;
      }
      // 2.2) No free buf.
      else{
#ifdef _OVERHEAD_PROF
        LSPresentTraceMallocTime++;
#endif
        WIDPtr = (LSPresentRWAddrID *)malloc(sizeof(LSPresentRWAddrID)); 
        WIDPtr->Next = PRWTraceRef.WTrace;  // Insert new buffer in the beginning. 
        PRWTraceRef.WTrace = WIDPtr;
      }
      // 3) Fill the content.
#ifdef _OVERHEAD_PROF
        TraceByteNum += __maskToByteNum(WriteMask); // may introduce redundant counts.
#endif
      WIDPtr->PC = (long) PC;
      WIDPtr->RWMask = WriteMask;
      //PRWTraceRef.WTrace.insert( std::make_pair(WriteMask, (long)PC) ); 
    }
  }

  return;
}

// Look for Loop independent output dependences. 
// Todo:
// 1) Addr1: W R R R R ===> not need to check True-dep every time.
// LIOD & LIAD.
#ifdef __ADDRKEY
inline
void __doWriteLIDWithAddrAsKey(void * PC, unsigned char *WriteMask, 
                        LSPresentRWTrace &PRWTraceRef, Trace &TraceRef)
{
  unsigned char RWMask;
  unsigned char NewRWMask;
  

  if( (RWMask = ( *WriteMask & PRWTraceRef.GlobalWriteMask)) ) {
    int EleNum = 0, EleTotal = PRWTraceRef.WriteNum;
    LSPresentRWAddrID *WIDPtr = PRWTraceRef.WTrace;

    for( ; EleNum < EleTotal && RWMask; ++EleNum, WIDPtr = WIDPtr->Next){
      NewRWMask = RWMask;
      RWMask &= RWMask ^ WIDPtr->RWMask;

      // LI-Output.
      if( RWMask != NewRWMask ){
        LSLIDep* LIDepOut = TraceRef.LIDep->OutDep;
        while( LIDepOut != NULL){
          if( LIDepOut->SrcPC == WIDPtr->PC && LIDepOut->SinkPC == (long)PC )
            break;
          LIDepOut = LIDepOut->next;
        }

        if( LIDepOut == NULL ){
          LIDepOut = (LSLIDep*) malloc( sizeof(LSLIDep));
          LIDepOut->next = TraceRef.LIDep->OutDep;
          TraceRef.LIDep->OutDep = LIDepOut;
          LIDepOut->SrcPC = WIDPtr->PC;
          LIDepOut->SinkPC = (long)PC;
          LDDLSResultTrace.insert((long)&TraceRef);
        }
        
      }
    } 

  }
  // LIAD.
  if( (RWMask = ( *WriteMask & PRWTraceRef.GlobalReadMask)) ) {
    //std::map<unsigned char, long>::iterator RTBegin, RTEnd;
    //RTBegin = PRWTraceRef.RTrace.begin();
    //RTEnd = PRWTraceRef.RTrace.end();

    int EleNum = 0, EleTotal = PRWTraceRef.ReadNum;
    LSPresentRWAddrID *RIDPtr = PRWTraceRef.RTrace;

    for( ; EleNum < EleTotal  && RWMask; ++EleNum, RIDPtr = RIDPtr->Next){
      NewRWMask = RWMask;
      RWMask &= RWMask ^ RIDPtr->RWMask;

        // LI-AntiDep.
      if( RWMask != NewRWMask ){
         LSLIDep* LIDepAnti = TraceRef.LIDep->AntiDep;
        while( LIDepAnti != NULL){
          if( LIDepAnti->SrcPC == RIDPtr->PC && LIDepAnti->SinkPC == (long)PC )
            break;
          LIDepAnti = LIDepAnti->next;
        }

        if( LIDepAnti == NULL ){
          LIDepAnti = (LSLIDep*) malloc( sizeof(LSLIDep));
          LIDepAnti->next = TraceRef.LIDep->AntiDep;
          TraceRef.LIDep->AntiDep = LIDepAnti;
          LIDepAnti->SrcPC = RIDPtr->PC;
          LIDepAnti->SinkPC = (long)PC;
        }   

      }
    } 

  }

  return;

}
#endif

// Look for Loop independent output dependences. 
// Todo:
// 1) Addr1: W R R R R ===> not need to check True-dep every time.
// LIOD & LIAD.
// 2) The results are stored in the ShadowAddr(SinkPC).
// 3) Deps(LSLID, LSLCD);
inline
void __doWriteLIDWithSinkPCAsKey(void * PC, unsigned char *WriteMask, 
                        LSPresentRWTrace &PRWTraceRef, Trace &TraceRef)
{
  unsigned char RWMask;
  unsigned char NewRWMask;

  Deps **DepShadowPtr;
  LSLIDepInfo *LIDepPtr;
  Deps *DepsPtr;
  DepShadowPtr = (Deps **)AppAddrToShadow((long)PC);
  if( *DepShadowPtr == NULL ){
    DepsPtr = (Deps*) malloc(sizeof(Deps));
    *DepShadowPtr = DepsPtr;
    DepsPtr->LIDep = (LSLIDepInfo*) malloc( sizeof(LSLIDepInfo) );
    DepsPtr->LIDep->TrueDep = NULL;
    DepsPtr->LIDep->AntiDep = NULL;
    DepsPtr->LIDep->OutDep = NULL;

  } 
  LIDepPtr =(*DepShadowPtr)->LIDep;

  if( (RWMask = ( *WriteMask & PRWTraceRef.GlobalWriteMask)) ) {
    LSPresentRWAddrID *WIDPtr = PRWTraceRef.WTrace;
    for( ; (WIDPtr != NULL) && RWMask; WIDPtr = WIDPtr->Next){
      NewRWMask = RWMask;
      RWMask &= RWMask ^ WIDPtr->RWMask;

      // LI-Output.
      if( RWMask != NewRWMask ){
        LSLIDep* LIDepOut = LIDepPtr->OutDep;
        while( LIDepOut != NULL){
          if( LIDepOut->SrcPC == WIDPtr->PC && LIDepOut->SinkPC == (long)PC )
            break;
          LIDepOut = LIDepOut->next;
        }

        if( LIDepOut == NULL ){
          LIDepOut = (LSLIDep*) malloc( sizeof(LSLIDep));
          LIDepOut->next = LIDepPtr->OutDep;
          LIDepPtr->OutDep = LIDepOut;
          LIDepOut->SrcPC = WIDPtr->PC;
          LIDepOut->SinkPC = (long)PC;
          LDDLSResultTrace.insert((long)&TraceRef);
        }
      }
    } 
  }


  // LIAD.
  if( (RWMask = ( *WriteMask & PRWTraceRef.GlobalReadMask)) ) {
    //std::map<unsigned char, long>::iterator RTBegin, RTEnd;
    //RTBegin = PRWTraceRef.RTrace.begin();
    //RTEnd = PRWTraceRef.RTrace.end();

    LSPresentRWAddrID *RIDPtr = PRWTraceRef.RTrace;

    for( ; (RIDPtr != NULL)  && RWMask; RIDPtr = RIDPtr->Next){
      NewRWMask = RWMask;
      RWMask &= RWMask ^ RIDPtr->RWMask;

      // LI-AntiDep.
      if( RWMask != NewRWMask ){
        LSLIDep* LIDepAnti = LIDepPtr->OutDep;
        while( LIDepAnti != NULL){
          if( LIDepAnti->SrcPC == RIDPtr->PC && LIDepAnti->SinkPC == (long)PC )
            break;
          LIDepAnti = LIDepAnti->next;
        }

        if( LIDepAnti == NULL ){
          LIDepAnti = (LSLIDep*) malloc( sizeof(LSLIDep));
          LIDepAnti->next = LIDepPtr->AntiDep;
          LIDepPtr->AntiDep = LIDepAnti;
          LIDepAnti->SrcPC = RIDPtr->PC;
          LIDepAnti->SinkPC = (long)PC;
        }   

      }
    } 

  }

  return;

}

// Look for Loop independent output dependences. 
// Todo:
// 1) Addr1: W R R R R ===> not need to check True-dep every time.
// LIOD & LIAD.
// 2) The results are stored in the ShadowAddr(SinkPC).
// 3) Dist[0]->LIDep, Dist[1]->LCDep;
inline
void __doWriteLIDWithSinkPCAsKeyLICDep(void * PC, unsigned char *WriteMask, 
                        LSPresentRWTrace &PRWTraceRef, Trace &TraceRef)
{
  unsigned char RWMask;
  unsigned char NewRWMask;

  LSDepInfo **DepShadowPtr;
  LSDepInfo *LIDepPtr;
  DepShadowPtr = (LSDepInfo **)AppAddrToShadow((long)PC);
  if( *DepShadowPtr == NULL ){
#ifdef _OVERHEAD_PROF
  LSPCShadowTime++;
  LCDepInfoMallocTime++;

  DepRangeNum++;
#endif
    LIDepPtr = (LSDepInfo*) malloc(sizeof(LSDepInfo));
    *DepShadowPtr = LIDepPtr;
    LIDepPtr->TrueDep = NULL;
    LIDepPtr->AntiDep = NULL;
    LIDepPtr->OutDep = NULL;
//#ifdef _DDA_PA
#if 0
    if( (long)DepShadowPtr > LSLIDepSinkPCMax )
      LSLIDepSinkPCMax = (long)DepShadowPtr;
    else if( (long)DepShadowPtr < LSLIDepSinkPCMin)
      LSLIDepSinkPCMin = (long) DepShadowPtr;
#endif
    if( (long)DepShadowPtr > LDDLSResultMax )
      LDDLSResultMax = (long)DepShadowPtr;
    else if( (long)DepShadowPtr < LDDLSResultMin)
      LDDLSResultMin = (long) DepShadowPtr;

  } 
  LIDepPtr =*DepShadowPtr;

  if( (RWMask = ( *WriteMask & PRWTraceRef.GlobalWriteMask)) ) {

    LSPresentRWAddrID *WIDPtr = PRWTraceRef.WTrace;
    for( ; (WIDPtr != NULL ) && RWMask; WIDPtr = WIDPtr->Next){
      NewRWMask = RWMask;
      RWMask &= RWMask ^ WIDPtr->RWMask;
      // LI-Output.
      if( RWMask != NewRWMask ){
        LSDep* LIDepOut = LIDepPtr->OutDep;
        while( LIDepOut != NULL){
          if( LIDepOut->SrcPC == WIDPtr->PC && LIDepOut->SinkPC == (long)PC ){
            if( LIDepOut->Dist[0] != 0 )
#ifdef _OVERHEAD_PROF
  LIDepNum++;
#endif
              LIDepOut->Dist[0] = 0;
            break;
          }
          LIDepOut = LIDepOut->next;
        }
        // find the node, but the node store LCD only.
        if( LIDepOut == NULL ){
#ifdef _OVERHEAD_PROF
  LIDepMallocTime++;

  LIDepNum++;
#endif
          LIDepOut = (LSDep*) malloc( sizeof(LSDep));
          LIDepOut->next = LIDepPtr->OutDep;
          LIDepPtr->OutDep = LIDepOut;
          LIDepOut->SrcPC = WIDPtr->PC;
          LIDepOut->SinkPC = (long)PC;
          LIDepOut->Dist[0] = 0;
          LIDepOut->Dist[1] = 0;
          //LDDLSResultTrace.insert((long)&TraceRef);
        }
      }
    } 
  }


  // LIAD.
  if( (RWMask = ( *WriteMask & PRWTraceRef.GlobalReadMask)) ) {

    LSPresentRWAddrID *RIDPtr = PRWTraceRef.RTrace;
    for( ; (RIDPtr != NULL) && RWMask; RIDPtr = RIDPtr->Next){
      NewRWMask = RWMask;
      RWMask &= RWMask ^ RIDPtr->RWMask;

      // LI-AntiDep.
      if( RWMask != NewRWMask ){
        LSDep* LIDepAnti = LIDepPtr->AntiDep;
        while( LIDepAnti != NULL){
          if( LIDepAnti->SrcPC == RIDPtr->PC && LIDepAnti->SinkPC == (long)PC ){
            if( LIDepAnti->Dist[0] != 0 )
#ifdef _OVERHEAD_PROF
  LIDepNum++;
#endif
              LIDepAnti->Dist[0] = 0;
            break;
          }
          LIDepAnti = LIDepAnti->next;
        }

        if( LIDepAnti == NULL ){
#ifdef _OVERHEAD_PROF
  LIDepMallocTime++;
  LIDepNum++;
#endif
          LIDepAnti = (LSDep*) malloc( sizeof(LSDep));
          LIDepAnti->next = LIDepPtr->AntiDep;
          LIDepPtr->AntiDep = LIDepAnti;
          LIDepAnti->SrcPC = RIDPtr->PC;
          LIDepAnti->SinkPC = (long)PC;
          LIDepAnti->Dist[0] = LIDepAnti->Dist[1] = 0;
        }   
      }
    } 

  }

  return;
}


inline
void __determinWriteLID(void * PC, unsigned char *WriteMask, 
                        LSPresentRWTrace &PRWTraceRef, Trace &TraceRef)
{


  // 1) Take as the key the Write-Address to store data dependence results.
  #ifdef __ADDRKEY
  __doWriteLIDWithAddrAsKey(PC, WriteMask, PRWTraceRef, TraceRef);
  #endif
  // 2) Take as the key the SrcPC of dependence pair to store data dependence
  // results.
  //__doWriteLIDWithSinkPCAsKey( PC, WriteMask, PRWTraceRef, TraceRef);
  
  // 3)
  __doWriteLIDWithSinkPCAsKeyLICDep( PC, WriteMask, PRWTraceRef, TraceRef);

  return;
}


// Present[1] when serial execution; Present[IterationNO] when parallel 
// execution. 
// 
void __checkLIDandUpdatePresentWTrace(void *PC, void **ShadowAddr, 
                     unsigned char * WriteMask, int LoopIter)
{
  // 1) Get the Present buf.
  // 1.1) First access the address, so create a new present/precede buffer.
  if( *ShadowAddr == NULL ){
    Trace *TracePtr = (Trace*) malloc(sizeof(Trace));
    *ShadowAddr = TracePtr;

#ifdef _OVERHEAD_PROF
  LSAddrShadowTime++;
  TraceRangeNum++;
#endif

  #ifdef __ADDRKEY
    TracePtr->LIDep = (LSLIDepInfo*) malloc ( sizeof(LSLIDepInfo) );
    TracePtr->LIDep->AntiDep = NULL;
    TracePtr->LIDep->OutDep = NULL;
    TracePtr->LIDep->TrueDep = NULL;

    TracePtr->LCDep = (LSLCDepInfo*) malloc ( sizeof(LSLCDepInfo) );
    TracePtr->LCDep->AntiDep = NULL;
    TracePtr->LCDep->OutDep = NULL;
    TracePtr->LCDep->TrueDep = NULL;
  #endif

    TracePtr->Precede =(LSPrecedeRWTrace*) malloc(sizeof(LSPrecedeRWTrace)); 
    TracePtr->Precede->RTrace = NULL;
    TracePtr->Precede->WTrace = NULL;
  
    TracePtr->Precede->GlobalReadMask = TracePtr->Precede->GlobalWriteMask = 0;

    TracePtr->Present  =(LSPresentRWTrace*) malloc(sizeof(LSPresentRWTrace)); 
    TracePtr->Present->RTrace = NULL;
    TracePtr->Present->WTrace = NULL;
    TracePtr->Present->GlobalReadMask = 0;
    TracePtr->Present->GlobalWriteMask = 0;
    TracePtr->Present->RWIter = LoopIter;
}
   // 1.2) App has read/written the address before.
  LSPresentRWTrace & PRWTraceRef = *((Trace*)(*ShadowAddr))->Present;
  Trace &TraceRef =  *((Trace*)(*ShadowAddr));
  // 1.2.1) Write in a new iteration and create a new buf.
  if( PRWTraceRef.RWIter != LoopIter ) {
    // Opt me?
    // Reset tags of Present-Read-Trace.
    //PRWTraceRef.RTrace.clear();
    PRWTraceRef.GlobalReadMask = 0;
    PRWTraceRef.GlobalWriteMask = 0;
    PRWTraceRef.RWIter = LoopIter;
  }
// 1.2.2) Still in the same iteration.
// do-nothing.


  
  // 4) There are maybe dependences.

  // Not write to the Address before within the current interation.
if( PRWTraceRef.GlobalReadMask )
  __determinWriteLID(PC, WriteMask, PRWTraceRef, TraceRef);

  __updatePresentWTrace(PC, ShadowAddr, *WriteMask, PRWTraceRef, 0);

  return;
}

void __checkLIDandUpdatePresentWTrace2(void *PC, void** ShadowAddr,
                                       unsigned char * WriteMask1, 
                                       unsigned char *WriteMask2, int LoopIter)
{
  __checkLIDandUpdatePresentWTrace(PC, ShadowAddr, 
                                     WriteMask1, LoopIter); 
  ShadowAddr++; // Fix me?
  __checkLIDandUpdatePresentWTrace(PC, ShadowAddr,  
                                    WriteMask2, LDDProfLoopIter); 
}
#endif


//
void __checkStoreLShadow(int *Ptr, long AddrLen)
{
#ifdef _DDA_PP
  if( !LDDProfCurIter ) 
    return;
#else
  if( !LDDProfLoopID ) 
    return;
#endif
  void* pc;  // TBD. Take as global variable is better.
  pc  = __builtin_return_address(0);  

#ifdef _DDA_PA
  GenerateAddrStream( Ptr, AddrLen ,  LDDProfLoopIter, 1, (void*)pc);
  return; // OPT-0-1
#ifndef _DDA_PP
#ifdef _TIME_PROF
    tbb::tick_count tstart, tend, pststart;
    pststart = tbb::tick_count::now();
#endif
    //std::cout<<"GeneratingAddrStream: LoopIter = " << LoopIter <<std::endl; 

    // case 1: A new iteration.
    // 1.1 Label the Streams[] is full;
    // 1.2 Sent StartLCDA = 1;
    // 1.3 Create new Streams[] to insert the new iteration's trace.
    if( LDDProfLoopIter != CurLoopIter){
      // The old stream is not full.
      if( WItemNum ){
        // Set the end flag of old stream.
        Streams[WStreamNum].LoopIter = CurLoopIter;
        Streams[WStreamNum].End = WItemNum;
        StreamIsFull[WStreamNum] = 1;
#ifdef _DEBUGINFO
        std::cerr<<std::setbase(10)<<"GenerateAddrStream WStreamNum =  " << WStreamNum  << " is full"<< "\n";
#endif
        // 1.1) New stream to start LCDA. 
        WStreamNum = (WStreamNum+1) % STREAMNUM;
      }
      // else The old stream is empty;

      // Create the new Streams for keep LCDA info. 
#ifdef _TIME_PROF
      tstart = tbb::tick_count::now();
#endif
      while( StreamIsFull[WStreamNum] ) ;
#ifdef _TIME_PROF
      GPTime += (tbb::tick_count::now() - tstart).seconds();
#endif

      Streams[WStreamNum].Items[0].RWFlag = 5; // To enable LCDA.  
      Streams[WStreamNum].LoopIter = CurLoopIter;
      StreamIsFull[WStreamNum] = 1;
      std::cerr<<std::setbase(10)<<"GenerateAddrStream CurLoopIter =  " << CurLoopIter << ", LoopIter = "<< LDDProfLoopIter << " is full. doLCDA"<< "\n";
#ifdef _DEBUGINFO
      std::cerr<<std::setbase(10)<<"GenerateAddrStream WStreamNum =  " << WStreamNum  << " is full. doLCDA"<< "\n";
#endif

      // 1.2) New stream to store RWTrace.
      WStreamNum = (WStreamNum+1) % STREAMNUM;
      WItemNum = 0;
      CurLoopIter = LDDProfLoopIter;
    }

    // Case2: put more data into the Streams[].
    //  Wait until the buf is not full. Need?
    // Case2.1: WItemNum == 0, insert the first element into the Streams[].
    if( !WItemNum  ){

#ifdef _TIME_PROF
      tstart = tbb::tick_count::now();
#endif
      while( StreamIsFull[WStreamNum] ) ; // Maybe not need?
#ifdef _TIME_PROF
      GPTime += (tbb::tick_count::now() - tstart).seconds();
#endif

    }
    Streams[WStreamNum].Items[WItemNum].Addr = Ptr; 
    Streams[WStreamNum].Items[WItemNum].AddrLen = AddrLen;
    Streams[WStreamNum].Items[WItemNum].RWFlag = 1;
    Streams[WStreamNum].Items[WItemNum].PC = pc;
    WItemNum++;

    // case 2.2: The stream is full. 
    if( WItemNum == STREAMITEMNUM ){
      Streams[WStreamNum].LoopIter = CurLoopIter;
      Streams[WStreamNum].End = WItemNum;
      StreamIsFull[WStreamNum] = 1;
#ifdef _DEBUGINFO
      std::cerr<<std::setbase(10)<<"GenerateAddrStream WStreamNum =  " << WStreamNum  << " is full"<< "\n";
#endif
      WStreamNum = (WStreamNum + 1)% STREAMNUM;
      WItemNum = 0;
    }


#ifdef _TIME_PROF
      GTime += (tbb::tick_count::now() - pststart).seconds();
#endif
  return;
#endif
#endif // end of _DDA_PA

  void **ShadowAddr = 0;
  int retval;
  long AddrInnerOffset = 0, Len1 = 0,  Len2 = 0;
  unsigned char WriteMask1 = 0, WriteMask2 = 0;

  ShadowAddr = (void**)AppAddrToShadow( (long)Ptr );
  //printf("store = %p pc = %p\n", ShadowAddr, pc );
  
  // RWTrace-1:
  // LDDRWTrace.insert((long)ShadowAddr);
  
  // RWTrace-2:
  // RBInsertVal(LDDRWTrace, (long) ShadowAddr);

  // RWTrace-3:
 #if 0
  long Addr = (long) ShadowAddr;
  // 0x7fff00000000
  if ( Addr > StackBeg ){
    if( Addr < LDDStackMin )
      LDDStackMin = Addr;
    else if ( Addr > LDDStackMax )
      LDDStackMax = Addr;
  }
  // 0x7cf000400000
  else if( Addr > GlobalEnd ){
    if( Addr > LDDHeapMax)
      LDDHeapMax = Addr;
    else if( Addr < LDDHeapMin )
      LDDHeapMin = Addr;
  }
  else{
    if( Addr > LDDGlobalMax)
      LDDGlobalMax = Addr;
    else if( Addr < LDDGlobalMin )
      LDDGlobalMin = Addr;
  }
#endif

#ifdef _OVERHEAD_PROF
 // for(int i = 0; i < AddrLen; i++)
{
  //PageSet.insert((long)ShadowAddr/PageSize);    
;//AddrSetNum += AddrLen;
    ; //AddrSet.insert((long)ShadowAddr+i);
  }
#endif

  // if( LDDProfLoopIter == 9 )
  //
#ifdef _PRINTF
  printf("__checkStoreLShadow WriteAddr = %p ShadowAddr = %p PC = %p iter= %d \n",Ptr, ShadowAddr, pc, LDDProfLoopIter);
#endif
  retval = __determinInner8bytesInfo( Ptr, AddrLen,  
       &WriteMask1,  &WriteMask2 );

  // RWTrace-4. ShadowAddr range. And ShadowAddr won't cross Virtual pages.
long PageNum = (long)ShadowAddr/PageSize;
#if 1
if( CacheBuf[0] == PageNum || CacheBuf[1] == PageNum || 
    CacheBuf[2] == PageNum  || CacheBuf[3] == PageNum || 
    CacheBuf[4] == PageNum  || CacheBuf[5] == PageNum ||
    CacheBuf[6] == PageNum  || CacheBuf[7] == PageNum  ||
    CacheBuf[8] == PageNum )
goto NEXT;
#endif

#if 0
  if( ShdwPageSet.find(PageNum) == ShdwPageSet.end() ){
    TotalPageNum++;
    printf("TotalPageNum = %d num = %ld\n", TotalPageNum, PageNum);
  }
#endif
ShdwPageSet.insert(PageNum);
if( 2 == retval  )
  ShdwPageSet.insert(PageNum);
  CacheBuf[CacheBufNum] = PageNum;
  CacheBufNum = (++CacheBufNum) >= CBUFLEN ? 0 : CacheBufNum;
NEXT:

  //retval = __determinInner8bytesInfo( Ptr, AddrLen, &AddrInnerOffset, 
   //   &Len1, &WriteMask1, &Len2, &WriteMask2 );

  // Compute the shadow cell address. 
  // The accessed space crosses the bounder of 8bytes.
  if( 1 == retval ){
    __checkLIDandUpdatePresentWTrace((void*)pc, ShadowAddr, 
        &WriteMask1, LDDProfLoopIter); 
  }
  else{
    __checkLIDandUpdatePresentWTrace2((void*)pc, ShadowAddr,  
         &WriteMask1, &WriteMask2, LDDProfLoopIter); 
  }

  return ;
}

/* Debug use.
 *
*/
void __checkStoreLShadowDebug(int *ptr, long AddrLen, char *AddrPos, char *VarName)
{

  if ( (long)ptr > 0x7fffffffffff || (long)ptr < 0x70000000000 )
    std::cout<<"Store(): VarName = "<< VarName << ", AddrPos = " << AddrPos << "\n";
  return;
}

void __startNewIteration()
{

  return;
}


//
// The address introduce dependence in std::set<long>::iterator LDDLSResultTrace.
void __outputLSResultWithAddrAsKey()
{
#if 0
  std::fstream fs;
  fs.open("dda-result", std::fstream::out);
  if( !fs.is_open() )
    std::cout<<"cannot open file dda-result" << "\n";

  printf("__outputLDDDependenceLShadow LDDLSResultTrace.size = %d\n", LDDLSResultTrace.size()  );
  std::set<long>::iterator LDDB, LDDE;
  LDDB = LDDLSResultTrace.begin();
  LDDE = LDDLSResultTrace.end();
  for( ; LDDB != LDDE; ++LDDB ){
    Trace *TracePtr =(Trace*) *LDDB;

  // 1) Output LITD.
    LSLIDep *LITD = TracePtr->LIDep->TrueDep; 
    while(LITD){
      //printf("TracePtr = %p SrcPC = %p, SinkPC = %p LITD\n", TracePtr, (void*)LITD->SrcPC, (void*)LITD->SinkPC);
      fs<< (void*)LITD->SrcPC << "  " << (void*)LITD->SinkPC<<"  LITD\n";
      LITD = LITD->next;
    }

  // 2) Output LIAD.
  LSLIDep *LIAD = TracePtr->LIDep->AntiDep; 
    while(LIAD){
      //printf("SrcPC = %p, SinkPC = %p LIAD\n", (void*)LIAD->SrcPC, (void*)LIAD->SinkPC);
      fs<< (void*)LIAD->SrcPC << "  " << (void*)LIAD->SinkPC<<"  LIAD\n";
      LIAD = LIAD->next;
    }

    // 3) Output LIOD.
    LSLIDep *LIOD = TracePtr->LIDep->OutDep; 
    while(LIOD){
      //printf("SrcPC = %p, SinkPC = %p LIOD\n",(void*) LIOD->SrcPC,(void*) LIOD->SinkPC);
      fs<< (void*)LIOD->SrcPC << "  " << (void*)LIOD->SinkPC<<"  LIOD\n";
      LIOD = LIOD->next;
    }

  // 4) Output LCTD.
    LSLCDep *LCTD = TracePtr->LCDep->TrueDep;
    while( LCTD){
      LSDepDist *LCDDist = LCTD->DepDist;
      while( LCDDist){
        //printf("SrcPC = %p, SinkPC = %p Dist = %d LCTD\n",(void*) LCTD->SrcPC,(void*) LCTD->SinkPC, *LCDDist);
        fs<< (void*)LCTD->SrcPC << "  " << (void*)LCTD->SinkPC <<"  "<< LCDDist->DepDistVal <<"  LCTD\n";
        LCDDist = LCDDist->next;
      }
      LCTD = LCTD->next;
    }

  // 5) Output LCAD.
  LSLCDep *LCAD = TracePtr->LCDep->AntiDep;
    while( LCAD){
      LSDepDist *LCDDist = LCAD->DepDist;
      while( LCDDist){

        //printf("SrcPC = %p, SinkPC = %p Dist = %d LCAD\n",(void*) LCAD->SrcPC,(void*) LCAD->SinkPC, *LCDDist);
        fs<< (void*)LCAD->SrcPC << "  " << (void*)LCAD->SinkPC<<"  "<< LCDDist->DepDistVal <<"  LCAD\n";
        LCDDist = LCDDist->next;
      }
      LCAD = LCAD->next;
    }

  // 6) Output LCOD.
   LSLCDep *LCOD = TracePtr->LCDep->OutDep;
    while( LCOD){
      LSDepDist *LCDDist = LCOD->DepDist;
      while( LCDDist){

        //printf("SrcPC = %p, SinkPC = %p Dist = %d LCOD\n",(void*) LCOD->SrcPC,(void*) LCOD->SinkPC, *LCDDist);
        fs<< (void*)LCOD->SrcPC << "  " << (void*)LCOD->SinkPC<<"  "<< LCDDist->DepDistVal <<"  LCOD\n";
        LCDDist = LCDDist->next;
      }
      LCOD = LCOD->next;
    }
     
  }

  return ;
#endif
}


// (LDDLSResultMin, LDDLSResultMax)
//
void  __outputLSResultWithSinkPCAsKey()
{
  std::fstream fs;
  fs.open("dda-result", std::fstream::out);
  LSDep *LSDepPtr;
  LSDepInfo *LSDepInfoPtr;
  if( !fs.is_open() )
    std::cout<<"cannot open file dda-result" << "\n";

  //printf("__outputLDDDependenceLShadow LDDLSResultTrace.size = %d\n", LDDLSResultTrace.size()  );
  //printf("__outputLSResultWithSinkPCAsKey() LDDLSResultMin = %p, LDDLSResultMax = %p\n", LDDLSResultMin, LDDLSResultMax  );


#ifdef _OVERHEAD_PROF
  float PresentTraceBuf = 0, PrecedeTraceBuf = 0, LCDepBuf = 0, LIDepBuf = 0;
  float LIDepInfo = 0, LCDepInfo = 0;
  float AddrShadowBuf = 0, PCShadowBuf = 0, Total = 0;
  
  int AddrShadowSize = sizeof(Trace) + 8 + sizeof(LSPresentRWTrace) + sizeof(LSPrecedeRWTrace);
  printf("AddrShadowSize = %d \n", AddrShadowSize);
  AddrShadowBuf = (float)(LSAddrShadowTime*(sizeof(Trace)+ 8 + sizeof(LSPresentRWTrace)+sizeof(LSPrecedeRWTrace)))/(1024.0*1024.0);
  PresentTraceBuf = (float)(LSPresentTraceMallocTime*sizeof(LSPresentRWAddrID))/(1024.0*1024.0);
  PrecedeTraceBuf = (float)(LSPrecedeTraceMallocTime*sizeof(LSPrecedeRWAddrID))/(1024.0*1024.0); 

  PCShadowBuf = (float)( LSPCShadowTime*(sizeof(LSDepInfo) + 8) )/(1024.0*1024.0) ;
  LCDepBuf = (float)(LCDepMallocTime*sizeof(LSDep))/(1024.0*1024.0) ; 
  LIDepBuf = (float)(LIDepMallocTime*sizeof(LSDep))/(1024.0*1024.0) ;
  Total = PresentTraceBuf + PrecedeTraceBuf + LCDepBuf + LIDepBuf + AddrShadowBuf + PCShadowBuf;
  // LCDepInfo + LSDepInfo

#ifndef _DDA_PA
  printf("RWNum = %d \n", RWNum);
#endif
  printf("LSPresentRWAddrID_size = %d LSDep = %d, LSPresentRWTrace = %d, LSPrecedeRWTrace = %d, Trace = %d\n", sizeof(LSPresentRWAddrID),
       sizeof(LSDep), sizeof(LSPresentRWTrace), sizeof(LSPrecedeRWTrace), sizeof(Trace) );
  printf("PresentTraceBuf = %f MB, PrecedeTracebuf = %f MB, LCDepBuf = %f MB, LIDepBuf = %f MB \n",
          PresentTraceBuf, PrecedeTraceBuf, LCDepBuf, LIDepBuf );
  printf("AddrShadowBuf = %f MB, PCShadowBuf = %f MB \n", AddrShadowBuf, PCShadowBuf);
  
  printf("TotalUpdateWrite = %ld, NWUpWrite = %ld \n", TotalUpWrite, NWTotalUpWrite);
  printf( "LSAddrShadowTime = %ld, LSPCShadowTime = %ld \n", LSAddrShadowTime, LSPCShadowTime );
  printf("LSPresentTraceMallocTime = %ld, LSPrecedeTraceMallocTime = %ld \n", LSPresentTraceMallocTime, LSPrecedeTraceMallocTime);
  printf("PresentBufFreeTime = %ld LSBufReuseTime = %ld TotalBuf = %f\n", LSPresentBufFreeTime, LSBufReuseTime, Total);
#endif
  //
  long LDDB, LDDE;
#ifdef _DDA_PA
  while( !LSOutputLCDepFinish ) 
    ; //std::cout<<" OutputLSResultWithSinkPCAsKey Wait for the pipeline to output LSDep info " << "\n";
  printf("__outputLSResultWithSinkPCAsKey() LSLCDepSinkPCMin = %p, LSLCDepSinkPCMax = %p\n", LSLCDepSinkPCMin, LSLCDepSinkPCMax  );
  LDDB = LSLCDepSinkPCMin;
  LDDE = LSLCDepSinkPCMax;
#else
  LDDB = LDDLSResultMin;
  LDDE = LDDLSResultMax;
#endif
  std::cout<< "MinSinkPC " << LDDB << ", MaxSinkPC " << LDDE << "\n";
  for( ; LDDB <= LDDE; LDDB += 8 ){
    LSDepInfoPtr =*((LSDepInfo**) LDDB);
    //printf("LDDB = %p \n", (void*)LSDepInfoPtr);
    if( LSDepInfoPtr == NULL )
      continue;

  // 1) Output LI/LC TD.
    LSDepPtr = LSDepInfoPtr->TrueDep; 
    while(LSDepPtr){
      if( LSDepPtr->Dist[0] == 0){
        //printf("ShadowSinkPC = %p SrcPC = %p, SinkPC = %p LITD\n", (void*)LDDB, (void*)LSDepPtr->SrcPC, (void*)LSDepPtr->SinkPC);
        fs<< std::setbase(16) <<  LSDepPtr->SrcPC << "  " << LSDepPtr->SinkPC<<"  LITD\n";
      }
      if( LSDepPtr->Dist[1] != 0 ){
        //printf("ShadowSinkPC = %p SrcPC = %p, SinkPC = %p dist = %d addr = %p LCTD\n", (void*)LDDB, (void*)LSDepPtr->SrcPC, (void*)LSDepPtr->SinkPC, LSDepPtr->Dist[1], LSDepPtr);
        fs<< std::setbase(16) <<  LSDepPtr->SrcPC << "  " << LSDepPtr->SinkPC<<" " <<std::setbase(10) <<  LSDepPtr->Dist[1] << "  LCTD\n";
      }
      LSDepPtr = LSDepPtr->next;
    }

  // 2) Output LI/LC AD.
    LSDepPtr = LSDepInfoPtr->AntiDep; 
    while(LSDepPtr){
      if( LSDepPtr->Dist[0] == 0){
        //printf("ShadowSinkPC = %p SrcPC = %p, SinkPC = %p LIAD\n", (void*)LDDB, (void*)LSDepPtr->SrcPC, (void*)LSDepPtr->SinkPC);
        fs<< std::setbase(16)<< LSDepPtr->SrcPC << "  " << LSDepPtr->SinkPC<<"  LIAD\n";
      }
      if( LSDepPtr->Dist[1] != 0 ){
        //printf("ShadowSinkPC = %p SrcPC = %p, SinkPC = %p dist = %d addr = %p LCAD\n", (void*)LDDB, (void*)LSDepPtr->SrcPC, (void*)LSDepPtr->SinkPC, LSDepPtr->Dist[1], LSDepPtr);
        fs<< std::setbase(16) <<  LSDepPtr->SrcPC << "  " << LSDepPtr->SinkPC<<" " << std::setbase(10) << LSDepPtr->Dist[1] << "  LCAD\n";
      }
      LSDepPtr = LSDepPtr->next;
    }

    // 3) Output LI/LC OD.
    LSDepPtr = LSDepInfoPtr->OutDep; 
    while(LSDepPtr){
      if( LSDepPtr->Dist[0] == 0){
        //printf("ShadowSinkPC = %p SrcPC = %p, SinkPC = %p LIOD\n", (void*)LDDB, (void*)LSDepPtr->SrcPC, (void*)LSDepPtr->SinkPC);
        fs<<std::setbase(16) << LSDepPtr->SrcPC << "  " << LSDepPtr->SinkPC<<"  LIOD\n";
      }
      if( LSDepPtr->Dist[1] != 0 ){
        //printf("ShadowSinkPC = %p SrcPC = %p, SinkPC = %p dist = %d addr = %p LCOD\n", (void*)LDDB, (void*)LSDepPtr->SrcPC, (void*)LSDepPtr->SinkPC, LSDepPtr->Dist[1], LSDepPtr);
        fs<< std::setbase(16) << LSDepPtr->SrcPC << "  " << LSDepPtr->SinkPC<<" " <<std::setbase(10) <<  LSDepPtr->Dist[1] << "  LCOD\n";
      }
      LSDepPtr = LSDepPtr->next;
    }

  }

  return ;
}

void __outputLDDDependenceLShadow()
{
  //printf("pcmin = %p, pcmax = %p \n",(void*) LDDLSResultMin, (void*)LDDLSResultMax);
  // 1) the dependence results are stored in LDDLSResultTrace.
  //__outputLSResultWithAddrAsKey();

  //
  std::cout<<"__outputLDDDependenceLShadow \n";
#ifdef _DDA_PA
  LDDPACancelPipeline();
#endif

  // 2) The dependence store with SinkPC pointed Shadow Addr.
  __outputLSResultWithSinkPCAsKey();


#ifdef _OVERHEAD_PROF
  //printf("pcmin = %p, pcmax = %p \n",(void*) LDDLSResultMin, (void*)LDDLSResultMax);
  long long MemoryPeak = getVmPeak();
  long HWMPeak = getVmHWM();
  long Data = getVmData();
  long Exe = getVmExe();
  long Stk = getVmStk();
  long Lib = getVmLib();
  long Size = getVmSize();

#ifdef _DDA_PA
  MasterThrEnd = rdtsc();
  float MasterThrTot = (float) ( MasterThrEnd - MasterThrStart ) / (1870.0*1000.0);
  printf("MasterThreadTot = %f ms \n", MasterThrTot);
#endif

  printf("VmHWM = %ld KB Data = %ld KB Stack = %ld KB Text = %ld KB\n", HWMPeak, Data, Stk, Exe );
  printf("VmPeak = %ld KB, VmSize = %ld KB Lib = %ld KB \n", MemoryPeak, Size, Lib );
  printf("TraceByteNum = %d TraceRangeNum = %ld TracePCNum = %ld \n", TraceByteNum, TraceRangeNum, TracePCNum);
  printf("LCDepNum = %ld LIDepNum = %ld DepRangeNum = %ld \n", LCDepNum, LIDepNum, DepRangeNum);
#endif

  std::map<long , std::set<long> >::iterator mbeg, mend;
  std::set<long>::iterator sbeg, send;                                                                        
#if 0
  for(mbeg = AddrPC.begin(), mend = AddrPC.end(); mbeg != mend; mbeg++){
    std::cout<<"set.size " << mbeg->second.size() <<"\n"; 
  }  
#endif
  return;
}


void __checkLoadStackVarLShadow(int *Ptr, long AddrLen)
{
#ifdef _DDA_PP
  if( !LDDProfCurIter ) 
    return;
#else
  if( !LDDProfLoopID ) 
    return;
#endif

  void *pc;
  pc = __builtin_return_address(0);

#ifdef _DDA_PA
  GenerateAddrStream( Ptr, AddrLen , LDDProfLoopIter, 0, pc);
  return;
#endif

 int retval;
  long AddrInnerOffset = 0, Len1 = 0,  Len2 = 0;
  unsigned char ReadMask1 = 0, ReadMask2 = 0;
  void **ShadowAddr = 0;


 
  // Compute the shadow cell address. 
  // The accessed space crosses the bounder of 8bytes.
  ShadowAddr = (void**)AppAddrToShadow((long)Ptr);


  // RWTrace-1:
  //LDDRWTrace.insert((long)ShadowAddr);

  // RWTrace-2:
  //RBInsertVal(LDDRWTrace, (long) ShadowAddr);

  // RWTrace-3:
#if 1
  long Addr = (long) ShadowAddr;
  if ( Addr > StackBeg ){
    if( Addr < LDDStackMin )
      LDDStackMin = Addr;
    else if ( Addr > LDDStackMax )
      LDDStackMax = Addr;
  }
  else if( Addr > GlobalEnd ){
    if( Addr > LDDHeapMax )
      LDDHeapMax = Addr;
    else if( Addr < LDDHeapMin )
      LDDHeapMin = Addr;
  }
  else{
    if( Addr > LDDGlobalMax)
      LDDGlobalMax = Addr;
    else if( Addr < LDDGlobalMin )
      LDDGlobalMin = Addr;
  }
#endif

  retval = __determinInner8bytesInfo( Ptr, AddrLen,  
                                     &ReadMask1, &ReadMask2 );
  //retval = __determinInner8bytesInfo( Ptr, AddrLen, &AddrInnerOffset, 
   //                                  &Len1, &ReadMask1, &Len2, &ReadMask2 );
 
//  if( LDDProfLoopIter == 9 )
#ifdef _DEBUGINFO
  printf("__checkLoadLShadow: ReadAddr = %p, ShadowAddr = %p pc = %p iter = %d\n", Ptr, ShadowAddr, pc, LDDProfLoopIter);
#endif

  if( 1 == retval ){
   __checkLIDandUpdatePresentRTrace(pc, ShadowAddr,  
                                    &ReadMask1, LDDProfLoopIter); 
  }
  else{
   __checkLIDandUpdatePresentRTrace2(pc, ShadowAddr,  
                                     &ReadMask1, &ReadMask2, LDDProfLoopIter); 
  }

  return;
}


void __checkStoreStackVarLShadow(int *Ptr, long AddrLen)
{
  //isStackVar = 1;
  //__checkStoreLShadow(ptr, AddrLen);
  //isStackVar = 0;
#ifdef _DDA_PP
  if( !LDDProfCurIter ) 
    return;
#else
  if( !LDDProfLoopID ) 
    return;
#endif
  //
  //printf( "__checkStoreLShadow  addr = %p iter = %d\n", Ptr,  LDDProfLoopIter );
  long PC;
  PC  = (long)__builtin_return_address(0);  
#if 0
  if ( isStackVar )  // bug: not thread safe.
    PC  = (long)__builtin_return_address(1);  
  else
    PC  = (long)__builtin_return_address(0);  
#endif

#ifdef _DDA_PA
  GenerateAddrStream( Ptr, AddrLen ,  LDDProfLoopIter, 1, (void*)PC);
  return;
#endif

  void **ShadowAddr = 0;
  int retval;
  long AddrInnerOffset = 0, Len1 = 0,  Len2 = 0;
  unsigned char WriteMask1 = 0, WriteMask2 = 0;

  ShadowAddr = (void**)AppAddrToShadow( (long)Ptr );
  //printf("store = %p pc = %p\n", ShadowAddr, PC );
  
  // RWTrace-1:
  // LDDRWTrace.insert((long)ShadowAddr);
  
  // RWTrace-2:
 #if 0
  RBInsertVal(LDDRWTrace, (long) ShadowAddr);
#endif

  // RWTrace-3:
 #if 1
  long Addr = (long) ShadowAddr;
  // 0x7fff00000000
  if ( Addr > StackBeg ){
    if( Addr < LDDStackMin )
      LDDStackMin = Addr;
    else if ( Addr > LDDStackMax )
      LDDStackMax = Addr;
  }
  // 0x7cf000400000
  else if( Addr > GlobalEnd ){
    if( Addr > LDDHeapMax)
      LDDHeapMax = Addr;
    else if( Addr < LDDHeapMin )
      LDDHeapMin = Addr;
  }
  else{
    if( Addr > LDDGlobalMax)
      LDDGlobalMax = Addr;
    else if( Addr < LDDGlobalMin )
      LDDGlobalMin = Addr;
  }
#endif

  // if( LDDProfLoopIter == 9 )
  //
#ifdef _PRINTF
  printf("__checkStoreLShadow WriteAddr = %p ShadowAddr = %p PC = %p iter= %d \n",Ptr, ShadowAddr, PC, LDDProfLoopIter);
#endif
  retval = __determinInner8bytesInfo( Ptr, AddrLen,  
       &WriteMask1,  &WriteMask2 );
  //retval = __determinInner8bytesInfo( Ptr, AddrLen, &AddrInnerOffset, 
   //   &Len1, &WriteMask1, &Len2, &WriteMask2 );

  // Compute the shadow cell address. 
  // The accessed space crosses the bounder of 8bytes.
  if( 1 == retval ){
    __checkLIDandUpdatePresentWTrace((void*)PC, ShadowAddr, 
        &WriteMask1, LDDProfLoopIter); 
  }
  else{
    __checkLIDandUpdatePresentWTrace2((void*)PC, ShadowAddr,  
         &WriteMask1, &WriteMask2, LDDProfLoopIter); 
  }


  return ;

}

void __initshadowbuffer(char *FuncName, char* LoopPos)
{


  return ;
}
