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

//using namespace std;


//  The original version of collect all RW trace and full dependence analysis.
//  It is a serial process of collect trace and analysis dependence at the same time.
// 1) Collect  RWTraceInfo: addr-> position, varname, length-> iteration.
// 2) Analysis Dependence: Loop carried, loop independent. Loop Carried dependence information  
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





// Declare functions in LDDProfilingCommon.cpp
unsigned  addReadtoHashTraceInfo( void *Addr, long AddrLen, RWTraceInfo *CurRWAddrNode , std::string AddrPos, std::string VarName);
unsigned findRWAddrIDIndex(std::vector<RWAddrID> &RWTrace, std::string AddrPos, std::string VarName);


void __initprofiling();
void __outputdependence();
void __storecheck(int *ptr, long size, char* Pos, char* Name) ;
void __loadcheck(int *ptr, long size, char* Pos, char* Name);
void __addnewprofilingbuffer(char* FuncName, char *LoopPos);
void __storecheckstackvar( int *ptr, long AddrLen, char *Pos, char* Name, int TargetLoopFunc);               
void __loadcheckstackvar( int *ptr, long AddrLen, char *Pos, char* Name, int TargetLoopFunc);


#endif

