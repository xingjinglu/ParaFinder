extern "C" {
#ifndef __LDDPROFILINGCOMMON_H
#define __LDDPROFILINGCOMMON_H

  #include<string.h>

  typedef struct dep{
    bool depType; // 0-Anti, 1-Output, 2-Flow 
    int loopID;
    size_t times;

    int srcLineNo;
    int sinkLineNo;
    char srcVarName[64];
    char sinkVarName[64];
    struct dep *next;

  }depNode;

  typedef struct dInfo{
    size_t prevTrueDep;
    size_t curTrueDep;
    size_t nextTrueDep;
    size_t outputDep;
    size_t antiDep;
  }depInfo;


#if 0
  typdef struct {
    depNode *prevTrueDep;
    depNode *curTrueDep;
    depNode *nextTrueDep;
    depNode *outputDep;
    depNode *antiDep;
  }depInfo;
#endif

  typedef struct memRW{
    long addr;
    int iterNo; // the current iter number    
    size_t size;
    // 1-write, 2-read, not used yet.
    short lastStat; //
    depInfo *depInfoPtr;
    struct memRW *next;

  }memRWInfo;



// Declare functions in LDDProfilingCommon.cpp
void __initprofiling();
void __storecheck(int *ptr, long size) ;
void __loadcheck(int *ptr, long size);


#endif

}
