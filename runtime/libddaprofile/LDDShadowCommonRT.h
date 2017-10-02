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


#ifndef __LDDSHADOWCOMMONRT_H
#define __LDDSHADOWCOMMONRT_H


#include<stdbool.h>

#include<string>
#include<vector>
#include<map>
#include<set>
#include<sstream>
#include<iostream>

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned long  PC64;
typedef unsigned int  PC32;


// Plastform specific, now only support 64-bit linux system.
//
typedef unsigned long u64;
typedef unsigned long uptr;
typedef int fd_t; // Not used now.

// Max app size: 0x000000000000->0x7FFFFFFFFFFF; 
// Kernel size : 0x800000000000->0xFFFFFFFFFFFF;
//
#if 0
static const uptr LinuxAppBeg = 0x7CF000000000ULL;
static const uptr LinuxAppEnd = 0x7FFFFFFFFFFFULL;  // 3TB
// Not fit debug.
static const uptr LinuxLShadowBeg = 0x6CF000000000ULL; // 3TB
static const uptr LinuxLShadowEnd = 0x6FFFFFFFFFFFULL; 
#endif

//#define AppAddrToShadow( Addr ) \
  ( (Addr) & 0x6FFFFFFFFFF8 )





// Not used now.
void __initPlatform();
void __die();
int __internal_close(fd_t fd); 

void * __internal_mmap(void *addr, uptr length, int prot, int flags,
                        int fd, u64 offset);
int __internal_munmap(void *addr, uptr length);
void * __mmapFixedReserveVMA(uptr fixedAddr, uptr size);

long long getVmPeak();
long long getVmSize();
long long getVmHWM();
long long getVmData();
long long getVmStk();
long long getVmExe();
long long getVmLib();
long long getVmPTE();

#ifdef _OVERHEAD_PROF
inline unsigned long long rdtsc();
#endif


//#define STREAMNUM 1000
//#define STREAMITEMNUM 100000
//#define SHADOWSTRMNUM 4
//#define PRESBUFNUM 4


#ifdef __cplusplus
}
#endif


#endif

