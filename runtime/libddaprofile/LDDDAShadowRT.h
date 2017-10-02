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

#include"llvm/Support/raw_iostream.h"

#include<string.h>
#include<stdbool.h>
#include<string>
#include<vector>
#include<map>
#include<set>
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
  std::vector<PrecedRWAddrID> RTrace;
  std::vector<precedRWAddrID> WTrace;
  // Point to the position of DepInfo of current RWAddr.
}PrecedRWTrace;


typedef struct __PresentRWAddrID{
  // Key: FileName_LineNo + VarName.
  std::string AddrPos; 
  std::string VarName;
}PresentRWAddrID;


typedef struct __PresentRWTrace{
  PresentRWAddrID RTrace; // First read.
  presentRWAddrID WTrace; // Last write.

  // Need this  field?
  int RWIterNo; // Used to label present trace ID and compute LC deps.
}PresentRWTrace;

typedef struct __Trace{
 PrecedRWTrace Precede; 
 map<int, PresentRWTrace> Present;
}

// How to link all RW address together?


//
typedef struct Dependence{
 std::string SrcAddrPos;   
 std::string SinkAddrPos;   
 std::string SrcVarName;   
 std::string SinkVarName;   
}Dep;

std::set<unsigned> DepDist;  // Dependence distance.

// Not used now.
typedef struct __LIDepInfo{
  std::set<Dep> AntiDep;
  std::set<Dep> OutDep;
  std::set<Dep> TrueDep;
}LIDepInfo;

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

// Declare functions in LDDProfilingCommon.cpp
unsigned  addReadtoHashTraceInfo( void *Addr, long AddrLen, 
        RWTraceInfo *CurRWAddrNode , std::string AddrPos, std::string VarName);
unsigned findRWAddrIDIndex(std::vector<RWAddrID> &RWTrace, 
        std::string AddrPos, std::string VarName);

void __outputdependence();
void __initprofiling();
void __storecheckshadow(int *ptr, long size, char* AddrPos, char* VarName) ;
void __loadcheckshadow(int *ptr, long size, char* AddrPos, char* VarName);
void __initshadowbuffer(char *FuncName, char* LoopPos);

void __storecheckshadowstackvar( int *ptr, long AddrLen, char *Pos, char* Name, 
      int TargetLoopFunc);
void __loadcheckshadowstackvar( int *ptr, long AddrLen, char *Pos, char* Name, 
      int TargetLoopFunc);

#ifdef __cplusplus
}
#endif


#endif

