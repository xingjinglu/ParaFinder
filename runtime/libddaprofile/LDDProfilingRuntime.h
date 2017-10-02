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


#ifndef __LDDPROFILINGCOMMON_H
#define __LDDPROFILINGCOMMON_H

#include<string.h>
#include<stdbool.h>
#include<string>
#include<vector>
#include<map>
#include<set>
#include<iostream>
#include<sstream>

//using namespace std;
#ifdef __cplusplus
extern "C" {
#endif


//  The original version of collect all RW trace and full dependence analysis.
//  It is a serial process of collect trace and analysis dependence at the same time.
// 1) Collect  RWTraceInfo: addr-> position, varname, length-> iteration.
// 2) Analysis Dependence: Loop carried, loop independent. Loop Carried dependence information  
//  include type->(src->position, name, sink->position, name)->distance. Loop 
//  independent dependence: type->(src->position->name; sink->position, name);

typedef struct __RWAddrID{
  // Key: FileName_LineNo + VarName.
  std::string AddrPos; 
  std::string VarName;
  unsigned AddrLen;
  // Dependence distance, loop independent.
  std::vector<int> RWIterNo; //(10);  // RWIterNo[0]: current, [1]: history
}RWAddrID;

typedef struct __RWTraceInfo{
  // Not consider (Addr[1]+AddrLen[1]) ~ (Addr[2]+AddrLen[2]).
  void* Addr;
  // Address is the key. 
  std::vector<RWAddrID> RTrace;
  std::vector<RWAddrID> WTrace;
  // Point to the position of DepInfo of current RWAddr.
  unsigned IndexToDepInfo;
  struct __RWTraceInfo *Next;  // Hash confiliction buckets.
}RWTraceInfo;
//
typedef struct Dependence{
 std::string SrcAddrPos;   
 std::string SinkAddrPos;   
 std::string SrcVarName;   
 std::string SinkVarName;   
}Dep;

std::set<unsigned> DepDist;  // Dependence distance.
typedef struct __LIDepInfo{
  std::set<Dep> AntiDep;
  std::set<Dep> OutDep;
  std::set<Dep> TrueDep;
}LIDepInfo;

typedef struct __LCDepInfo{
   // Deps, Deps distance.
  std::map<Dep, std::set<int> > AntiDep;
  std::map<Dep, std::set<int> > OutDep;
  std::map<Dep, std::set<int> > TrueDep;
}LCDepInfo;

typedef struct __DepInfo{
  std::string FuncName; // The Function that contains the loop.
  std::string LoopPos;  // FileName_LineNo

  LIDepInfo LIDDep;
  LCDepInfo LCDDep;
}DepInfo;

typedef std::map<int, DepInfo> LDDepInfo;

// Declare functions in LDDProfilingCommon.cpp
unsigned  addReadtoHashTraceInfo( void *Addr, long AddrLen, RWTraceInfo *CurRWAddrNode , std::string AddrPos, std::string VarName);
unsigned findRWAddrIDIndex(std::vector<RWAddrID> &RWTrace, std::string AddrPos, std::string VarName);

void __outputdependence();
void __initprofiling();
//void __storecheck(int *ptr, long size, std::string AddrPos, std::string VarName) ;
//void __loadcheck(int *ptr, long size, std::string AddrPos, std::string VarName);
void __storecheck(int *ptr, long size, char* AddrPos, char* VarName) ;
void __loadcheck(int *ptr, long size, char* AddrPos, char* VarName);
void __addnewprofilingbuffer(char *FuncName, char* LoopPos);

void __storecheckstackvar( int *ptr, long AddrLen, char *Pos, char* Name, int TargetLoopFunc);
void __loadcheckstackvar( int *ptr, long AddrLen, char *Pos, char* Name, int TargetLoopFunc);

#ifdef __cplusplus
}
#endif


#endif

