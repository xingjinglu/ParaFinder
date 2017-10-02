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

//#include"llvm/Support/raw_ostream.h"
//#include "llvm/Support/Debug.h"

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

#include<stdio.h>
#include<stdlib.h>




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

long long getVmHWM()                                                                                                     
{
  pid_t pid = getpid();
  int i = 13;
  char inbuf[500];
  char FileName[100];
  sprintf(FileName, "/proc/%ld/status", pid);
  std::ifstream ifs;
  ifs.open( FileName, std::ifstream::in );

  while ( i ){
    ifs.getline(inbuf, 499);
    i--;
  }
  ifs.getline(inbuf, 499);
  //printf("VmHWMStr = %s pid = %ld\n", inbuf, pid);
  std::string EventName = inbuf;
  EventName = EventName.substr(0, 5);
  if ( EventName.compare("VmHWM") != 0 ){
    printf("No VmHWM field \n");  
    return 0;
  }
  std::string lstr = inbuf;
  size_t pos = lstr.find(" ");
  if( pos != std::string::npos )
    lstr = lstr.substr( pos, lstr.length() );
  pos = lstr.rfind(" ");
  if( pos != std::string::npos )
    lstr = lstr.substr( 0, pos );

  pos = lstr.find(" ");
  while( pos != std::string::npos ){
    lstr.erase(pos, 1);
    pos = lstr.find(" ");
  }
  long val = atol(lstr.c_str());
  return val;
}


long long getVmSize()                                                                                                     
{
  pid_t pid = getpid();
  int i = 11;
  char inbuf[500];
  char FileName[100];
  sprintf(FileName, "/proc/%ld/status", pid);
  std::ifstream ifs;
  ifs.open( FileName, std::ifstream::in );
  // Not sound.
  while ( i ){
    ifs.getline(inbuf, 499);
    i--;
  }
  ifs.getline(inbuf, 499);
  //printf("VmSize = %s pid = %ld\n", inbuf, pid);
  std::string EventName = inbuf;
  EventName = EventName.substr(0, 6);
  if ( EventName.compare("VmSize") != 0 ){
    printf("No VmSize field \n");  
    return 0;
  }
  std::string lstr = inbuf;
  //printf("lstr_length = %ld  len = %ld \n", lstr.length(), sizeof(lstr.c_str()) );
  size_t pos = lstr.find(' ');
  if( pos != std::string::npos )
    lstr = lstr.substr( 8, pos );
  //printf("lstr =%s pos = %d\n", lstr.c_str(), pos );
  pos = lstr.rfind(" ");
  if( pos != std::string::npos )
    lstr = lstr.substr( 0, pos );

  pos = lstr.find(" ");
  while( pos != std::string::npos ){
    lstr.erase(pos, 1);
    pos = lstr.find(" ");
  }
  //printf("str = %s \n", lstr.c_str());
  long val = atol(lstr.c_str());
  return val;
}




long long getVmPeak()                                                                                                     
{
  pid_t pid = getpid();
  int i = 10;
  char inbuf[500];
  char FileName[100];
  sprintf(FileName, "/proc/%ld/status", pid);
  std::ifstream ifs;
  ifs.open( FileName, std::ifstream::in );

  while ( i ){
    ifs.getline(inbuf, 499);
    i--;
  }
  ifs.getline(inbuf, 499);
  //printf("VmPeak = %s \n", inbuf);
  std::string EventName = inbuf;
  EventName = EventName.substr(0, 6);
  if ( EventName.compare("VmPeak") != 0 ){
    printf("No VmPeak field \n");  
    return 0;
  }

  std::string lstr = inbuf;
  //printf("VmPeakStr = %s pid = %ld\n", lstr.c_str(), pid);
  // VmPeak: 3288461232 kB
  size_t pos = lstr.find(":");
  if( pos != std::string::npos ){
    //printf("pos = %d \n", pos);
    lstr = lstr.substr( pos+1, lstr.length()-pos );
  }

  //printf("VmPeak val_str 00 = %s \n", lstr.c_str());
  pos = lstr.rfind(" ");
  if( pos != std::string::npos )
    lstr = lstr.substr( 0, pos );
  //printf("VmPeak val_str 11 = %s \n", lstr.c_str());

  pos = lstr.find(" ");
  while( pos != std::string::npos ){
    lstr.erase(pos, 1);
    pos = lstr.find(" ");
  }
  long val = atol(lstr.c_str());
  return val;
}

long long getVmData()                                                                                                     
{
  pid_t pid = getpid();
  int i = 15;
  char inbuf[500];
  char FileName[100];
  sprintf(FileName, "/proc/%ld/status", pid);
  std::ifstream ifs;
  ifs.open( FileName, std::ifstream::in );

  while ( i ){
    ifs.getline(inbuf, 499);
    i--;
  }
  ifs.getline(inbuf, 499);
  //printf("VmData = %s \n", inbuf);
  std::string EventName = inbuf;
  EventName = EventName.substr(0, 6);
  if ( EventName.compare("VmData") != 0 ){
    printf("No VmData field \n");  
    return 0;
  }

  std::string lstr = inbuf;
  //printf("VmPeakStr = %s pid = %ld\n", lstr.c_str(), pid);
  // VmPeak: 3288461232 kB
  size_t pos = lstr.find(":");
  if( pos != std::string::npos ){
    //printf("pos = %d \n", pos);
    lstr = lstr.substr( pos+1, lstr.length()-pos );
  }

  //printf("VmPeak val_str 00 = %s \n", lstr.c_str());
  pos = lstr.rfind(" ");
  if( pos != std::string::npos )
    lstr = lstr.substr( 0, pos );
  //printf("VmPeak val_str 11 = %s \n", lstr.c_str());

  pos = lstr.find(" ");
  while( pos != std::string::npos ){
    lstr.erase(pos, 1);
    pos = lstr.find(" ");
  }
  long val = atol(lstr.c_str());
  return val;
}

long long getVmStk()                                                                                                     
{
  pid_t pid = getpid();
  int i = 16;
  char inbuf[500];
  char FileName[100];
  sprintf(FileName, "/proc/%ld/status", pid);
  std::ifstream ifs;
  ifs.open( FileName, std::ifstream::in );

  while ( i ){
    ifs.getline(inbuf, 499);
    i--;
  }
  ifs.getline(inbuf, 499);
 // printf("VmPeak = %s \n", inbuf);
  std::string EventName = inbuf;
  EventName = EventName.substr(0, 5);
  if ( EventName.compare("VmStk") != 0 ){
    printf("No VmStk field \n");  
    return 0;
  }

  std::string lstr = inbuf;
  //printf("VmPeakStr = %s pid = %ld\n", lstr.c_str(), pid);
  // VmPeak: 3288461232 kB
  size_t pos = lstr.find(":");
  if( pos != std::string::npos ){
    //printf("pos = %d \n", pos);
    lstr = lstr.substr( pos+1, lstr.length()-pos );
  }

  //printf("VmPeak val_str 00 = %s \n", lstr.c_str());
  pos = lstr.rfind(" ");
  if( pos != std::string::npos )
    lstr = lstr.substr( 0, pos );
  //printf("VmPeak val_str 11 = %s \n", lstr.c_str());

  pos = lstr.find(" ");
  while( pos != std::string::npos ){
    lstr.erase(pos, 1);
    pos = lstr.find(" ");
  }
  long val = atol(lstr.c_str());
  return val;
}

long long getVmExe()                                                                                                     
{
  pid_t pid = getpid();
  int i = 17;
  char inbuf[500];
  char FileName[100];
  sprintf(FileName, "/proc/%ld/status", pid);
  std::ifstream ifs;
  ifs.open( FileName, std::ifstream::in );

  while ( i ){
    ifs.getline(inbuf, 499);
    i--;
  }
  ifs.getline(inbuf, 499);
 // printf("VmPeak = %s \n", inbuf);
  std::string EventName = inbuf;
  EventName = EventName.substr(0, 5);
  if ( EventName.compare("VmExe") != 0 ){
    printf("No VmExe field \n");  
    return 0;
  }

  std::string lstr = inbuf;
  //printf("VmPeakStr = %s pid = %ld\n", lstr.c_str(), pid);
  // VmPeak: 3288461232 kB
  size_t pos = lstr.find(":");
  if( pos != std::string::npos ){
    //printf("pos = %d \n", pos);
    lstr = lstr.substr( pos+1, lstr.length()-pos );
  }

  //printf("VmPeak val_str 00 = %s \n", lstr.c_str());
  pos = lstr.rfind(" ");
  if( pos != std::string::npos )
    lstr = lstr.substr( 0, pos );
  //printf("VmPeak val_str 11 = %s \n", lstr.c_str());

  pos = lstr.find(" ");
  while( pos != std::string::npos ){
    lstr.erase(pos, 1);
    pos = lstr.find(" ");
  }
  long val = atol(lstr.c_str());
  return val;
}

long long getVmLib()                                                                                                     
{
  pid_t pid = getpid();
  int i = 18;
  char inbuf[500];
  char FileName[100];
  sprintf(FileName, "/proc/%ld/status", pid);
  std::ifstream ifs;
  ifs.open( FileName, std::ifstream::in );

  while ( i ){
    ifs.getline(inbuf, 499);
    i--;
  }
  ifs.getline(inbuf, 499);
 // printf("VmPeak = %s \n", inbuf);
  std::string EventName = inbuf;
  EventName = EventName.substr(0, 5);
  if ( EventName.compare("VmLib") != 0 ){
    printf("No VmLib field \n");  
    return 0;
  }

  std::string lstr = inbuf;
  //printf("VmPeakStr = %s pid = %ld\n", lstr.c_str(), pid);
  // VmPeak: 3288461232 kB
  size_t pos = lstr.find(":");
  if( pos != std::string::npos ){
    //printf("pos = %d \n", pos);
    lstr = lstr.substr( pos+1, lstr.length()-pos );
  }

  //printf("VmPeak val_str 00 = %s \n", lstr.c_str());
  pos = lstr.rfind(" ");
  if( pos != std::string::npos )
    lstr = lstr.substr( 0, pos );
  //printf("VmPeak val_str 11 = %s \n", lstr.c_str());

  pos = lstr.find(" ");
  while( pos != std::string::npos ){
    lstr.erase(pos, 1);
    pos = lstr.find(" ");
  }
  long val = atol(lstr.c_str());
  return val;
}

long long getVmPTE()                                                                                                     
{
  pid_t pid = getpid();
  int i = 19;
  char inbuf[500];
  char FileName[100];
  sprintf(FileName, "/proc/%ld/status", pid);
  std::ifstream ifs;
  ifs.open( FileName, std::ifstream::in );

  while ( i ){
    ifs.getline(inbuf, 499);
    i--;
  }
  ifs.getline(inbuf, 499);
 // printf("VmPeak = %s \n", inbuf);
  std::string EventName = inbuf;
  EventName = EventName.substr(0, 5);
  if ( EventName.compare("VmPTE") != 0 ){
    printf("No VmPTE field \n");  
    return 0;
  }

  std::string lstr = inbuf;
  //printf("VmPeakStr = %s pid = %ld\n", lstr.c_str(), pid);
  // VmPeak: 3288461232 kB
  size_t pos = lstr.find(":");
  if( pos != std::string::npos ){
    //printf("pos = %d \n", pos);
    lstr = lstr.substr( pos+1, lstr.length()-pos );
  }

  //printf("VmPeak val_str 00 = %s \n", lstr.c_str());
  pos = lstr.rfind(" ");
  if( pos != std::string::npos )
    lstr = lstr.substr( 0, pos );
  //printf("VmPeak val_str 11 = %s \n", lstr.c_str());

  pos = lstr.find(" ");
  while( pos != std::string::npos ){
    lstr.erase(pos, 1);
    pos = lstr.find(" ");
  }
  long val = atol(lstr.c_str());
  return val;
}




#ifdef _OVERHEAD_PROF

#define _RDTSC(high ,low) \
    __asm__ __volatile__("rdtsc" : "=a" (low), "=d" (high));

inline unsigned long long rdtsc(){
   unsigned long lo, hi;
   __asm__ __volatile__(
     "xorl %%eax, %%eax\n"
     "cpuid\n"
     "rdtsc\n"
     :"=a"(lo), "=d"(hi)                                                                                                                                                                     
     :
     :"%ebx", "%ecx");
   return ((unsigned long long)hi)<<32 | lo;
}

#define GET_DURATION_RDTSC(START, END)  (( END - START) / 3500.0 / 1000.0)

#endif

