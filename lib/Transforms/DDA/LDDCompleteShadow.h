//===- CommonMemorySafetyPasses.h - Declare common memory safety passes ---===//
// 
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// This file declares the interfaces required to use the LDD profiling 
// passes while they are not part of LLVM.
//
//===----------------------------------------------------------------------===//

#ifndef LDD_COMPLETESHADOW_H 
#define LDD_COMPLETESHADOW_H

#include<string.h>
#include<stdbool.h>

#include<string>
#include<map>
#include<vector>

typedef unsigned long uptr;
typedef unsigned long u64;

#if 0
void * __internal_mmap(void *addr, uptr length, int prot, int flags,
                        int fd, u64 offset);
int __internal_munmap(void *addr, uptr length);
void * __mmapFixedReserveVMA(uptr fixedAddr, uptr size);

void __initLightShadowMemory();
void __initLDDLightShadow();
void __finiLightShadowMemory();
void __finiLDDLightShadow();

void __shadowR(long *ptr, long AddrLen, char *Pos, char* Name,int AddrInnerOffset,void **shadow_addr, int KeyWord);
void analyzeReadAddrDepSet(void** CurRWAddrNode,std::string AddrPos,std::string VarName, int Keyword);
void __checkLoadCShadow(int *ptr, long size, char* AddrPos, char* VarName) ;
void __checkLoadCShadowStackVar( long *ptr, long AddrLen, char *Pos, char* Name,  int TargetLoopFunc);


void __shadowW(long *ptr, long AddrLen, char *Pos, char* Name,int AddrInnerOffset,void **shadow_addr, char KeyWord);
void analyzeWriteAddrDepSet(void** CurRWAddrNode,std::string AddrPos,std::string VarName, int KeyWord);
void __checkStoreCShadow(int *ptr, long size, char* AddrPos, char* VarName) ;
void __checkStoreCShadowStackVar( long *ptr, long AddrLen, char *Pos, char* Name, int TargetLoopFunc);

void addpendingtohistory(long *ptr, long AddrLen, int TargetLoopFunc,char* Pos, char* Name);
void __addnewDepInfo(int *DDAProfilingFlagnum , char* FuncName, char* LoopPos);
void __outputLDDCSDependence();
#endif

#endif
