//===---- LDDDAShadowRT.h - LDDDA based Shadow  -----*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// \file
// \brief This file defines data structures for DDDA based Shadow memory.
//
//
//===----------------------------------------------------------------------===//
//

#ifndef __LDDPROFILINGSHADOWRT_H
#define __LDDPROFILINGSHADOWRT_H

//#include"llvm/Support/raw_iostream.h"

#include<string.h>
//#include<stdbool.h>
#include<string>
#include<vector>
#include<map>
#include<set>
#include<sstream>
#include<iostream>
#include<iomanip>
#include<fstream>
#include<sstream>

#ifdef __cplusplus
extern "C" {
#endif


//  The original version of collect all RW trace and full dependence analysis.
//  It is a serial process of collect trace and analysis dependence at the 
//  same time.
// 1) Collect  RWTraceInfo: addr-> position, varname, length-> iteration.
// 2) Analysis Dependence: Loop carried, loop independent. Loop Carried 
//    dependence information  
//  include type->(src->position, name, sink->position, name)->distance. Loop 
//  independent dependence: type->(src->position->name; sink->position, name);

typedef unsigned long uptr;
typedef unsigned long u64;

static const uptr LinuxCShadowAppBeg = 0x7CF000000000ULL;
static const uptr LinuxCShadowAppEnd = 0x7FFFFFFFFFFFULL;  // 3TB
// Not fit debug.
static const uptr LinuxCShadowBeg = 0x6CF000000000ULL; // 3TB
static const uptr LinuxCShadowEnd = 0x6FFFFFFFFFFFULL; 

#define AppAddrToShadow( Addr ) \
  ( (Addr) & 0x6FFFFFFFFFF8 )





typedef struct __PresentRWAddrID{
  // Key: FileName_LineNo + VarName.
  std::string AddrPos; 
  std::string VarName;
  char RAddrLen;
  char WAddrLen;
}PresentRWAddrID;

typedef struct __PresentRWTrace{
 // PresentRWAddrID RTrace; // First read.
  std::map<int,PresentRWAddrID> RTrace;
  std::map<int,PresentRWAddrID> WTrace;
//  PresentRWAddrID WTrace; // Last write.
  int RWIterNo; // Used to label present trace ID and compute LC deps.
}PresentRWTrace;

typedef struct __PrecedingRWAddrID{
  // Key: FileName_LineNo + VarName.
  std::string AddrPos; 
  std::string VarName;
  char RAddrLen; // 8 bits
  char WAddrLen;
  // Dependence distance, loop independent.
  int RWIterNo; //(10);  // Only keep the latest iteration number.
}PrecedRWAddrID;

typedef struct __PrecedRWTrace{
  // Address is the key. 
  //  std::vector<PrecedRWAddrID> RTrace;
  std::map<int,PrecedRWAddrID> RTrace;  
  std::map<int,PrecedRWAddrID> WTrace; 
  //  std::vector<PrecedRWAddrID> WTrace;
  // Point to the position of DepInfo of current RWAddr.
}PrecedRWTrace;

typedef struct __RWTraceInfo{
   PrecedRWTrace Precede; 
   std::map<int, PresentRWTrace> Present;
   int AddrRflag;
   int AddrWflag;
}RWTraceInfo;

// How to link all RW address together?
//
typedef struct Dependence{
 std::string SrcAddrPos;   
 std::string SinkAddrPos;   
 std::string SrcVarName;   
 std::string SinkVarName;   
}Dep;

//std::set<unsigned> DepDist;  // Dependence distance.

// Not used now.
typedef struct __LIDepInfo{
  std::set<Dep> AntiDep;
  std::set<Dep> OutDep;
  std::set<Dep> TrueDep;
}LIDepInfo;
//LIDepInfo LIDepInfonow;  // 记录当前无关依赖 

typedef struct __LCDepInfo{
   // Deps, Deps distance.
  std::map<Dep, std::set<signed int> > AntiDep;
  std::map<Dep, std::set<signed int> > OutDep;
  std::map<Dep, std::set<signed int> > TrueDep;
}LCDepInfo;

typedef struct __DepInfo{
  std::string FuncName; // The Function that contains the loop.
  std::string LoopPos;  // FileName_LineNo

  //LIDepInfo LIDDep;
  LCDepInfo LDDep; // The LIDepInfo's distance = -1;
}DepInfo;

typedef std::map<int, DepInfo> LDDepInfo;

//void * __internal_mmap(void *addr, uptr length, int prot, int flags,
 //                       int fd, u64 offset);                  
//int __internal_munmap(void *addr, uptr length);               
//void * __mmapFixedReserveVMA(uptr fixedAddr, uptr size);


// Declare functions in LDDProfilingCommon.cpp
void __initCompleteShadowMemory();
void __finiCompleteShadowMemory();
void __initLDDCompleteShadow();
void __finiLDDCompleteShadow();



// Read. 
void __shadowR(long *ptr, long AddrLen, char *Pos, char* Name,int AddrInnerOffset,void **shadow_addr, int KeyWord);
void analyzeReadAddrDepSet(void** CurRWAddrNode,std::string AddrPos,std::string VarName, int Keyword);
void __checkLoadCShadow(long *ptr, long size, char* AddrPos, char* VarName) ;
void __checkLoadCShadowStackVar( long *ptr, long AddrLen, char *Pos, char* Name,  int TargetLoopFunc);


//  Write.
void __shadowW( long AddrLen, char *Pos, char* Name,void **shadow_addr, int KeyWord);
void analyzeWriteAddrDepSet(void** CurRWAddrNode,std::string AddrPos,std::string VarName, int KeyWord);
void __checkStoreCShadow(long *ptr, long size, char* AddrPos, char* VarName) ;
void __checkStoreCShadowStackVar( long *ptr, long AddrLen, char *Pos, char* Name, int TargetLoopFunc);

void addpendingtohistory();
void __addnewDepInfo(int *DDAProfilingFlagnum , char* FuncName, char* LoopPos);
void __outputLDDCSDependence(); 





#ifdef __cplusplus
}
#endif


#endif
