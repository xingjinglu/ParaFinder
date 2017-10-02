//===---- LDDDAShadowRT.h - LDDDA based Light Shadow  -----*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
// LShadow: LightShadow, which recores less information. 
// \file  
// \brief This file defines data structures for DDDA based Shadow memory.
//
//
//===----------------------------------------------------------------------===//
//


#ifndef __LDDPROFILINGSHADOWRT_H
#define __LDDPROFILINGSHADOWRT_H

#include "LDDCommonConst.h"

#include<stdbool.h>

#include<string>
#include<vector>
#include<map>
#include<set>
#include<sstream>
#include<iostream>
 
extern int LDDProfLoopID, LDDProfLoopIter, LDDProfCurIter;

#ifdef __cplusplus
extern "C" {
#endif

#undef __ADDRKEY
  typedef unsigned long  PC64;
  typedef unsigned int  PC32;

  // Plastform specific, now only support 64-bit linux system.
  typedef unsigned long u64;
  typedef unsigned long uptr;
  typedef int fd_t; // Not used now.

  // Max app size: 0x000000000000->0x7FFFFFFFFFFF; 
  // Kernel size : 0x800000000000->0xFFFFFFFFFFFF;
  //
  static const uptr LinuxLShadowAppBeg = 0x7CF000000000ULL;
  static const uptr LinuxLShadowAppEnd = 0x7FFFFFFFFFFFULL;  // 3TB
  // Not fit debug.
  static const uptr LinuxLShadowBeg = 0x6CF000000000ULL; // 3TB
  static const uptr LinuxLShadowEnd = 0x6FFFFFFFFFFFULL; 


  static const uptr StackBeg = 0x6fff00000000;  // 4G stack segments. 
  static const uptr StackEnd = 0x6fffffffffff;

  static const uptr GlobalBeg = 0x6cf000000000; 
  static const uptr GlobalEnd = 0x6cf000400000; // 4M for .text and .data segments.

  static const uptr HeapBeg = 0x6cf000400000;  // 
  static const uptr HeapEnd = 0x6fff00000000;

  #define CBUFLEN 9
  static const long PageSize = 8192;

#define AppAddrToShadow( Addr ) \
  ( (Addr) & 0x6FFFFFFFFFF8 )

  // 24 TB
  //static const uptr LinuxLShadowBeg = 0x078000000000ULL; 
  //static const uptr LinuxLShadowEnd = 0x1FFFFFFFFFFFULL; 


  //  The original version of collect all RW trace and full dependence analysis.
  //  It is a serial process of collect trace and analysis dependence at the 
  //  same time.
  // 1) Collect  RWTraceInfo: addr, pc.
  // 2) Analysis Dependence: Loop carried, loop independent. Loop Carried 
  //    dependence information  
  //  include type->(src->pc, sink->pc)->distance. Loop 
  //  independent dependence: type->(True, Anti, Output);

  // Opt me?
  typedef struct __LSPrecedeRWAddrID{
    long PC; 
    struct __LSPrecedeRWAddrID *Next;
    int RWIter; // Iteration
    unsigned char RWMask; // 8 bits
  }LSPrecedeRWAddrID;

  // Memory trade off for performance.
  // 1) The same address accessed by different statments; (PC)
  // 2) The same statements access different bytes in different iterations. (
  //    R/W-AddrLenMask)
  // 3) First Read/ Last Write. 
  typedef struct __LSPrecedRWTrace{
    // Address is the key. 
    LSPrecedeRWAddrID *RTrace;  
    LSPrecedeRWAddrID  *WTrace;
    unsigned char GlobalReadMask;  // Opt me?
    unsigned char GlobalWriteMask;
  }LSPrecedeRWTrace;

  typedef struct __LSPresentRWAddrID{
    struct __LSPresentRWAddrID *Next;
    long PC;
    unsigned char RWMask; // read/write mask.
  }LSPresentRWAddrID;

  typedef struct __LSPresentRWTrace{
    // Opt me?
    // For every byte, there are at most one write/read pc.
    LSPresentRWAddrID *RTrace; // First read.
    LSPresentRWAddrID *WTrace; // Last write.
    int RWIter; 

    // Need this  field?
    unsigned char GlobalReadMask;  // Opt me?
    unsigned char GlobalWriteMask;
  }LSPresentRWTrace;

  // How to link all RW address together?
  typedef struct __LSDepDist{
    int DepDistVal;
    struct __LSDepDist *next;
  }LSDepDist;

  typedef struct __LSLCDep{
    long SrcPC;   
    long SinkPC;   
    struct __LSLCDep *next;
    LSDepDist *DepDist;
  }LSLCDep;

  typedef struct __LSLIDep{
    struct __LSLIDep *next;
    long SrcPC;   
    long SinkPC;   
  }LSLIDep;

  typedef struct __LSDep{
    struct __LSDep *next;
    long SrcPC;   
    long SinkPC;   
    int Dist[2]; // dist[0]: if no-LID 1, else 0, dist[1]:mini-LCD dist.
  }LSDep;

// LI/LC-Dep stored in the same struct. LS-4.
  typedef struct __LSDepInfo{
    LSDep *AntiDep;
    LSDep *OutDep;
    LSDep *TrueDep;
  }LSDepInfo;

  //std::set<unsigned> DepDist;  // Dependence distance.
  // Not used now.
  typedef struct __LSLIDepInfo{
    LSLIDep *AntiDep;
    LSLIDep *OutDep;
    LSLIDep *TrueDep;
  }LSLIDepInfo;


  // Not used now.
  typedef struct __LSLCDepInfo{
    // Deps, Deps distance.
    LSLCDep *AntiDep;
    LSLCDep *OutDep;
    LSLCDep *TrueDep;
  }LSLCDepInfo;

#ifdef _DDA_PA
  typedef struct __LSAddrShadowStrmItem{
    struct __LSAddrShadowStrmItem *Next;
    long PC;
    unsigned char RWMask; // read/write mask.
    // 0-read, 1-write. tag: 2-read-end, 3-write-end. 
    short int RWFlag; 
  }LSAddrShadowStrmItem;

  typedef struct __LSAddrShadowStream{
    //struct __LSAddrShadowStream *Next; // Every Iteration a stream.
    LSAddrShadowStrmItem *FirstItem;   // Read Position Number.
    // the last  ele, has different RWFalg,(2,3)->(read,write)
    LSAddrShadowStrmItem *LastItem;   // Write Position number.
    //LSAddrShadowStrmItem *HeadItem;  // Label the Head of LSAddrShadowStream.
    unsigned int LoopIter;
  }LSAddrShadowStream;
#endif

#ifdef _GAS_SHADOW
#ifdef _POOL_LIST
  typedef struct __RWTraceShdNode{
    long PC;
    struct __RWTraceShdNode * Next;
    struct __RWTraceShdNode * Prev;
    unsigned char RWMask[2];  // RWMask[0]: 1->read, 2->write; RWMask[1]: mask.
  }RWTraceShdNode;
#endif

  // Buffer for RWTraceShd (Array).
  typedef struct __RWTraceShdArray{
    long PC;
    unsigned char RWMask[2];
  }RWTraceShdArrayEle;

#endif

  typedef struct __Trace{
    LSPrecedeRWTrace *Precede; 
#ifdef _DDA_PA
    // Support Multiple PresentTable.
    LSPresentRWTrace *Present[PRESBUFNUM]; // <iteration, PresentTable>
#else
    LSPresentRWTrace *Present; // <iteration, PresentTable>
#endif

#ifdef _DDA_PA
    // Write: append to LastRWStream, Read from FirstRWStream. One Stream for each pipeline task.
    //LSAddrShadowStream *AddrShadowStream[SHADOWSTRMNUM]; // The same order as read/write.
#endif

#ifdef _GAS_SHADOW
#ifndef _DDA_PP
    // RWTraceShdNode *RWTraceShd[STREAMNUM]; // Init as NULL.
    RWTraceShdArrayEle *ShdArray[STREAMNUM];
    //int *ShdArraySequence[STREAMNUM];
    int MaxSize[STREAMNUM];
    int CurArrayNum[STREAMNUM]; // The current index of the array.
#endif

#ifdef _DDA_PP
  //RWTraceShdNode **RWTraceShd; // PROCID/STREAMNUM
  RWTraceShdArrayEle **ShdArray;
  int **CurArrayNum; // PROCID/STREAMNUM
  int *MaxSize; // PROCID
  // Local Pointer for each proc.
#endif
#endif // end _GAS_SHADOW

#ifdef _PARLIDA
    int *ShdArraySequence;
    int MaxSize;
    int CurArrayNum; // The current index of the array.
    int Flag; // access or not ?
#endif

#ifdef  __ADDRKEY  // Store deps result in the ShadowPtr(addr).
    LSLCDepInfo *LCDep;
    LSLIDepInfo *LIDep;
#endif
  }Trace;

  typedef struct __Deps{
    LSLIDepInfo *LIDep;
    LSLCDepInfo *LCDep;
  }Deps;

  //typedef std::map<int, DepInfo> LDDep; // not used now.

#ifdef _OVERHEAD_PROF
  extern long LSPresentTraceMallocTime, LSPrecedeTraceMallocTime,  LSPresentBufFreeTime,LSBufReuseTime; 
  extern long LSAddRWToPrecedeTraceTime;
  extern long LCDepInfoMallocTime, LCDepMallocTime, LCDepDistMallocTime,  LIDepMallocTime, LIDepInfoMallocTime;
  extern long LSAddrShadowTime, LSPCShadowTime;

  extern unsigned long OFSCheckLoad, OFECheckLoad, OFSCheckStore, OFECheckStore;
  extern unsigned long TotalUpWrite, NWTotalUpWrite ;
#endif
extern  long LDDStackMax, LDDStackMin, LDDHeapMax, LDDHeapMin, LDDGlobalMax, LDDGlobalMin;
extern long LDDLSResultMax, LDDLSResultMin;
extern LSPresentRWAddrID *LSPresentRWBuf; 
#ifdef _DDA_PA
extern long LSLIDepSinkPCMax, LSLIDepSinkPCMin, LSLCDepSinkPCMax, LSLCDepSinkPCMin;
extern long TraceByteNum, TraceRangeNum, TracePCNum, LCDepNum, LIDepNum, DepRangeNum;
#endif

  void __initLightShadowMemory();
  void __initLDDLightShadow();
  void __finiLightShadowMemory();
  void __finiLDDLightShadow();

  // Read related. 
  int __determinInner8bytesInfo(void *ptr, long AddrLen, 
      unsigned char* ReadMask1, 
      unsigned char * ReadMask2 );
  //int __determinInner8bytesInfo(void *ptr, long AddrLen, 
   //   long *AddrInnerOffset, long *Len1, unsigned char* ReadMask1, 
    //  long *Len2, unsigned char * ReadMask2 );
  void __updatePresentRTrace(void *PC, void **ShadowAddr, 
      unsigned char *ReadMask, LSPresentRWTrace &PRWTraceRef, 
      int WTEmptyFlag );
  void __determinReadLID(void * PC, unsigned char *ReadMask,
      LSPresentRWTrace &PRWTraceRef, Trace &TraceRef);
  void __checkLIDandUpdatePresentRTrace(void *PC, void **ShadowAddr, 
      unsigned char * ReadMask, unsigned int LoopIter);
  void __checkLIDandUpdatePresentRTrace2(void *pc, void** ShadowAddr,
      unsigned char * ReadMask1, 
      unsigned char *ReadMask2, unsigned int LoopIter);
  void __checkLoadLShadow(int *ptr, long AddrLen);
  void __checkLoadLShadowDebug(int *ptr, long AddrLen, char *AddrPos, char *VarName);
  void __init_proc();
  void ReInitPipelineState();



  // Write related.
    void __updatePresentWTrace(void *PC, void **ShadowAddr, 
      unsigned char WriteMask, LSPresentRWTrace &PRWTraceRef, 
      int WTEmptyFlag );
    void __determinWriteLID(void * PC, unsigned char *WriteMask, 
      LSPresentRWTrace &PRWTraceRef, Trace &TraceRef);
  void __checkLIDandUpdatePresentWTrace(void *PC, void **ShadowAddr, 
       unsigned char * WriteMask, unsigned int LoopIter);
   void __checkLIDandUpdatePresentWTrace2(void *PC, void** ShadowAddr,
       unsigned char * WriteMask1,unsigned char *WriteMask2, unsigned int LoopIter);
  void __checkStoreLShadow(int *WriteAddr, long AddrLen);
  void __checkStoreLShadowDebug(int *WriteAddr, long AddrLen, char *AddrPos, char *VarName);

  //
  void __checkLoadStackVarLShadow(int *ptr, long AddrLen);
  void __checkStoreStackVarLShadow(int *ptr, long AddrLen);

  //
  void __traverseRBTreeRWTrace(); 
  void __traverseTriSectionRWTrace();
  void __outputLDDDependenceLShadow();
  void __addPresentToPrecedeTrace();
  void __EndAddPresentToPrecedeTrace();

  //inline bool operator< (const LSDep & Lhs, const LSDep & Rhs);
  //inline bool operator< (const PrecedeRWAddrID & Lhs, const PrecedeRWAddrID & Rhs);


#ifdef __cplusplus
}
#endif


#endif

