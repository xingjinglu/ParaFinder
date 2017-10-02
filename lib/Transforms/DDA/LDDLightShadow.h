//===---- lib/Transforms/DDA/LDDProfilingCommon.h ------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines the  class, which is used as a convenient way
// to create LLVM instructions with a consistent and simplified interface.
//
//===----------------------------------------------------------------------===//
//


#ifndef __LDDLIGHTSHADOW_H
#define __LDDLIGHTSHADOW_H

#include<string.h>
#include<stdbool.h>
#include<string>
#include<vector>
#include<map>

//using namespace std;


//  The original version of collect all RW trace and full dependence analysis.
//  It is a serial process of collect trace and analysis dependence at the 
//  same time.
// 1) Collect  RWTraceInfo: addr-> position, varname, length-> iteration.
// 2) Analysis Dependence: Loop carried, loop independent. Loop Carried 
// dependence information  
//  include type->(src->position, name, sink->position, name)->distance. Loop 
//  independent dependence: type->(src->position->name; sink->position, name);

// Not need now. 
typedef struct Position{
  std::string FileName;
  unsigned int LineNo;
}Pos;

//
typedef struct RWMemoryTrace{
  // long Addr; // Not need.
  unsigned AddrLen;
  std::string AddrPos;

  // Not need now.
  unsigned IterNo;

}RWRtrace;

typedef struct RWAddressID{
  // Key: FileName_LineNo + VarName.
  std::string AddrPos; 
  std::string VarName;
  unsigned AddrLen;
  // Dependence distance, loop independen.
  unsigned IterID; 
}RWAddrID;

typedef struct RWTraceInformation{
  // Not consider (Addr[1]+AddrLen[1]) ~ (Addr[2]+AddrLen[2]).
  void* Addr;
  // Address is the key. 
  std::vector<RWAddrID> RTrace;
  std::vector<RWAddrID> WTrace;
  // Point to the position of DepInfo of current RWAddr.
  unsigned IndexToDepInfo;
  struct RWTraceInformation *Next;  // Hash confiliction buckets.
}RWTraceInfo;

//
typedef struct LIDependence{
 std::string SrcAddrPos;   
 std::string SinkAddrPos;   
}LIDep;

typedef struct LCDependence{
 std::string SrcAddrPos;   
 std::string SinkAddrPos;   
 std::vector<unsigned> DepDist;  // Dependence distance.
}LCDep;

typedef struct LIDependenceInformation{
  std::vector<LIDep> AntiDep;
  std::vector<LIDep> OutDep;
  std::vector<LIDep> TrueDep;
}LIDepInfo;

typedef struct LCDependenceInformation{
  std::vector<LCDep> AntiDep;
  std::vector<LCDep> OutDep;
  std::vector<LCDep> TrueDep;
}LCDepInfo;

typedef struct DependenceInformation{
  LIDepInfo LIDDep;
  LCDepInfo LCDDep;
}DepInfo;

//std::map<int, DepInfo> FullDeps;


typedef struct DependenceType{
 std::vector<RWAddrID>::iterator OutDep;
 std::vector<RWAddrID>::iterator TrueDep;
 std::vector<RWAddrID>::iterator AntiDep;
}DepType;





// Defined functions in LDDDALightShadowRT.cpp
//
void __initPlatform();
void __die();
int __internal_close(fd_t fd); 

//void * __internal_mmap(void *addr, uptr length, int prot, int flags,
                        int fd, u64 offset);
//int __internal_munmap(void *addr, uptr length);
//void * __mmapFixedReserveVMA(uptr fixedAddr, uptr size);
//
#if 0
void __initLightShadowMemory();
void __initLDDLightShadow();
void __finiLightShadowMemory();
void __finiLDDLightShadow();

int __determinInner8bytesInfo(int *ptr, long AddrLen, 
                                     long *AddrInnerOffset,
                                     long *Len1, unsigned char* ReadMask1, 
                                     long *Len2, unsigned char * ReadMask2 );
void __updatePresentRTrace(void *PC, void **ShadowAddr, unsigned char *ReadMask,
                            PresentRWTrace &PRWTraceRef, int WTEmptyFlag );
void __determinReadLID(void * PC, unsigned char *ReadMask,
                        PresentRWTrace &PRWTraceRef, Trace &TraceRef);
void __checkLIDandUpdatePresentRTrace(void *PC, void **ShadowAddr, 
                      unsigned char * ReadMask, int LoopIter);
void __checkLIDandUpdatePresentRTrace2(void *pc, void** ShadowAddr,
                                       unsigned char * ReadMask1, 
                                       unsigned char *ReadMask2, int LoopIter);
void __checkLoadLShadow(int *ptr, long AddrLen);
void __checkLoadLShadow(int *ptr, long AddrLen, char *AddrPos, char *VarName);


void __updatePresentWTrace(void *PC, void **ShadowAddr, unsigned char WriteMask,
                            PresentRWTrace &PRWTraceRef, int WTEmptyFlag );
void __determinWriteLID(void * PC, unsigned char *WriteMask, 
                        PresentRWTrace &PRWTraceRef);
void __checkLIDandUpdatePresentWTrace(void *PC, void **ShadowAddr, 
                     unsigned char * WriteMask, int LoopIter);
void __checkLIDandUpdatePresentWTrace2(void *PC, void** ShadowAddr,
                                       unsigned char * WriteMask1, 
                                       unsigned char *WriteMask2, int LoopIter);
void __checkStoreLShadow(int *WriteAddr, long AddrLen);
void __checkStoreLShadowDebug(int *WriteAddr, long AddrLen, char *AddrPos, char *VarName);


void __checkLoadStackVarLShadow(int *ptr, long AddrLen);
void __checkStoreStackVarLShadow(int *ptr, long AddrLen);

void __outputLDDDependenceLShadow();

#endif

#endif

