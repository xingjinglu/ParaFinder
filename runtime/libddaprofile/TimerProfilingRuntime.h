//===---- runtime/libddaprofile/TimerProfilingRuntime.h ------*- C++ -*-===//
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


#ifndef __TIMERPROFILINGCOMMON_H
#define __TIMERPROFILINGCOMMON_H

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

//inline void __outputProfilingResults();

//inline 
void __outputLoopID2Name();
//inline 
void __outputTimerResults();

//inline 
void __recordLoopID2Name( char *Name, long LoopID);
//inline 
void __recordTimerResults( long LoopID,unsigned long  long Time);

//inline 
unsigned  long long __rdtsc();

//inline 
void __initprofiling(); // Not used now.

#ifdef __cplusplus
}
#endif


#endif

