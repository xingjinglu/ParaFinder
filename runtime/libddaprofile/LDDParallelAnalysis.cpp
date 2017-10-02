//===- LDDParallelAnalysis.cpp - LDDPA Parallel Analysis  --*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// \file  
// \brief This file defines parallel analysis funnctions called by DA in runtime, which is 
// based TBB pipeline parallel.
//
//
//===----------------------------------------------------------------------===//
//

#ifdef _DDA_PA
 
#include "LDDParallelAnalysis.h"
#include "LDDLightShadowRT.h"
#include "LDDCommonConst.h"
//#include  "interval_custom.hpp"
#include  "stl_interval_set.h"
//#include <google/profiler.h>

#include<map>

#include "tbb/pipeline.h"
#include "tbb/tick_count.h"
#include "tbb/task_scheduler_init.h"
#include "tbb/tbb_allocator.h"
#include "tbb/compat/thread"
#include "tbb/critical_section.h"
#include "tbb/blocked_range.h"
#include "tbb/parallel_for.h"
#include "tbb/concurrent_unordered_set.h"
#include "tbb/concurrent_unordered_map.h"

#include<omp.h>

#include <cstdlib>
#include <cstdio>
#include <cctype>
#include <cstddef>

#include<string>
#include<iostream>
#include<stdexcept>
#include<iomanip>

#include "atomic.h"
#include "rt_header.h"
#include "rt_shm_malloc.h"

#define ReadThreadNum 1
#define ReadUpdateThreadNum 1
#define ParUpdateTaskNum 1

#define READBUFNUM  20

#define RangeCacheLen 10
#define PipeThreadNum 3
#define PARLIDAMASK 0x7
#define RLIDATASKNUM 8
#define LIDAThreadNum 8


/* 1) _DDA_DEFAULT  GenerateAddrStream ReadLIDAFilter, LCDAFilter, OutputFilter
 ** 1.1) Serial in every stage
 ** 1.2) _DDA_DEFAULT_PARLIDA  Do LIDA in parallel 
 ** 1.3) _DDA_DEFAULT_PP_PP_PARLIDA  Parallel Profiling + Parallel ReadLIDAFilter(each thread for each Proc)
 **
 ** 2) _DDA_GAS_SHADOW GenerateAddrStream(RW in shadow), ReadLIDAFilter, LCDAFilter, OutputFilter
 ** 2.1) _DDA_GAS_SHADOW_PARLIDA: Same to _DDA_FAULT_PP
 ** 2.2) _DDA_GAS_SHADOW_PP_PARLIDA
 **
 ** 3) _DDA_FOUR_STAGES GenerateAddrStream ReadLIDAFilter, LCDAFilter, OutputFilter
 ** 3.1) _DDA_FOUR_STAGES_PARLIDA
 ** 3.2) _DDA_FOUR_STAGES_PP
 ** 5) 
 */

#ifdef _GAS_SHADOW
// Keep ShdAddr accessed.
// [x][0] == 1, new iteration, start LCDA.
#ifdef _DDA_GAS_SHADOW_PP_PARLIDA
// Producer Side Write by Procs.
long ***AccShdAddrStream; // PROCID/STREAMNUM/STREAMITEMNUM
int **AccShdNum; // PROCID/STREAMNUM
int **AccShdLoopIter; // PROCID/STREAMNUM
int AccNum; // Private for each PROC.
atomic_t **PPStreamIsFull; // PROCID/STREAMNUM
// local opt.
long *GLAccShdAddrStream;
int *GLAccShdNum;
int *GLAccShdLoopIter;
atomic_t *GLPPStreamIsFull;

int PROCID;
int CurProc;
#else
long AccShdAddrStream[STREAMNUM][STREAMITEMNUM]; //[][1,2,..]
int AccShdNum[STREAMNUM]; // Length. 0, 1,2, ...
int AccShdLoopIter[STREAMNUM];// Keep LoopIter number.  
int AccNum = 1; // ith of ShdAddr;
#endif


#ifdef _MUTEX
pthread_mutex_t GRMutex[STREAMNUM];
pthread_cond_t GRCond[STREAMNUM];
#endif

#ifdef _GAS_SHADOW_PARLIDA
__thread int LCurNum;
__thread long *LAccShdAddr;
#endif

// Not used now.
//RWTraceShdArrayEle *ShdArrayPool[STREAMNUM];
//RWTraceShdArrayEle *PtrShdArrayPool;
//int UseArrayNum = 0; // GenerateAddrStream 
#endif // #end ifdef _GAS_SHADOW

#ifdef _DDA_DEFAULT

#if defined(_DDA_DEFAULT_PARLIDA) 
long AccShdAddrStream[STREAMITEMNUM]; //[][1,2,..]
long AccShdAddrSet[STREAMITEMNUM];
int AccShdNum[STREAMNUM]; // Length. 0, 1,2, ...
int AccNum = 1; // ith of ShdAddr;
AddrStreamItem *PLIDAPtrItems;
tbb::atomic<long> RStreamNum;
bool AccAddrOfIter[STREAMITEMNUM]; // label New accessed ShdAddr of cur iter.
std::map<long,std::set<long> > *ThrLITDep;
std::map<long,std::set<long> > *ThrLIADep;
std::map<long,std::set<long> > *ThrLIODep;
#endif

#ifdef _DDA_DEFAULT_PP_PARLIDA
AddrStreamItem *ProcPLIDAPtrItems[PROCNUM];
int ProcPLIDALength[PROCNUM];
bool ProcAccAddrOfIter[PROCNUM][STREAMITEMNUM]; // label New accessed ShdAddr of cur iter.
std::map<long,std::set<long> > *ProcThrLITDep[PROCNUM];
std::map<long,std::set<long> > *ProcThrLIADep[PROCNUM];
std::map<long,std::set<long> > *ProcThrLIODep[PROCNUM];

// ReadRWTraceFilter <---Commnuniction---> LIDAFitler. 
tbb::atomic<long> ProcRStreamNum[PROCNUM];
// the first is 0, PROCNUM, 2*PROCNUM; later mask 0, 1, 2, 3
tbb::atomic<long> ProcWPresNum[PROCNUM], ProcWPresNumTh[PROCNUM];
AddrStream **Streams; // PROCID/STREAMITEMNUM
atomic_t **PPStreamIsFull; // PROCID/ STREAMNUM
int PROCID;
int CurProc;
// Local opts.
AddrStream *GLStreams; // Local var for each proc.
atomic_t *GLPPStreamIsFull;

__thread long ThrLRStreamNum; // Local OPT in parreadlida().
#else
AddrStream *Streams;
tbb::atomic<long> StreamIsFull[STREAMNUM];
LSAddrShadowStrmItem *LSStreamItemBuf; // AddrShadowStreamItem's Reuse buffer.
#endif

#endif  // end _DDA_DEFAULT

// Common variables.
tbb::atomic<long>  PresIsFull[PRESBUFNUM];
tbb::atomic<long> WShadowStrmNum,  RShadowStrmNum, WPresNum, RPresNum, RItemNum;
long WItemNum, WStreamNum;

// LIDAFilter <---Commnuniction---> LCDAFitler. 
// RPresNum/WPresNum is member of LIDAFilter, LCDAFilter.
//volatile int WPresNum, RPresNum;

LSPresentRWAddrID *LSPresentBuf; // Present[]'s Reuse buffer. 

unsigned int CurLoopIter; //Not thread safe.
int LCDAReady;

std::thread *SpawnThread;
volatile bool LSOutputLCDepFinish  = 0;
AddrStreamItem *PtrItems; // Labels the current AddrStreamItem[cur];

// critical_section
tbb::critical_section CSLSStreamItemBuf; 
tbb::critical_section CSLSPresentBuf; 

#ifdef _OVERHEAD_PROF
long LSStreamItemBufReuseTime, LSRWStreamItemMallocTime;
long MasterThrStart, MasterThrEnd;
#endif

#ifdef _TIME_PROF
volatile float GPTime = 0.0;
volatile float RCTime = 0.0, RPTime = 0.0;
volatile float LIDAPTime = 0.0, LIDACTime = 0.0,  LCDAPTime = 0.0, LCDACTime = 0.0;
volatile float CancelWaitTime = 0.0, JoinWaitTime = 0.0;
// pipeline stage execution time.
volatile float GTime = 0.0,  ReadTime = 0.0, LIDATime = 0.0,  LCDATime = 0.0,  OutputTime = 0.0;
volatile float PUASSTime = 0.0, ReadPreTime = 0.0, ReadUpdateTime = 0.0;
volatile float KERNELTime = 0.0;
volatile float  KCacheTime = 0.0, KRBTime = 0.0, KAddrTime = 0.0;
volatile float PRETIME = 0.0, PROTIME = 0.0;
long AddrNum = 0;
long ReadAddrNum = 0;
#endif

#ifdef _DDA_PA
// BOOST.ICL Stride interval.
//typedef interval_set<long, std::less, MyInterval> MyIntervalSet;
//MyIntervalSet AddrInterval;
std::interval_set<long>  AddrIntervalSet(8);
std::set<long> AddrSet;
#endif

int MaxPCNum;
std::map< long, std::set<long> > AddrPC;

using namespace std;
using namespace tbb;


int tasknum = 0;

#ifdef _TIME_PROF
tbb::tick_count partstart;
#endif

// 
bool operator<(SrcPCDist Src1, SrcPCDist Src2){

  return( Src1.SrcPC < Src2.SrcPC);
}

bool operator<(DepPair &Src1, DepPair &Src2){
  return( Src1.SinkPC < Src2.SinkPC || Src1.SrcPC < Src2.SrcPC);
}





/* Stored RWTrace info.
 *
 */
class RWTrace{

  long IterStackMin, IterStackMax, IterHeapMax, IterHeapMin, IterGlobalMin, IterGlobalMax;

  public:
  bool StartLCDA;
  bool CancelPipeline;
  // Input for the LIDAFilter.
  unsigned int LoopIter; // Input of LIDA.
  long StackMin, StackMax, HeapMax, HeapMin, GlobalMin, GlobalMax;
  interval_set<long> RWTraceInterval;
  //interval_set<long> *RWTraceInterval;
  //
  AddrStream *Streams; // Used by ReadFilter.



};

/* Stored LIDep/LCDep info.
*/
class LICDep {
  public:

    // Addrs-scope to do LCDA.
    long LCDAStackMin, LCDAStackMax, LCDAHeapMax, LCDAHeapMin, LCDAGlobalMin, LCDAGlobalMax;

    bool StartLCDA;
    bool CancelPipeline;

    interval_set<long> LCDAAddrInterval;

    // Input for the LIDAFilter.
    unsigned int LoopIter; // Input of LIDep.
    unsigned int CurPresNum;
    int EndLoopIter;
    //long SinkPC, SrcPC
    std::map<long,std::set<long> > LITDep;
    std::map<long,std::set<long> > LIADep;
    std::map<long,std::set<long> > LIODep;

    std::map<long,std::set<SrcPCDist> > LCTDep;
    std::map<long,std::set<SrcPCDist> > LCADep;
    std::map<long,std::set<SrcPCDist> > LCODep;

    //! Allocate a RWTraceDep object that can hold up to max_size characters.
    #if 0
    static LICDep* allocate( size_t max_size ) {
      // +1 leaves room for a terminating null character.
      LICDep* t = (LICDep*)tbb::tbb_allocator<char>().allocate( sizeof(LICDep)+1 );
      return t;
    }
    //! Free a RWTraceDep object 
    void free() {
      tbb::tbb_allocator<char>().deallocate((char*)this,sizeof(LICDep)+1); // bug?
    } 
  #endif

};

#if defined(_FOUR_STAGES)

class ReadFilter: public tbb::filter{

  public:
    ReadFilter();
    ~ReadFilter();

  private:
    //std::interval_set<long> ReadAddrInterval;
    // Output;
    class RWTrace* next_rwtrace;
    int copysize;
    long LWPresNum, LRStreamNum;
    /*override*/ 
    void* operator()(void*); // why private?

  private:
    class LICDep *next_lidep;

};

ReadFilter::ReadFilter( ):
  filter(serial_in_order),
  LRStreamNum(RStreamNum)
  //next_rwtrace( RWTraceDep::allocate(0) )
{ 
  copysize = sizeof(AddrStreamItem) * STREAMITEMNUM;
}

ReadFilter::~ReadFilter() 
{
  //next_rwtrace->free(); // not need, can be done by the follow filter.

}


void* ReadFilter::operator()(void*) 
{

#ifdef _TIME_PROF
  tbb::tick_count tstart, pstart;
  pstart = tbb::tick_count::now();
#endif

  // 1) Read RWTrace from Streams[].
  // 1.1) Read from the Streams[] one by one.
  LRStreamNum = RStreamNum;
#ifdef _TIME_PROF
  tstart = tbb::tick_count::now();
#endif 

  while( !StreamIsFull[LRStreamNum] ); // printf("RCTime RStreamNum = %d \n", RStreamNum); // OH: worst.

#ifdef _TIME_PROF
  RCTime += (tbb::tick_count::now() - tstart).seconds();
#endif 

  next_rwtrace = new RWTrace;
  next_rwtrace->Streams = (AddrStream*) malloc ( sizeof(AddrStream) );
  next_rwtrace->Streams->LoopIter = Streams[LRStreamNum].LoopIter;
  next_rwtrace->Streams->End = Streams[LRStreamNum].End;
  memcpy(&next_rwtrace->Streams->Items[0], &Streams[LRStreamNum].Items[0], copysize);


  if( Streams[LRStreamNum].Items[0].RWFlag < 2  ){
    StreamIsFull[LRStreamNum] = 0;
    RStreamNum = (RStreamNum+1) % STREAMNUM;
#ifdef _TIME_PROF
    ReadTime += (tbb::tick_count::now() - pstart).seconds();
#endif 
    return next_rwtrace;
  }
  else if( Streams[LRStreamNum].Items[0].RWFlag == 2 ){
    Streams[LRStreamNum].Items[0].RWFlag = 3; 
#ifdef _TIME_PROF
    ReadTime += (tbb::tick_count::now() - pstart).seconds();
#endif 
    return next_rwtrace;
  }
  else if( Streams[LRStreamNum].Items[0].RWFlag == 3 ){
    return NULL;
  }

}

class LIDAFilter: public tbb::filter {

#ifdef _DDA_FOUR_STAGES_PARLIDA
  struct ParLIDA{
    unsigned int LoopIter;
    int CurProc;
    int LRStreamNum, LWPresNum;
    class LIDAFilter *LIDAPtr;

    void operator()(const blocked_range<int>& range) const{
      for(int i = range.begin(); i != range.end(); ++i){
        LIDAPtr->parcheck(LoopIter, i);
      }
    }

  };
#endif

#ifdef _GAS_SHADOW
#ifdef _GAS_SHADOW_PARLIDA
  struct ParLIDACheck{
    unsigned int LoopIter;
    class LIDAFilter *LIDAPtr;
    void operator()(const blocked_range<int>& range) const{
      for( int i = range.begin() ; i != range.end(); ++i){
        LIDAPtr->check(LoopIter, i );
      }
    }
  };
#endif
#endif

  public:
  LIDAFilter();
  ~LIDAFilter();


  // 20140127.
  // 1) LIDep/LCDep store in the same struct with different Dist;
  // Dist[0] = 0, keep the LID Dist whose value is 0.
  // 2) LIDep: take SinkPC as key.
  // 3) Pipeline Parallel analysis.
  // Anti-LIDep info.
#if defined(_DDA_FOUR_STAGES_PP_PARLIDA) 
  void doReadLIDAWithSinkPCAsKey(void * PC, unsigned char ReadMask, LSPresentRWTrace &PRWTraceRef, Trace &TraceRef, int ThreadID, int CurProc)
  {
    unsigned char RWMask;

    // 1) check for LIDep.
    if( (RWMask = (ReadMask & PRWTraceRef.GlobalWriteMask)) ) {
      LSPresentRWAddrID * WIDPtr = PRWTraceRef.WTrace;
      unsigned char NewRWMask;
      std::map<long, std::set<long> > &LITDep = ProcThrLITDep[CurProc][ThreadID];
      //std::map<long, std::set<long> > &LITDep = next_lidep->LITDep;

      //for( ; RWMask && (WIDPtr != 0) ; WIDPtr = WIDPtr->Next )
      for( ; WIDPtr != 0 ; WIDPtr = WIDPtr->Next ){
        NewRWMask = RWMask & WIDPtr->RWMask;
        //NewRWMask = RWMask;
        //RWMask &= RWMask ^ WIDPtr->RWMask;

        // LI True Dep?
        // WIDPtr->PC ==> (long)PC
        if( NewRWMask ){
          bool inH = 0;
#ifdef _OPT2
          LSDepInfo **DepShadowPtr;
          //LSDepInfo *LIDepPtr;
          DepShadowPtr = (LSDepInfo**) AppAddrToShadow((long)PC);
          if( *DepShadowPtr != 0 ){
            //LIDepPtr = *DepShadowPtr;
            LSDep *LIDepTrue = (*DepShadowPtr)->TrueDep; //TraceRef.LIDep->TrueDep; 
            while( LIDepTrue != 0 ){
              if(LIDepTrue->SrcPC == WIDPtr->PC){
                inH = 1;
                break;
              }
              LIDepTrue = LIDepTrue->next;
            }
          }
#endif
          if(!inH){
            LITDep[(long)PC].insert(WIDPtr->PC);
          }
#ifdef _DEBUGINFO
          std::cout<<"doReadLIDWithSinkPCAsKey: LITDep SinkPC = " << PC << ", SrcPC = " << WIDPtr->PC << "\n";
#endif
        }
      }
    }

    return ;
  }



  // 20140127.
  // 1) LIDep/LCDep store in the same struct with different Dist;
  // Dist[0] = 0, keep the LID Dist whose value is 0.
  // 2) LIDep: take SinkPC as key.
  // 3) Pipeline Parallel analysis.
  // Anti-LIDep info.
#elif defined(_DDA_FOUR_STAGES_PP_PARLIDA)
  void doReadLIDAWithSinkPCAsKey(void * PC, unsigned char ReadMask, LSPresentRWTrace &PRWTraceRef, Trace &TraceRef, int ThreadID)
  {
    unsigned char RWMask;

    // 1) check for LIDep.
    if( (RWMask = (ReadMask & PRWTraceRef.GlobalWriteMask)) ) {
      LSPresentRWAddrID * WIDPtr = PRWTraceRef.WTrace;
      unsigned char NewRWMask;
      std::map<long, std::set<long> > &LITDep = ThrLITDep[ThreadID];
      //std::map<long, std::set<long> > &LITDep = next_lidep->LITDep;

      //for( ; RWMask && (WIDPtr != 0) ; WIDPtr = WIDPtr->Next )
      for( ; WIDPtr != 0 ; WIDPtr = WIDPtr->Next ){
        NewRWMask = RWMask & WIDPtr->RWMask;
        //NewRWMask = RWMask;
        //RWMask &= RWMask ^ WIDPtr->RWMask;

        // LI True Dep?
        // WIDPtr->PC ==> (long)PC
        if( NewRWMask ){
          bool inH = 0;
#ifdef _OPT2
          LSDepInfo **DepShadowPtr;
          //LSDepInfo *LIDepPtr;
          DepShadowPtr = (LSDepInfo**) AppAddrToShadow((long)PC);
          if( *DepShadowPtr != 0 ){
            //LIDepPtr = *DepShadowPtr;
            LSDep *LIDepTrue = (*DepShadowPtr)->TrueDep; //TraceRef.LIDep->TrueDep; 
            while( LIDepTrue != 0 ){
              if(LIDepTrue->SrcPC == WIDPtr->PC){
                inH = 1;
                break;
              }
              LIDepTrue = LIDepTrue->next;
            }
          }
#endif
          if(!inH){
            LITDep[(long)PC].insert(WIDPtr->PC);
          }
#ifdef _DEBUGINFO
          std::cout<<"doReadLIDWithSinkPCAsKey: LITDep SinkPC = " << PC << ", SrcPC = " << WIDPtr->PC << "\n";
#endif
        }
      }
    }

    return ;
  }

#else
  void doReadLIDAWithSinkPCAsKey(void * PC, unsigned char ReadMask, LSPresentRWTrace &PRWTraceRef, Trace &TraceRef)
  {
    unsigned char RWMask;

    // 1) check for LIDep.
    if( (RWMask = (ReadMask & PRWTraceRef.GlobalWriteMask)) ) {
      LSPresentRWAddrID * WIDPtr = PRWTraceRef.WTrace;
      unsigned char NewRWMask;
      //std::map<long, std::set<long> > &LITDep = ThrLITDep[ThreadID];
      std::map<long, std::set<long> > &LITDep = next_lidep->LITDep;

      //for( ; RWMask && (WIDPtr != 0) ; WIDPtr = WIDPtr->Next )
      for( ; WIDPtr != 0 ; WIDPtr = WIDPtr->Next ){
        NewRWMask = RWMask & WIDPtr->RWMask;
        //NewRWMask = RWMask;
        //RWMask &= RWMask ^ WIDPtr->RWMask;

        // LI True Dep?
        // WIDPtr->PC ==> (long)PC
        if( NewRWMask ){
          bool inH = 0;
#ifdef _OPT2
          LSDepInfo **DepShadowPtr;
          //LSDepInfo *LIDepPtr;
          DepShadowPtr = (LSDepInfo**) AppAddrToShadow((long)PC);
          if( *DepShadowPtr != 0 ){
            //LIDepPtr = *DepShadowPtr;
            LSDep *LIDepTrue = (*DepShadowPtr)->TrueDep; //TraceRef.LIDep->TrueDep; 
            while( LIDepTrue != 0 ){
              if(LIDepTrue->SrcPC == WIDPtr->PC){
                inH = 1;
                break;
              }
              LIDepTrue = LIDepTrue->next;
            }
          }
#endif
          if(!inH){
            LITDep[(long)PC].insert(WIDPtr->PC);
          }
#ifdef _DEBUGINFO
          std::cout<<"doReadLIDWithSinkPCAsKey: LITDep SinkPC = " << PC << ", SrcPC = " << WIDPtr->PC << "\n";
#endif
        }
      }
    }

    return ;
  }
#endif

  void updatePresentRTrace(void *PC, void **ShadowAddr, unsigned char ReadMask,
      LSPresentRWTrace &PRWTraceRef, int WTEmptyFlag )
  {

    // printf("insert load addr = %p pc = %p\n",ShadowAddr, (void*) PC );
    unsigned char NewReadMask;
    LSPresentRWAddrID *RIDPtr;

    // No Write before within cur iteration.
    unsigned char FirstNewReadMask;
    unsigned char OldGlobalReadMask = PRWTraceRef.GlobalReadMask; 
    unsigned char NewGlobalReadMask = OldGlobalReadMask | ReadMask;

    // 1) Having New-Read bytes.
    if(  (NewReadMask = (NewGlobalReadMask ^ OldGlobalReadMask)) ) {
      // 1.1) The New-Read bytes have not been written before.
      if( FirstNewReadMask = (NewReadMask & (NewReadMask ^ PRWTraceRef.GlobalWriteMask)) ){

        // 1.2) Get the RWTrace buf to insert the New-Read-Not-Written bytes. 
        // 1.2.1) There are free RWTrace buf. NewBuf->next = RTrace; RTrace = NewBuf;
#ifdef _OVERHEAD_PROF
        LSPresentTraceMallocTime++;
#endif
        RIDPtr = (LSPresentRWAddrID*) malloc (sizeof(LSPresentRWAddrID)); // 
        RIDPtr->Next = PRWTraceRef.RTrace;
        PRWTraceRef.RTrace = RIDPtr;

#ifdef _DEBUGINFO
        //printf("Insert Read PresentTable Addr = %p \n",ShadowAddr );
#endif
        // 4) Fill content.
        RIDPtr->PC = (long)PC;
        RIDPtr->RWMask = FirstNewReadMask;
        PRWTraceRef.GlobalReadMask |= FirstNewReadMask;  // 
      }
    }

    return;
  }

  // PresentWTrace: Last Write.
  // 1) The write operation is last, so it must be inserted here.
  // 2) Addr->(PC, RWMask), every item maintains 
  void updatePresentWTrace(void *PC, void **ShadowAddr, unsigned char WriteMask,
      LSPresentRWTrace &PRWTraceRef, int WTEmptyFlag )
  {
    unsigned char DifWriteMask, NewWriteMask, RemainWriteMask;
    unsigned char OldGlobalWriteMask = PRWTraceRef.GlobalWriteMask; 
    unsigned char NewGlobalWriteMask = PRWTraceRef.GlobalWriteMask = OldGlobalWriteMask | WriteMask;
    LSPresentRWAddrID *WIDPtr, *PrevWIDPtr;
    //LSPresentRWAddrID *CurBuf;

    // 1) Firt time to write whole WriteMask....
    // May miss some deps. (different pc have the same WriteMask.
    if( WriteMask == (OldGlobalWriteMask ^ NewGlobalWriteMask) ){
      // 2 Opt me? Need merged with other ReadMask?
      // 2.1) buffer reuse.
#ifdef _OVERHEAD_PROF
      //LSPresentTraceMallocTime++;
#endif
      WIDPtr = (LSPresentRWAddrID*) malloc(sizeof(LSPresentRWAddrID));
      WIDPtr->Next = PRWTraceRef.WTrace;
      PRWTraceRef.WTrace = WIDPtr;

#ifdef _DEBUGINFO
      //printf("Insert Write PresentTable Addr = %p \n",ShadowAddr );
#endif
      WIDPtr->PC = (long)PC;
      WIDPtr->RWMask = WriteMask;   
      return;
    }

    // 2) Check overlap with all preceding write.
    int InsertNew = 1; // label whether has insert the NewWrite info.
    // 1100->1111: del preceding write;
    // 1100->0110: del preced and add (1000, prev-pc), (0110, cur-pc);
    WIDPtr = PRWTraceRef.WTrace;

    // case 1): ==  killer,CurRWMask == NewRWMask.
    if( WriteMask == WIDPtr->RWMask){
      WIDPtr->PC = (long) PC;
      return;
    }

    PrevWIDPtr = WIDPtr;
    NewWriteMask = WriteMask;
    for( ; WIDPtr != 0 && NewWriteMask; ){
      DifWriteMask = WIDPtr->RWMask ^ NewWriteMask;
      RemainWriteMask =  WIDPtr->RWMask & DifWriteMask; // WIDPtr Write-bytes before.
      //NewWriteMask = NewWriteMask & DifWriteMask; // New Write-Mask not overlap. fix me?
      //NewWriteMask = NewWriteMask & DifWriteMask; 

      // case 2) No overlap. No overlap: 1100->0011
      if( RemainWriteMask == WIDPtr->RWMask ){
        PrevWIDPtr = WIDPtr;
        WIDPtr = WIDPtr->Next;
        continue;
      }
      // case 3) < || ==  0110->1110, <
      if( !RemainWriteMask ){
        // The first node of <, changed to new Write operation. 
        if( InsertNew ){
          WIDPtr->PC = (long) PC;
          WIDPtr->RWMask = NewWriteMask; // bug?
          PrevWIDPtr = WIDPtr;
          WIDPtr = PrevWIDPtr->Next;
          InsertNew = 0;
        }
        else{
          PrevWIDPtr->Next = WIDPtr->Next;
          free(WIDPtr);
          WIDPtr = PrevWIDPtr->Next;
        }
#ifdef _OVERHEAD_PROF
        //LSPresentBufFreeTime++;
#endif
        // Free LSPresentBuf. opt ?
        // // need free.
        NewWriteMask = NewWriteMask & DifWriteMask; 
      }
      // case 4) > 1110->1100 || 0110->0011 
      // Just update the pre-write mask first.
      // case 5) Part overlap, NewWriteMask && WriteMask.
      // 0110->0011 ===> 
      else if( RemainWriteMask ){
        WIDPtr->RWMask = RemainWriteMask;
        PrevWIDPtr = WIDPtr;
        WIDPtr = WIDPtr->Next;
        NewWriteMask = NewWriteMask & DifWriteMask; 
      }
      else{
        std::cout<<"updatePresentWTrace error \n";
      }
    }

    if ( InsertNew && NewWriteMask ){
      // 2) Get present WTrace buf.
      // 2.1) There are free WTrace bufs.
#ifdef _OVERHEAD_PROF
      //LSPresentTraceMallocTime++;
#endif
      WIDPtr = (LSPresentRWAddrID *)malloc(sizeof(LSPresentRWAddrID)); 
      WIDPtr->Next = PRWTraceRef.WTrace;  // Insert new buffer in the beginning. 
      PRWTraceRef.WTrace = WIDPtr;
#ifdef _DEBUGINFO
      //printf("Insert Write PresentTable Addr = %p \n",ShadowAddr );
#endif
      // 3) Fill the content.
      WIDPtr->PC = (long) PC;
      WIDPtr->RWMask = NewWriteMask;
    }

    return;
  }


  // 1) PresentRWTrace is setup while doing LIDA, and just keep FirstRead LastWrite
  // operations.
  // 2) Every iteration writes its RWTrace into its own Present[idx] buffer.
  // 3) The PresentRWTrace is released when doing the LCDA.
#if defined(_DDA_FOUR_STAGES_PP_PARLIDA) 
  void doLIDAandUpdatePresentRTrace(void *PC, void **ShadowAddr, unsigned char ReadMask, int LoopIter, int TaskNum, int ThrID, int CurProc)
  {
    // 1) Get the right RWTrace Buf.
    Trace  &TraceRef = *((Trace*)(*ShadowAddr)); // hot
    // 1.1) First access the address, no buffer for Trace.
    // Not need?  Should have been allocated in ReadRWTraceFilter.
    // donothing;
    // *ShadowAddr == NULL;

    // 1.2) App has read/written the address before.  
    LSPresentRWTrace &PRWTraceRef = *(TraceRef.Present[ProcWPresNum[CurProc]]);
    // if PRWTraceRef.RWIter != 0, then all the previous PresentRWTrace has been
    // removed.
    // 1.2.1) Enter the next iteration.
    // To remove. 20141129.
    if(  PRWTraceRef.RWIter != LoopIter )  { 

      // Keep Loop Iteration RWTrace.
#ifdef _OPT1
#ifdef _GAS_SHADOW_PARLIDA
      //PLIDAShdAddr[ThreadID][PLIDACurNum[ThreadID]] = (long)ShadowAddr;
      //PLIDACurNum[ThreadID]++;
      LAccShdAddr[LCurNum] = (long)ShadowAddr;
      LCurNum++;
#endif
#if defined(_PARLIDA) 
      ProcAccAddrOfIter[CurProc][TaskNum] = 1;
#endif

      // LIDAAddrInterval.insert( (long)ShadowAddr );
#endif

      PRWTraceRef.RWIter = LoopIter;
      // Not delete the precede iterations's PresentWTrace buffer. free?
      //PRWTraceRef.GlobalWriteMask =  0;
      //PRWTraceRef.GlobalReadMask =  0; 
    }
    // 1.2.2) Read/Write in the same iteration. 
    // do-nothing.

    // 2) Do LIDA and update PresentRTrace table. Opt ?
    if( PRWTraceRef.GlobalWriteMask )
      doReadLIDAWithSinkPCAsKey( PC, ReadMask, PRWTraceRef, TraceRef, ThrID, CurProc);

    // 3) Setup or Update PresentTrace table.
    updatePresentRTrace(PC, ShadowAddr, ReadMask, PRWTraceRef, 0);

    return;
  }

  // 1) PresentRWTrace is setup while doing LIDA, and just keep FirstRead LastWrite
  // operations.
  // 2) Every iteration writes its RWTrace into its own Present[idx] buffer.
  // 3) The PresentRWTrace is released when doing the LCDA.
#elif defined(_DDA_FOUR_STAGES_PARLIDA)
  void doLIDAandUpdatePresentRTrace(void *PC, void **ShadowAddr, unsigned char ReadMask, int LoopIter, int TaskNum, int ThrID)
  {
    // 1) Get the right RWTrace Buf.
    Trace  &TraceRef = *((Trace*)(*ShadowAddr)); // hot
    // 1.1) First access the address, no buffer for Trace.
    // Not need?  Should have been allocated in ReadRWTraceFilter.
    // donothing;
    // *ShadowAddr == NULL;

    // 1.2) App has read/written the address before.  
    LSPresentRWTrace &PRWTraceRef = *(TraceRef.Present[LWPresNum]);
    // if PRWTraceRef.RWIter != 0, then all the previous PresentRWTrace has been
    // removed.
    // 1.2.1) Enter the next iteration.
    // To remove. 20141129.
    if(  PRWTraceRef.RWIter != LoopIter )  { 

      // Keep Loop Iteration RWTrace.
#ifdef _OPT1
#ifdef _GAS_SHADOW_PARLIDA
      //PLIDAShdAddr[ThreadID][PLIDACurNum[ThreadID]] = (long)ShadowAddr;
      //PLIDACurNum[ThreadID]++;
      LAccShdAddr[LCurNum] = (long)ShadowAddr;
      LCurNum++;
#endif
#if defined(_PARLIDA) 
      AccAddrOfIter[TaskNum] = 1;
#endif

      // LIDAAddrInterval.insert( (long)ShadowAddr );
#endif

      PRWTraceRef.RWIter = LoopIter;
      // Not delete the precede iterations's PresentWTrace buffer. free?
      //PRWTraceRef.GlobalWriteMask =  0;
      //PRWTraceRef.GlobalReadMask =  0; 
    }
    // 1.2.2) Read/Write in the same iteration. 
    // do-nothing.

    // 2) Do LIDA and update PresentRTrace table. Opt ?
    if( PRWTraceRef.GlobalWriteMask )
      //doReadLIDA(PC, ReadMask, PRWTraceRef, TraceRef);
      doReadLIDAWithSinkPCAsKey( PC, ReadMask, PRWTraceRef, TraceRef, ThrID);

    // 3) Setup or Update PresentTrace table.
    updatePresentRTrace(PC, ShadowAddr, ReadMask, PRWTraceRef, 0);

    return;
  }

#else
  void doLIDAandUpdatePresentRTrace(void *PC, void **ShadowAddr, unsigned char ReadMask, int LoopIter)
  {
    // 1) Get the right RWTrace Buf.
    Trace  &TraceRef = *((Trace*)(*ShadowAddr)); // hot

    // 1.1) First access the address, no buffer for Trace.
    // Not need?  Should have been allocated in ReadRWTraceFilter.
    // donothing;
    // *ShadowAddr == NULL;

    // 1.2) App has read/written the address before.  
    LSPresentRWTrace &PRWTraceRef = *(TraceRef.Present[LWPresNum]);
    // if PRWTraceRef.RWIter != 0, then all the previous PresentRWTrace has been
    // removed.
    // 1.2.1) Enter the next iteration.
    // To remove. 20141129.
    if(  PRWTraceRef.RWIter != LoopIter )  { 

      // Keep Loop Iteration RWTrace.
#ifdef _OPT1
      LIDAAddrInterval.insert( (long)ShadowAddr );
#endif

      PRWTraceRef.RWIter = LoopIter;
      // Not delete the precede iterations's PresentWTrace buffer. free?
      //PRWTraceRef.GlobalWriteMask =  0;
      //PRWTraceRef.GlobalReadMask =  0; 
    }
    // 1.2.2) Read/Write in the same iteration. 
    // do-nothing.

    // 2) Do LIDA and update PresentRTrace table. Opt ?
    if( PRWTraceRef.GlobalWriteMask )
      //doReadLIDA(PC, ReadMask, PRWTraceRef, TraceRef);
      doReadLIDAWithSinkPCAsKey( PC, ReadMask, PRWTraceRef, TraceRef);

    // 3) Setup or Update PresentTrace table.
    updatePresentRTrace(PC, ShadowAddr, ReadMask, PRWTraceRef, 0);

    return;
  }
#endif

  // Present[1] when serial execution; Present[IterationNO] when parallel 
  // execution. 
  // 
#if defined (_DDA_FOUR_STAGES_PP_PARLIDA)
  void doLIDAandUpdatePresentWTrace(void *PC, void **ShadowAddr, unsigned char  WriteMask, int LoopIter, int TaskNum, int ThrID, int CurProc )
  {
    // 1) Get the Present buf.
    Trace &TraceRef =  *((Trace*)(*ShadowAddr));

    // 1.1) First access the address, so create a new present/precede buffer.
    // done in ReadLIDA::operator.

    // 1.2) App has read/written the address before.
    // 1.2.1) The Present[] is 0.

    // 1.2.2) A new iteratrion.
    // To remove.
    if( TraceRef.Present[ProcWPresNum[CurProc]]->RWIter != LoopIter ) {
      // Keep Loop Iteration RWTrace.
#ifdef __OPT1
      //   LIDAAddrInterval.insert( (long)ShadowAddr );
#ifdef _GAS_SHADOW_PARLIDA
      //PLIDAShdAddr[ThreadID][PLIDACurNum[ThreadID]] = (long)ShadowAddr;
      //PLIDACurNum[ThreadID]++;
      LAccShdAddr[LCurNum] = (long)ShadowAddr;
      LCurNum++;
#endif

#ifdef _PARLIDA
      ProcAccAddrOfIter[CurProc][TaskNum] = 1;
#endif

#endif
      // Opt me?
      // Reset tags of Present-Read-Trace.
      //PRWTraceRef.RTrace.clear();
      TraceRef.Present[ProcWPresNum[CurProc]]->GlobalReadMask = 0;
      TraceRef.Present[ProcWPresNum[CurProc]]->GlobalWriteMask = 0;
      TraceRef.Present[ProcWPresNum[CurProc]]->RWIter = LoopIter;
    }
    // 1.2.2) Still in the same iteration.
    // do-nothing.

    // 4) There are maybe dependences.

    // Not write to the Address before within the current interation.
    LSPresentRWTrace &PRWTraceRef = *(TraceRef.Present[ProcWPresNum[CurProc]]);
    if( PRWTraceRef.GlobalReadMask )
      doWriteLIDAWithSinkPCAsKey( PC, WriteMask, PRWTraceRef, TraceRef, ThrID, CurProc);

    updatePresentWTrace(PC, ShadowAddr, WriteMask, PRWTraceRef, 0);

    return;
  }


  // Present[1] when serial execution; Present[IterationNO] when parallel 
  // execution. 
  // 
#elif defined(_DDA_FOUR_STAGES_PARLIDA) 
  void doLIDAandUpdatePresentWTrace(void *PC, void **ShadowAddr, unsigned char  WriteMask, int LoopIter, int TaskNum, int ThrID )
  {
    // 1) Get the Present buf.
    Trace &TraceRef =  *((Trace*)(*ShadowAddr));

    // 1.1) First access the address, so create a new present/precede buffer.
    // done in ReadLIDA::operator.

    // 1.2) App has read/written the address before.
    // 1.2.1) The Present[] is 0.

    // 1.2.2) A new iteratrion.
    // To remove.
    if( TraceRef.Present[LWPresNum]->RWIter != LoopIter ) {
      // Keep Loop Iteration RWTrace.
#ifdef __OPT1
      //   LIDAAddrInterval.insert( (long)ShadowAddr );
#ifdef _GAS_SHADOW_PARLIDA
      //PLIDAShdAddr[ThreadID][PLIDACurNum[ThreadID]] = (long)ShadowAddr;
      //PLIDACurNum[ThreadID]++;
      LAccShdAddr[LCurNum] = (long)ShadowAddr;
      LCurNum++;
#endif

#ifdef _PARLIDA
      AccAddrOfIter[TaskNum] = 1;
#endif

#endif
      // Opt me?
      // Reset tags of Present-Read-Trace.
      //PRWTraceRef.RTrace.clear();
      TraceRef.Present[LWPresNum]->GlobalReadMask = 0;
      TraceRef.Present[LWPresNum]->GlobalWriteMask = 0;
      TraceRef.Present[LWPresNum]->RWIter = LoopIter;
    }
    // 1.2.2) Still in the same iteration.
    // do-nothing.

    // 4) There are maybe dependences.

    // Not write to the Address before within the current interation.
    LSPresentRWTrace &PRWTraceRef = *(TraceRef.Present[LWPresNum]);
    if( PRWTraceRef.GlobalReadMask )
      //doWriteLIDA(PC, WriteMask, PRWTraceRef, TraceRef);
      doWriteLIDAWithSinkPCAsKey( PC, WriteMask, PRWTraceRef, TraceRef, ThrID);

    updatePresentWTrace(PC, ShadowAddr, WriteMask, PRWTraceRef, 0);

    return;
  }
#else
  void doLIDAandUpdatePresentWTrace(void *PC, void **ShadowAddr, unsigned char  WriteMask, int LoopIter )
  {
    // 1) Get the Present buf.
    Trace &TraceRef =  *((Trace*)(*ShadowAddr));

    // 1.1) First access the address, so create a new present/precede buffer.
    // done in ReadLIDA::operator.

    // 1.2) App has read/written the address before.
    // 1.2.1) The Present[] is 0.

    // 1.2.2) A new iteratrion.
    // To remove.
    if( TraceRef.Present[LWPresNum]->RWIter != LoopIter ) {
      // Keep Loop Iteration RWTrace.
#ifdef __OPT1
      LIDAAddrInterval.insert( (long)ShadowAddr );
#endif
      // Opt me?
      // Reset tags of Present-Read-Trace.
      //PRWTraceRef.RTrace.clear();
      TraceRef.Present[LWPresNum]->GlobalReadMask = 0;
      TraceRef.Present[LWPresNum]->GlobalWriteMask = 0;
      TraceRef.Present[LWPresNum]->RWIter = LoopIter;
    }
    // 1.2.2) Still in the same iteration.
    // do-nothing.


    // 4) There are maybe dependences.
    // Not write to the Address before within the current interation.
    LSPresentRWTrace &PRWTraceRef = *(TraceRef.Present[LWPresNum]);
    if( PRWTraceRef.GlobalReadMask )
      //doWriteLIDA(PC, WriteMask, PRWTraceRef, TraceRef);
      doWriteLIDAWithSinkPCAsKey( PC, WriteMask, PRWTraceRef, TraceRef);

    updatePresentWTrace(PC, ShadowAddr, WriteMask, PRWTraceRef, 0);

    return;
  }
#endif

#ifdef _DDA_FOUR_STAGES_PARLIDA  // LIDAFilter::
  void parcheck(int LoopIter, int ThrID)
  {
#ifdef _TIME_PROF
    tbb::tick_count tstart, pststart;
    pststart = tbb::tick_count::now();
#endif
    long ShdAddr, AddrInnerOffset, len1;
    Trace *ShdTrcPtr;
    int ItemNum = 0, AddrIndex, RWFlag, AddrLen;
    unsigned  char RWMask1;
    AddrStreamItem *PtrItems;
    PtrItems = PLIDAPtrItems;
    int tnum = 0;

    for( ItemNum = 1; ItemNum < PLIDALength; PtrItems++, ItemNum++){
      AddrIndex = ((long) PtrItems->Addr & PARLIDAMASK)>>4;
      //AddrIndex = AddrIndex>>4;
      if( ThrID == AddrIndex ){
        //std::cout<<"ThrID " << ThrID <<", AddrIndex " << AddrIndex << "\n";
        //tnum++;
        ShdAddr = (long)PtrItems->Addr & 0x6ffffffffff8;
        AddrInnerOffset = ShdAddr & 0x0000000000000007;
        AddrLen = PtrItems->AddrLen;
        RWFlag = PtrItems->RWFlag;
        PtrItems->Addr = (int*)ShdAddr ;
        len1 = AddrInnerOffset + AddrLen; 
        RWMask1 = (1<<len1) - (1<<AddrInnerOffset); 
        ShdTrcPtr = *((Trace**)ShdAddr);

        AccAddrOfIter[ItemNum] = 0;

        // Not DoLIAD before, so apply buffer for the Shadow.
        if( ShdTrcPtr ){
          // Do LIDA.
          if( RWFlag )
            doLIDAandUpdatePresentWTrace(PtrItems->PC, (void**)ShdAddr, RWMask1, LoopIter, ItemNum, ThrID);
          else
            doLIDAandUpdatePresentRTrace(PtrItems->PC, (void**)ShdAddr, RWMask1, LoopIter, ItemNum, ThrID);
        } // end if( ShdTrcPtr->Precede == NULL)

        else{
          // Keep Loop Iteration RWTrace.
#ifdef __OPT1
          //LIDAAddrInterval.insert((long)ShdAddr);
          AccAddrofIter[ItemNum] = 1;
#endif
          Trace *TrcPtr = (Trace*) malloc(sizeof(Trace));
          *((Trace**)ShdAddr) = TrcPtr;
          TrcPtr->Precede = (LSPrecedeRWTrace*) malloc (sizeof(LSPrecedeRWTrace));
          TrcPtr->Precede->RTrace = 0;
          TrcPtr->Precede->WTrace = 0;
          TrcPtr->Precede->GlobalReadMask = TrcPtr->Precede->GlobalWriteMask = 0;

          for(int i = 0; i < PRESBUFNUM; i++){
            TrcPtr->Present[i] = new LSPresentRWTrace;   
            TrcPtr->Present[i]->WTrace = 0;
            TrcPtr->Present[i]->RTrace = 0;
            TrcPtr->Present[i]->GlobalWriteMask = 0;
            TrcPtr->Present[i]->GlobalReadMask = 0;
            TrcPtr->Present[i]->RWIter =  0; // bug?
          }

          // Add the RWTrace into the PresentRWTrace.
          LSPresentRWTrace &PRWTraceRef = *(TrcPtr->Present[LWPresNum]);
          PRWTraceRef.RWIter = LoopIter;
          //  TrcPtr->Present[LWPresNum]->RWIter =  LoopIter; // bug?
          if( RWFlag )
            updatePresentWTrace(PtrItems->PC, (void**)ShdAddr, RWMask1, PRWTraceRef, 0);
          else
            updatePresentRTrace( PtrItems->PC, (void**)ShdAddr, RWMask1, PRWTraceRef, 0);
        }

      } // if(ThrID == Index
    } // for( ItemNum = 1;
  }
#endif



  //
#ifdef _GAS_SHADOW_PARLIDA
  void check(int LoopIter, int ThreadID)
  {
    long ShdAddr;
    Trace*ShdTrcPtr;
    int Index,  *ShdSeq, Beg, End, AddrNum;
    int Num = 0, i, j;
    RWTraceShdArrayEle *ShdArray = &ShdArrayPool[LRStreamNum][0];
    RWTraceShdArrayEle *ShdArrayNode; 

    LCurNum = 0; // PLIDACurNum[ThreadID];
    LAccShdAddr = &PLIDAShdAddr[ThreadID][0];

    AddrNum = PLIDALength/LIDAThreadNum;
    Beg = AddrNum * ThreadID;
    End = Beg + AddrNum;
    if( ThreadID == (LIDAThreadNum-1) )
      End = PLIDALength;
    else
      End = Beg + AddrNum;
    if(!ThreadID) Beg = 1; // Beg == 0,
    for( j = Beg; j < End; j++){
      ShdAddr = PLIDAPtrShdAddr[j];
      ShdTrcPtr = *((Trace**)ShdAddr);
      int &CurNum = ShdTrcPtr->CurArrayNum[LRStreamNum];
      //ShdSeq = &ShdTrcPtr->ShdArraySequence[LRStreamNum][0]; 
      //Index = ShdSeq[0];

      // Not DoLIAD before, so apply buffer for the Shadow.
      if( !ShdTrcPtr->Precede ){
        // Keep Loop Iteration RWTrace.
#ifdef __OPT1
        //LIDAAddrInterval.insert( ShdAddr );
        //PLIDAShdAddr[ThreadID][PLIDACurNum[ThreadID]] = ShdAddr;
        //PLIDACurNum[ThreadID]++;
        LAccShdAddr[LCurNum] = ShdAddr;
        LCurNum++;
#endif

        ShdTrcPtr->Precede = (LSPrecedeRWTrace*) malloc (sizeof(LSPrecedeRWTrace));
        ShdTrcPtr->Precede->RTrace = 0;
        ShdTrcPtr->Precede->WTrace = 0;
        ShdTrcPtr->Precede->GlobalReadMask = ShdTrcPtr->Precede->GlobalWriteMask = 0;
        for(int j = 0; j < PRESBUFNUM; j++){
          ShdTrcPtr->Present[j] = new LSPresentRWTrace;   
          ShdTrcPtr->Present[j]->WTrace = 0;
          ShdTrcPtr->Present[j]->RTrace = 0;
          ShdTrcPtr->Present[j]->GlobalWriteMask = 0;
          ShdTrcPtr->Present[j]->GlobalReadMask = 0;
          ShdTrcPtr->Present[j]->RWIter =  0; // bug?
        }
        ShdTrcPtr->Present[LWPresNum]->RWIter =  LoopIter; // bug?
      } // end if( ShdTrcPtr->Precede == NULL)


      // 1.2) Read RWTrace info.
      for(int k = 0; k < CurNum; k++  ){
        //Index = ShdSeq[k];
        //ShdArrayNode = &ShdArray[Index];
        // 2) Do LIDA and setup presentRWTrace in shadow memory.
        if( ShdArrayNode->RWMask[0] ){
          doLIDAandUpdatePresentRTrace( (void*) ShdTrcPtr->ShdArray[LRStreamNum][k].PC, (void**)ShdAddr, ShdTrcPtr->ShdArray[LRStreamNum][k].RWMask[0], LoopIter,0, ThreadID);
        }
        else{
          doLIDAandUpdatePresentWTrace( (void*) ShdTrcPtr->ShdArray[LRStreamNum][k].PC, (void**)ShdAddr, ShdTrcPtr->ShdArray[LRStreamNum][k].RWMask[1], LoopIter,0, ThreadID);
        }
      }

      CurNum = 0;
      free(ShdTrcPtr->ShdArray[LRStreamNum]);
      PLIDACurNum[ThreadID] = LCurNum;
    }

  }
#endif


  private:
  //std::interval_set<long> ReadAddrInterval;
  // Output;
  class RWTrace* next_rwtrace;
  /*override*/ 
  void* operator()(void*); // why private?
  inline void doReadLIDA(void * PC, unsigned char ReadMask, LSPresentRWTrace &PRWTraceRef, Trace &TraceRef);
#if defined(_DDA_FOUR_STAGES_PARLIDA)
  inline void doWriteLIDAWithSinkPCAsKey(void * PC, unsigned char WriteMask, LSPresentRWTrace &PRWTraceRef, Trace &TraceRef, int ThreadID);

#elif defined (_DDA_FOUR_STAGES_PP_PARLIDA)
  inline void doWriteLIDAWithSinkPCAsKey(void * PC, unsigned char WriteMask, LSPresentRWTrace &PRWTraceRef, Trace &TraceRef, int ThreadID, int CurProc);
#else
  inline void doWriteLIDAWithSinkPCAsKey(void * PC, unsigned char WriteMask, LSPresentRWTrace &PRWTraceRef, Trace &TraceRef);
#endif
  inline void doWriteLIDA(void * PC, unsigned char WriteMask,LSPresentRWTrace &PRWTraceRef, Trace &TraceRef);

  private:
  class LICDep *next_lidep;
  // For LCDA, Thread local? Cross multi-task.
  long LIDAStackMin, LIDAStackMax, LIDAHeapMax, LIDAHeapMin, LIDAGlobalMin, LIDAGlobalMax;
  interval_set<long> LIDAAddrInterval; // For what?

  // Many Deps happens repeatly, so we keep a memo of these deps.
  // when reset?
  std::map<long,std::set<long> > HLITDep;
  std::map<long,std::set<long> > HLIADep;
  std::map<long,std::set<long> > HLIODep;

  int PLIDALength;
#ifdef _DDA_FOUR_STAGES_PARLIDA 
  std::map<long,std::set<long> > *ThrLITDep;
  std::map<long,std::set<long> > *ThrLIADep;
  std::map<long,std::set<long> > *ThrLIODep;
  int PLIDACurNum[LIDAThreadNum];
  int PLIDALength;
  long *PLIDAShdAddr[LIDAThreadNum];
  long *PLIDAPtrShdAddr; // ShdAddrStream generated by GenerateAddrStream. 
#endif


  // local opt.
  long LWPresNum, LRStreamNum;

};


LIDAFilter::LIDAFilter( ):
  filter(serial_in_order),
  LWPresNum(WPresNum)
  //next_rwtrace( RWTraceDep::allocate(0) )
{ 
#ifndef _DDA_FOUR_STAGES_PP_PARLIDA
  LRStreamNum = RStreamNum;
#endif
}

LIDAFilter::~LIDAFilter() 
{
  //next_rwtrace->free(); // not need, can be done by the follow filter.

}


/* Look for Loop independent output dependences. 
// Todo:
// 1) Addr1: W R R R R ===> not need to check True-dep every time.
// LIOD & LIAD.
// 2) The results are stored in the ShadowAddr(SinkPC).
// 3) Dist[0]->LIDep, Dist[1]->LCDep;
*/ 
#if defined(_DDA_FOUR_STAGES_PARLIDA) 
inline void  LIDAFilter::doWriteLIDAWithSinkPCAsKey(void * PC, unsigned char WriteMask, LSPresentRWTrace &PRWTraceRef, Trace &TraceRef, int ThrID)
{
  unsigned char RWMask;
  unsigned char NewRWMask;

  if( (RWMask = ( WriteMask & PRWTraceRef.GlobalWriteMask)) ) {
    LSPresentRWAddrID *WIDPtr = PRWTraceRef.WTrace;
    std::map<long, std::set<long> > &LIODep = ThrLIODep[ThrID]; // bug?
    //std::map<long, std::set<long> > &LIODep = next_lidep->LIODep; // bug?
    std::map<long, std::set<long> >::iterator it;

    for( ; (WIDPtr != 0 ) && RWMask; WIDPtr = WIDPtr->Next){
      NewRWMask = RWMask;
      RWMask &= RWMask ^ WIDPtr->RWMask;
      // LI-Output.
      // WIDPtr->PC ===> (long)PC
      if( RWMask != NewRWMask ){
#ifdef _DEBUGINFO_LIDA1
        //std::cout<<"doWriteLIDWithSinkPCAsKeyLICDepPA: LIODep SinkPC = " << PC << "\n";
#endif
        long SinkPC = (long) PC;
        long SrcPC = WIDPtr->PC;
        bool inH = 0;
#if 1
        LSDepInfo **DepShadowPtr;
        //LSDepInfo *LIDepPtr;
        DepShadowPtr = (LSDepInfo**) AppAddrToShadow(SinkPC);
        if( *DepShadowPtr != 0 ){
          //LIDepPtr = *DepShadowPtr;
          LSDep *LIDepOut = (*DepShadowPtr)->OutDep; //TraceRef.LIDep->TrueDep; 
          while( LIDepOut != 0 ){
            if(LIDepOut->SrcPC == SrcPC){
              inH = 1;
              break;
            }
            LIDepOut = LIDepOut->next;
          }
        }
#endif
        if( !inH )
          LIODep[SinkPC].insert(SrcPC);
#ifdef _DEBUGINFO
        std::cout<<"doWriteLIDWithSinkPCAsKey: LIODep SinkPC = " << PC << ", SrcPC = " << WIDPtr->PC << "\n";
#endif

      }
    } 
  }


  // LIAD.
  if( (RWMask = ( WriteMask & PRWTraceRef.GlobalReadMask)) ) {

    //std::map<long, std::set<long> > &LIADep = next_lidep->LIADep;
    std::map<long, std::set<long> > &LIADep = ThrLIADep[ThrID];
    //std::map<long, std::set<long> >::iterator it;

    LSPresentRWAddrID *RIDPtr = PRWTraceRef.RTrace;
    for( ; (RIDPtr != 0) && RWMask; RIDPtr = RIDPtr->Next){
      NewRWMask = RWMask;
      RWMask &= RWMask ^ RIDPtr->RWMask;

      // LI-AntiDep.
      // RIDPtr->PC ===>  (long)PC
      if( RWMask != NewRWMask ){
#ifdef _DEBUGINFO1
        //std::cout<<"doWriteLIDWithSinkPCAsKeyLICDepPA: LIADep SinkPC = " << PC << "\n";
#endif
        long SinkPC = (long)PC;
        long SrcPC = RIDPtr->PC;
        bool inH = 0;
#if 1
        LSDepInfo **DepShadowPtr;
        //LSDepInfo *LIDepPtr;
        DepShadowPtr = (LSDepInfo**) AppAddrToShadow(SinkPC);
        if( *DepShadowPtr != 0 ){
          //LIDepPtr = *DepShadowPtr;
          LSDep *LIDepAnti = (*DepShadowPtr)->AntiDep; //TraceRef.LIDep->TrueDep; 
          while( LIDepAnti != 0 ){
            if(LIDepAnti->SrcPC == SrcPC ){
              inH = 1;
              break;
            }
            LIDepAnti = LIDepAnti->next;
          }
        }
#endif
        if(!inH){
          LIADep[SinkPC].insert(SrcPC);
        }
#ifdef _DEBUGINFO
        std::cout<<"doWriteLIDWithSinkPCAsKey: LIADep SinkPC = " << PC << ", SrcPC = " << RIDPtr->PC <<   "\n";
#endif

      }
    } 

  }

  return;
}
#else

  inline void 
LIDAFilter::doWriteLIDAWithSinkPCAsKey(void * PC, unsigned char WriteMask, LSPresentRWTrace &PRWTraceRef, Trace &TraceRef)
{
  unsigned char RWMask;
  unsigned char NewRWMask;

  if( (RWMask = ( WriteMask & PRWTraceRef.GlobalWriteMask)) ) {
    LSPresentRWAddrID *WIDPtr = PRWTraceRef.WTrace;
#if 1
    //std::map<long, std::set<long> > &LIODep = ThrLIODep[ThreadID]; // bug?
    std::map<long, std::set<long> > &LIODep = next_lidep->LIODep; // bug?
    std::map<long, std::set<long> >::iterator it;
#endif
    for( ; (WIDPtr != 0 ) && RWMask; WIDPtr = WIDPtr->Next){
      NewRWMask = RWMask;
      RWMask &= RWMask ^ WIDPtr->RWMask;
      // LI-Output.
      // WIDPtr->PC ===> (long)PC
      if( RWMask != NewRWMask ){
#ifdef _DEBUGINFO_LIDA1
        //std::cout<<"doWriteLIDWithSinkPCAsKeyLICDepPA: LIODep SinkPC = " << PC << "\n";
#endif
        long SinkPC = (long) PC;
        long SrcPC = WIDPtr->PC;
        bool inH = 0;
#if 1
        LSDepInfo **DepShadowPtr;
        //LSDepInfo *LIDepPtr;
        DepShadowPtr = (LSDepInfo**) AppAddrToShadow(SinkPC);
        if( *DepShadowPtr != 0 ){
          //LIDepPtr = *DepShadowPtr;
          LSDep *LIDepOut = (*DepShadowPtr)->OutDep; //TraceRef.LIDep->TrueDep; 
          while( LIDepOut != 0 ){
            if(LIDepOut->SrcPC == SrcPC){
              inH = 1;
              break;
            }
            LIDepOut = LIDepOut->next;
          }
        }
#endif
        if( !inH )
          LIODep[SinkPC].insert(SrcPC);
#ifdef _DEBUGINFO
        std::cout<<"doWriteLIDWithSinkPCAsKey: LIODep SinkPC = " << PC << ", SrcPC = " << WIDPtr->PC << "\n";
#endif

      }
    } 
  }


  // LIAD.
  if( (RWMask = ( WriteMask & PRWTraceRef.GlobalReadMask)) ) {

#if 1
    std::map<long, std::set<long> > &LIADep = next_lidep->LIADep;
    //std::map<long, std::set<long> > &LIADep = ThrLIADep[TaskId];
    //std::map<long, std::set<long> >::iterator it;
#endif

    LSPresentRWAddrID *RIDPtr = PRWTraceRef.RTrace;
    for( ; (RIDPtr != 0) && RWMask; RIDPtr = RIDPtr->Next){
      NewRWMask = RWMask;
      RWMask &= RWMask ^ RIDPtr->RWMask;

      // LI-AntiDep.
      // RIDPtr->PC ===>  (long)PC
      if( RWMask != NewRWMask ){
#ifdef _DEBUGINFO1
        //std::cout<<"doWriteLIDWithSinkPCAsKeyLICDepPA: LIADep SinkPC = " << PC << "\n";
#endif
        long SinkPC = (long)PC;
        long SrcPC = RIDPtr->PC;
        bool inH = 0;
#if 1
        LSDepInfo **DepShadowPtr;
        //LSDepInfo *LIDepPtr;
        DepShadowPtr = (LSDepInfo**) AppAddrToShadow(SinkPC);
        if( *DepShadowPtr != 0 ){
          //LIDepPtr = *DepShadowPtr;
          LSDep *LIDepAnti = (*DepShadowPtr)->AntiDep; //TraceRef.LIDep->TrueDep; 
          while( LIDepAnti != 0 ){
            if(LIDepAnti->SrcPC == SrcPC ){
              inH = 1;
              break;
            }
            LIDepAnti = LIDepAnti->next;
          }
        }
#endif
        if(!inH){
          LIADep[SinkPC].insert(SrcPC);
        }
#ifdef _DEBUGINFO
        std::cout<<"doWriteLIDWithSinkPCAsKey: LIADep SinkPC = " << PC << ", SrcPC = " << RIDPtr->PC <<   "\n";
#endif

      }
    } 

  }

  return;
}
#endif

  inline
void LIDAFilter::doWriteLIDA(void * PC, unsigned char WriteMask, 
    LSPresentRWTrace &PRWTraceRef, Trace &TraceRef )
{


  // 1) Take as the key the Write-Address to store data dependence results.
#ifdef __ADDRKEY
  __doWriteLIDWithAddrAsKey(PC, WriteMask, PRWTraceRef, TraceRef);
#endif
  // 2) Take as the key the SrcPC of dependence pair to store data dependence
  // results.
  //__doWriteLIDWithSinkPCAsKey( PC, WriteMask, PRWTraceRef, TraceRef);

  // 3)
  doWriteLIDAWithSinkPCAsKey( PC, WriteMask, PRWTraceRef, TraceRef);

  return;
}



/* 1) Read RWTrace from Streams[];
 ** 2) Do LIDA;
 ** 3) Setup the PresentRWTrace in shadow memory.
 ** 4) Check whether need LCDA.
 */
int num = 0;
void* LIDAFilter::operator()(void* item) 
{
  int LoopIter, Length;
  AddrStreamItem *PtrItems;
  short int RWFlag;
  //long LStackMin, LStackMax, LHeapMax, LHeapMin, LGlobalMin, LGlobalMax;

#ifdef _TIME_PROF
  tbb::tick_count tstart, pststart;
  pststart = tbb::tick_count::now();
#endif

  // bug?  Once read a Stream[], will finish it.

  class RWTrace &input = *static_cast<class RWTrace*>(item);

  Length = input.Streams->End;
  LoopIter = input.Streams->LoopIter;
  PtrItems = &input.Streams->Items[1];
  RWFlag = input.Streams->Items[0].RWFlag;

  // 
  next_lidep = new LICDep; //

#ifdef _TIME_PROF
  tbb::tick_count prestart, prostart;
#endif

#ifdef _DDA_FOUR_STAGES_PARLIDA

  // Par process.
  if( Length > 1 ){
    struct ParLIDA PLIDAC;
    PLIDAC.LoopIter = LoopIter;
    PLIDAC.LIDAPtr = this;
    PLIDAC.LRStreamNum = LRStreamNum;
    PLIDAC.LWPresNum = LWPresNum;
    PLIDAC.CurProc = CurProc;
    PLIDALength = Length;
    PLIDAPtrItems[CurProc] = &input.Streams->Items[1];  // PRIVATE.

    ThrLITDep[CurProc] = new std::map<long, std::set<long> > [RLIDATASKNUM];
    ThrLIADep[CurProc] = new std::map<long, std::set<long> > [RLIDATASKNUM];
    ThrLIODep[CurProc] = new std::map<long, std::set<long> > [RLIDATASKNUM];


#ifdef _TIME_PROF
    prostart = tbb::tick_count::now();
#endif
    //task_scheduler_init LIDAInit(LIDAThreadNum);
    parallel_for( blocked_range<int>(0, RLIDATASKNUM), PLIDAC);

#ifdef _TIME_PROF
    PROTIME += (tbb::tick_count::now() -prostart).seconds();
#endif
    //  num++;
    // std::cout<<"num " << num <<", PROTIME " << PROTIME <<"\n";

#ifdef _TIME_PROF
    prestart = tbb::tick_count::now();
#endif


    // Reduction.
    // Reduction-1) ShdAddr-Interval for LCDA.
    for( int i = 1; i < Length; PtrItems++, i++){
      //std::cout<<std::setbase(16)<<"Insert Addr = "<<PLIDAShdAddr[i][j]<<"\n";
      if(AccAddrOfIter[i])
        LIDAAddrInterval[CurProc].insert( (long)PtrItems->Addr );
    }

#if 1
    // Reduction-2) Dep-info results.
    //std::unordered_map<long, std::set<long> >::iterator DepBeg, DepEnd;
    std::map<long, std::set<long> >::iterator DepBeg, DepEnd;
    std::set<long>::iterator SetBeg, SetEnd;
    for( int i = 0; i <  RLIDATASKNUM; i++){
      DepBeg = ThrLITDep[i].begin();  
      DepEnd = ThrLITDep[i].end();  
#if 1
      for( ;DepBeg != DepEnd; ++DepBeg){
        SetBeg = DepBeg->second.begin(); 
        SetEnd = DepBeg->second.end();
        for( ; SetBeg != SetEnd; ++SetBeg) {
          next_lidep->LITDep[(long)DepBeg->first].insert(*SetBeg); 
        }
      }
#endif
      //  next_lidep->LITDep.insert(DepBeg, DepEnd);

      DepBeg = ThrLIADep[i].begin();  
      DepEnd = ThrLIADep[i].end();  
#if 1
      for( ;DepBeg != DepEnd; ++DepBeg){
        SetBeg = DepBeg->second.begin(); 
        SetEnd = DepBeg->second.end();
        for( ; SetBeg != SetEnd; ++SetBeg) {
          next_lidep->LIADep[(long)DepBeg->first].insert(*SetBeg); 
        }
      }
#endif
      //  next_lidep->LIADep.insert(DepBeg, DepEnd);

      DepBeg = ThrLIODep[i].begin();  
      DepEnd = ThrLIODep[i].end();  
      for( ;DepBeg != DepEnd; ++DepBeg){
        SetBeg = DepBeg->second.begin(); 
        SetEnd = DepBeg->second.end();
        for( ; SetBeg != SetEnd; ++SetBeg) {
          next_lidep->LIODep[(long)DepBeg->first].insert(*SetBeg); 
        }
      }
#if 0
      ThrLITDep[i].clear();
      ThrLIADep[i].clear();
      ThrLIODep[i].clear();
#endif
      //    next_lidep->LIODep.insert(DepBeg, DepEnd);
    }
#endif

#if 1
    delete []ThrLITDep;
    delete []ThrLIADep;
    delete []ThrLIODep;
#endif
    //delete []AccAddrOfIter;
#ifdef _TIME_PROF
    PRETIME += (tbb::tick_count::now() -prestart).seconds();
#endif


  }

#endif

  //RWFlag = input.Streams->Items[0].RWFlag;
  free(input.Streams);
  delete(&input);

  if( RWFlag == 0 ){
    //std::cout<<"ReadLIDAFilter RWFlag = 0, RStreamNum = "<<RStreamNum<<"\n";
    next_lidep->StartLCDA = 0;
    next_lidep->CancelPipeline = 0;
#ifdef _TIME_PROF
    LIDATime += (tbb::tick_count::now() - pststart).seconds();
#endif 
    return next_lidep;

  }
  // Start LCDA.
  else if( RWFlag == 1 ){
    //std::cout<<"ReadLIDAFilter RWFlag = 1, RStreamNum = "<<RStreamNum<<"\n";
    //std::cout<<"ReadLIDAFilter RWFlag = 1, LoopIter = "<< LoopIter <<"\n";
    next_lidep->LoopIter = LoopIter;
    next_lidep->StartLCDA = 1;
    next_lidep->CancelPipeline = 0;
    PresIsFull[LWPresNum] = 1;

    // Update PresentRWTrace buffer.
    WPresNum = (WPresNum+1) % PRESBUFNUM;
    LWPresNum = WPresNum;

#ifdef _TIME_PROF
    tstart = tbb::tick_count::now();
#endif 
    while( PresIsFull[LWPresNum] );
#ifdef _TIME_PROF
    LIDAPTime += (tbb::tick_count::now() - tstart).seconds();
#endif 

    // LCDA address range.   To_OPT: copy-assignment. 
    next_lidep->LCDAAddrInterval = LIDAAddrInterval;
    LIDAAddrInterval.clear();

#ifdef _TIME_PROF
    LIDATime += (tbb::tick_count::now() - pststart).seconds();
#endif 
    return next_lidep;
  }
  // Cancel pipeline, read this buffer again, so not do RStreamNum += 1;
  else if( RWFlag == 2 ){
    next_lidep->StartLCDA = 0;
    next_lidep->CancelPipeline = 2;
    //Streams[LRStreamNum].Items[0].RWFlag = 3;
#ifdef _TIME_PROF
    LIDATime += (tbb::tick_count::now() - pststart).seconds();
#endif 
    return next_lidep;
  }



}
#endif // _FOUR_STAGES

// 
#ifdef _DDA_GAS_SHADOW
class ReadLIDAFilter: public tbb::filter{
#ifdef _GAS_SHADOW_PARLIDA
  struct ParLIDACheck{
    unsigned int LoopIter;
    class ReadLIDAFilter *LIDAPtr;
    void operator()(const blocked_range<int>& range) const{
      //std::cout<<setbase(16) <<"LIDAFilter pthread_id " << pthread_self() << "\n";
      for( int i = range.begin() ; i != range.end(); ++i){
        LIDAPtr->check(LoopIter, i );
      }
    }
  };
#endif


  public:
  ReadLIDAFilter();
  ~ReadLIDAFilter();

  // 20140127.
  // 1) LIDep/LCDep store in the same struct with different Dist;
  // Dist[0] = 0, keep the LID Dist whose value is 0.
  // 2) LIDep: take SinkPC as key.
  // 3) Pipeline Parallel analysis.
  // Anti-LIDep info.
#if defined(_GAS_SHADOW_PARLIDA) 
  void doReadLIDAWithSinkPCAsKey(void * PC, unsigned char ReadMask, LSPresentRWTrace &PRWTraceRef, Trace &TraceRef, int ThreadID)
  {
    unsigned char RWMask;

    // 1) check for LIDep.
    if( (RWMask = (ReadMask & PRWTraceRef.GlobalWriteMask)) ) {
      LSPresentRWAddrID * WIDPtr = PRWTraceRef.WTrace;
      unsigned char NewRWMask;
      std::map<long, std::set<long> > &LITDep = ThrLITDep[ThreadID];
      //std::map<long, std::set<long> > &LITDep = next_lidep->LITDep;

      //for( ; RWMask && (WIDPtr != 0) ; WIDPtr = WIDPtr->Next )
      for( ; WIDPtr != 0 ; WIDPtr = WIDPtr->Next ){
        NewRWMask = RWMask & WIDPtr->RWMask;
        //NewRWMask = RWMask;
        //RWMask &= RWMask ^ WIDPtr->RWMask;

        // LI True Dep?
        // WIDPtr->PC ==> (long)PC
        if( NewRWMask ){
          bool inH = 0;
#ifdef _OPT2
          LSDepInfo **DepShadowPtr;
          //LSDepInfo *LIDepPtr;
          DepShadowPtr = (LSDepInfo**) AppAddrToShadow((long)PC);
          if( *DepShadowPtr != 0 ){
            //LIDepPtr = *DepShadowPtr;
            LSDep *LIDepTrue = (*DepShadowPtr)->TrueDep; //TraceRef.LIDep->TrueDep; 
            while( LIDepTrue != 0 ){
              if(LIDepTrue->SrcPC == WIDPtr->PC){
                inH = 1;
                break;
              }
              LIDepTrue = LIDepTrue->next;
            }
          }
#endif
          if(!inH){
            LITDep[(long)PC].insert(WIDPtr->PC);
          }
#ifdef _DEBUGINFO
          std::cout<<"doReadLIDWithSinkPCAsKey: LITDep SinkPC = " << PC << ", SrcPC = " << WIDPtr->PC << "\n";
#endif
        }
      }
    }

    return ;
  }

#elif  // _GAS_SHADOW, serial.
  void doReadLIDAWithSinkPCAsKey(void * PC, unsigned char ReadMask, LSPresentRWTrace &PRWTraceRef, Trace &TraceRef)
  {
    unsigned char RWMask;

    // 1) check for LIDep.
    if( (RWMask = (ReadMask & PRWTraceRef.GlobalWriteMask)) ) {
      LSPresentRWAddrID * WIDPtr = PRWTraceRef.WTrace;
      unsigned char NewRWMask;
      //std::map<long, std::set<long> > &LITDep = ThrLITDep[ThreadID];
      std::map<long, std::set<long> > &LITDep = next_lidep->LITDep;

      //for( ; RWMask && (WIDPtr != 0) ; WIDPtr = WIDPtr->Next )
      for( ; WIDPtr != 0 ; WIDPtr = WIDPtr->Next ){
        NewRWMask = RWMask & WIDPtr->RWMask;
        //NewRWMask = RWMask;
        //RWMask &= RWMask ^ WIDPtr->RWMask;

        // LI True Dep?
        // WIDPtr->PC ==> (long)PC
        if( NewRWMask ){
          bool inH = 0;
#ifdef _OPT2
          LSDepInfo **DepShadowPtr;
          //LSDepInfo *LIDepPtr;
          DepShadowPtr = (LSDepInfo**) AppAddrToShadow((long)PC);
          if( *DepShadowPtr != 0 ){
            //LIDepPtr = *DepShadowPtr;
            LSDep *LIDepTrue = (*DepShadowPtr)->TrueDep; //TraceRef.LIDep->TrueDep; 
            while( LIDepTrue != 0 ){
              if(LIDepTrue->SrcPC == WIDPtr->PC){
                inH = 1;
                break;
              }
              LIDepTrue = LIDepTrue->next;
            }
          }
#endif
          if(!inH){
            LITDep[(long)PC].insert(WIDPtr->PC);
          }
#ifdef _DEBUGINFO
          std::cout<<"doReadLIDWithSinkPCAsKey: LITDep SinkPC = " << PC << ", SrcPC = " << WIDPtr->PC << "\n";
#endif
        }
      }
    }

    return ;
  }
#endif

  void updatePresentRTrace(void *PC, void **ShadowAddr, unsigned char ReadMask,
      LSPresentRWTrace &PRWTraceRef, int WTEmptyFlag )
  {

    // printf("insert load addr = %p pc = %p\n",ShadowAddr, (void*) PC );
    unsigned char NewReadMask;
    LSPresentRWAddrID *RIDPtr;

    // No Write before within cur iteration.
    unsigned char FirstNewReadMask;
    unsigned char OldGlobalReadMask = PRWTraceRef.GlobalReadMask; 
    unsigned char NewGlobalReadMask = OldGlobalReadMask | ReadMask;

    // 1) Having New-Read bytes.
    if(  (NewReadMask = (NewGlobalReadMask ^ OldGlobalReadMask)) ) {
      // 1.1) The New-Read bytes have not been written before.
      if( FirstNewReadMask = (NewReadMask & (NewReadMask ^ PRWTraceRef.GlobalWriteMask)) ){

        // 1.2) Get the RWTrace buf to insert the New-Read-Not-Written bytes. 
        // 1.2.1) There are free RWTrace buf. NewBuf->next = RTrace; RTrace = NewBuf;
#ifdef _OVERHEAD_PROF
        LSPresentTraceMallocTime++;
#endif
        RIDPtr = (LSPresentRWAddrID*) malloc (sizeof(LSPresentRWAddrID)); // 
        RIDPtr->Next = PRWTraceRef.RTrace;
        PRWTraceRef.RTrace = RIDPtr;

#ifdef _DEBUGINFO
        //printf("Insert Read PresentTable Addr = %p \n",ShadowAddr );
#endif
        // 4) Fill content.
        RIDPtr->PC = (long)PC;
        RIDPtr->RWMask = FirstNewReadMask;
        PRWTraceRef.GlobalReadMask |= FirstNewReadMask;  // 
      }
    }

    return;
  }

  // PresentWTrace: Last Write.
  // 1) The write operation is last, so it must be inserted here.
  // 2) Addr->(PC, RWMask), every item maintains 
  void updatePresentWTrace(void *PC, void **ShadowAddr, unsigned char WriteMask,
      LSPresentRWTrace &PRWTraceRef, int WTEmptyFlag )
  {
    unsigned char DifWriteMask, NewWriteMask, RemainWriteMask;
    unsigned char OldGlobalWriteMask = PRWTraceRef.GlobalWriteMask; 
    unsigned char NewGlobalWriteMask = PRWTraceRef.GlobalWriteMask = OldGlobalWriteMask | WriteMask;
    LSPresentRWAddrID *WIDPtr, *PrevWIDPtr;
    //LSPresentRWAddrID *CurBuf;

    // 1) Firt time to write whole WriteMask....
    // May miss some deps. (different pc have the same WriteMask.
    if( WriteMask == (OldGlobalWriteMask ^ NewGlobalWriteMask) ){
      // 2 Opt me? Need merged with other ReadMask?
      // 2.1) buffer reuse.
#ifdef _OVERHEAD_PROF
      //LSPresentTraceMallocTime++;
#endif
      WIDPtr = (LSPresentRWAddrID*) malloc(sizeof(LSPresentRWAddrID));
      WIDPtr->Next = PRWTraceRef.WTrace;
      PRWTraceRef.WTrace = WIDPtr;

#ifdef _DEBUGINFO
      //printf("Insert Write PresentTable Addr = %p \n",ShadowAddr );
#endif
      WIDPtr->PC = (long)PC;
      WIDPtr->RWMask = WriteMask;   
      return;
    }

    // 2) Check overlap with all preceding write.
    int InsertNew = 1; // label whether has insert the NewWrite info.
    // 1100->1111: del preceding write;
    // 1100->0110: del preced and add (1000, prev-pc), (0110, cur-pc);
    WIDPtr = PRWTraceRef.WTrace;

    // case 1): ==  killer,CurRWMask == NewRWMask.
    if( WriteMask == WIDPtr->RWMask){
      WIDPtr->PC = (long) PC;
      return;
    }

    PrevWIDPtr = WIDPtr;
    NewWriteMask = WriteMask;
    for( ; WIDPtr != 0 && NewWriteMask; ){
      DifWriteMask = WIDPtr->RWMask ^ NewWriteMask;
      RemainWriteMask =  WIDPtr->RWMask & DifWriteMask; // WIDPtr Write-bytes before.
      //NewWriteMask = NewWriteMask & DifWriteMask; // New Write-Mask not overlap. fix me?
      //NewWriteMask = NewWriteMask & DifWriteMask; 

      // case 2) No overlap. No overlap: 1100->0011
      if( RemainWriteMask == WIDPtr->RWMask ){
        PrevWIDPtr = WIDPtr;
        WIDPtr = WIDPtr->Next;
        continue;
      }
      // case 3) < || ==  0110->1110, <
      if( !RemainWriteMask ){
        // The first node of <, changed to new Write operation. 
        if( InsertNew ){
          WIDPtr->PC = (long) PC;
          WIDPtr->RWMask = NewWriteMask; // bug?
          PrevWIDPtr = WIDPtr;
          WIDPtr = PrevWIDPtr->Next;
          InsertNew = 0;
        }
        else{
          PrevWIDPtr->Next = WIDPtr->Next;
          free(WIDPtr);
          WIDPtr = PrevWIDPtr->Next;
        }
#ifdef _OVERHEAD_PROF
        //LSPresentBufFreeTime++;
#endif
        // Free LSPresentBuf. opt ?
        // // need free.
        NewWriteMask = NewWriteMask & DifWriteMask; 
      }
      // case 4) > 1110->1100 || 0110->0011 
      // Just update the pre-write mask first.
      // case 5) Part overlap, NewWriteMask && WriteMask.
      // 0110->0011 ===> 
      else if( RemainWriteMask ){
        WIDPtr->RWMask = RemainWriteMask;
        PrevWIDPtr = WIDPtr;
        WIDPtr = WIDPtr->Next;
        NewWriteMask = NewWriteMask & DifWriteMask; 
      }
      else{
        std::cout<<"updatePresentWTrace error \n";
      }
    }

    if ( InsertNew && NewWriteMask ){
      // 2) Get present WTrace buf.
      // 2.1) There are free WTrace bufs.
#ifdef _OVERHEAD_PROF
      //LSPresentTraceMallocTime++;
#endif
      WIDPtr = (LSPresentRWAddrID *)malloc(sizeof(LSPresentRWAddrID)); 
      WIDPtr->Next = PRWTraceRef.WTrace;  // Insert new buffer in the beginning. 
      PRWTraceRef.WTrace = WIDPtr;
#ifdef _DEBUGINFO
      //printf("Insert Write PresentTable Addr = %p \n",ShadowAddr );
#endif
      // 3) Fill the content.
      WIDPtr->PC = (long) PC;
      WIDPtr->RWMask = NewWriteMask;
    }

    return;
  }



  // 1) PresentRWTrace is setup while doing LIDA, and just keep FirstRead LastWrite
  // operations.
  // 2) Every iteration writes its RWTrace into its own Present[idx] buffer.
  // 3) The PresentRWTrace is released when doing the LCDA.
#if defined(_GAS_SHADOW_PARLIDA) 
  void doLIDAandUpdatePresentRTrace(void *PC, void **ShadowAddr, unsigned char ReadMask, int LoopIter, int TaskNum, int ThrID)
  {
    // 1) Get the right RWTrace Buf.
    Trace  &TraceRef = *((Trace*)(*ShadowAddr)); // hot
    // 1.1) First access the address, no buffer for Trace.
    // Not need?  Should have been allocated in ReadRWTraceFilter.
    // donothing;
    // *ShadowAddr == NULL;

    // 1.2) App has read/written the address before.  
    LSPresentRWTrace &PRWTraceRef = *(TraceRef.Present[LWPresNum]);
    // if PRWTraceRef.RWIter != 0, then all the previous PresentRWTrace has been
    // removed.
    // 1.2.1) Enter the next iteration.
    // To remove. 20141129.
    if(  PRWTraceRef.RWIter != LoopIter )  { 

      // Keep Loop Iteration RWTrace.
#ifdef _OPT1
      //PLIDAShdAddr[ThreadID][PLIDACurNum[ThreadID]] = (long)ShadowAddr;
      //PLIDACurNum[ThreadID]++;

#ifdef _PARLIDA
      AccAddrOfIter[TaskNum] = 1;
#endif

      // LIDAAddrInterval.insert( (long)ShadowAddr );
#endif

      PRWTraceRef.RWIter = LoopIter;
      // Not delete the precede iterations's PresentWTrace buffer. free?
      //PRWTraceRef.GlobalWriteMask =  0;
      //PRWTraceRef.GlobalReadMask =  0; 
    }
    // 1.2.2) Read/Write in the same iteration. 
    // do-nothing.

    // 2) Do LIDA and update PresentRTrace table. Opt ?
    if( PRWTraceRef.GlobalWriteMask )
      //doReadLIDA(PC, ReadMask, PRWTraceRef, TraceRef);
      doReadLIDAWithSinkPCAsKey( PC, ReadMask, PRWTraceRef, TraceRef, ThrID);

    // 3) Setup or Update PresentTrace table.
    updatePresentRTrace(PC, ShadowAddr, ReadMask, PRWTraceRef, 0);

    return;
  }
#endif

  void doLIDAandUpdatePresentRTrace(void *PC, void **ShadowAddr, unsigned char ReadMask, int LoopIter)
  {
    // 1) Get the right RWTrace Buf.
    Trace  &TraceRef = *((Trace*)(*ShadowAddr)); // hot

    // 1.1) First access the address, no buffer for Trace.
    // Not need?  Should have been allocated in ReadRWTraceFilter.
    // donothing;
    // *ShadowAddr == NULL;

    // 1.2) App has read/written the address before.  
    LSPresentRWTrace &PRWTraceRef = *(TraceRef.Present[LWPresNum]);
    // if PRWTraceRef.RWIter != 0, then all the previous PresentRWTrace has been
    // removed.
    // 1.2.1) Enter the next iteration.
    // To remove. 20141129.
    if(  PRWTraceRef.RWIter != LoopIter )  { 

      // Keep Loop Iteration RWTrace.
#ifdef _OPT1
      LIDAAddrInterval.insert( (long)ShadowAddr );
#endif

      PRWTraceRef.RWIter = LoopIter;
      // Not delete the precede iterations's PresentWTrace buffer. free?
      //PRWTraceRef.GlobalWriteMask =  0;
      //PRWTraceRef.GlobalReadMask =  0; 
    }
    // 1.2.2) Read/Write in the same iteration. 
    // do-nothing.

    // 2) Do LIDA and update PresentRTrace table. Opt ?
    if( PRWTraceRef.GlobalWriteMask )
      //doReadLIDA(PC, ReadMask, PRWTraceRef, TraceRef);
      doReadLIDAWithSinkPCAsKey( PC, ReadMask, PRWTraceRef, TraceRef);

    // 3) Setup or Update PresentTrace table.
    updatePresentRTrace(PC, ShadowAddr, ReadMask, PRWTraceRef, 0);

    return;
  }

  // Present[1] when serial execution; Present[IterationNO] when parallel 
  // execution. 
  // 
#if defined(_GAS_SHADOW_PARLIDA) 
  void doLIDAandUpdatePresentWTrace(void *PC, void **ShadowAddr, unsigned char  WriteMask, int LoopIter, int TaskNum, int ThrID )
  {
    // 1) Get the Present buf.
    Trace &TraceRef =  *((Trace*)(*ShadowAddr));

    // 1.1) First access the address, so create a new present/precede buffer.
    // done in ReadLIDA::operator.

    // 1.2) App has read/written the address before.
    // 1.2.1) The Present[] is 0.

    // 1.2.2) A new iteratrion.
    // To remove.
    if( TraceRef.Present[LWPresNum]->RWIter != LoopIter ) {
      // Keep Loop Iteration RWTrace.
#ifdef __OPT1
      //   LIDAAddrInterval.insert( (long)ShadowAddr );
      //PLIDAShdAddr[ThreadID][PLIDACurNum[ThreadID]] = (long)ShadowAddr;
      //PLIDACurNum[ThreadID]++;
      LAccShdAddr[LCurNum] = (long)ShadowAddr;
      LCurNum++;
#endif
      // Opt me?
      // Reset tags of Present-Read-Trace.
      //PRWTraceRef.RTrace.clear();
      TraceRef.Present[LWPresNum]->GlobalReadMask = 0;
      TraceRef.Present[LWPresNum]->GlobalWriteMask = 0;
      TraceRef.Present[LWPresNum]->RWIter = LoopIter;
    }
    // 1.2.2) Still in the same iteration.
    // do-nothing.

    // 4) There are maybe dependences.

    // Not write to the Address before within the current interation.
    LSPresentRWTrace &PRWTraceRef = *(TraceRef.Present[LWPresNum]);
    if( PRWTraceRef.GlobalReadMask )
      //doWriteLIDA(PC, WriteMask, PRWTraceRef, TraceRef);
      doWriteLIDAWithSinkPCAsKey( PC, WriteMask, PRWTraceRef, TraceRef, ThrID);

    updatePresentWTrace(PC, ShadowAddr, WriteMask, PRWTraceRef, 0);

    return;
  }
#endif
  void doLIDAandUpdatePresentWTrace(void *PC, void **ShadowAddr, unsigned char  WriteMask, int LoopIter )
  {
    // 1) Get the Present buf.
    Trace &TraceRef =  *((Trace*)(*ShadowAddr));

    // 1.1) First access the address, so create a new present/precede buffer.
    // done in ReadLIDA::operator.

    // 1.2) App has read/written the address before.
    // 1.2.1) The Present[] is 0.

    // 1.2.2) A new iteratrion.
    // To remove.
    if( TraceRef.Present[LWPresNum]->RWIter != LoopIter ) {
      // Keep Loop Iteration RWTrace.
#ifdef __OPT1
      LIDAAddrInterval.insert( (long)ShadowAddr );
#endif
      // Opt me?
      // Reset tags of Present-Read-Trace.
      //PRWTraceRef.RTrace.clear();
      TraceRef.Present[LWPresNum]->GlobalReadMask = 0;
      TraceRef.Present[LWPresNum]->GlobalWriteMask = 0;
      TraceRef.Present[LWPresNum]->RWIter = LoopIter;
    }
    // 1.2.2) Still in the same iteration.
    // do-nothing.


    // 4) There are maybe dependences.

    // Not write to the Address before within the current interation.
    LSPresentRWTrace &PRWTraceRef = *(TraceRef.Present[LWPresNum]);
    if( PRWTraceRef.GlobalReadMask )
      //doWriteLIDA(PC, WriteMask, PRWTraceRef, TraceRef);
      doWriteLIDAWithSinkPCAsKey( PC, WriteMask, PRWTraceRef, TraceRef);

    updatePresentWTrace(PC, ShadowAddr, WriteMask, PRWTraceRef, 0);

    return;
  }
  //
#ifdef _GAS_SHADOW_PARLIDA
  void check(int LoopIter, int ThreadID)
  {
    long ShdAddr;
    Trace*ShdTrcPtr;
    int Index,  Beg, End, AddrNum;
    RWTraceShdArrayEle* ShdSeq;
    int Num = 0, i, j;
    RWTraceShdArrayEle *ShdArray; //= &ShdArrayPool[LRStreamNum][0];
    RWTraceShdArrayEle *ShdArrayNode; 

    LCurNum = 0; // PLIDACurNum[ThreadID];
    LAccShdAddr = &PLIDAShdAddr[ThreadID][0];

    AddrNum = PLIDALength/LIDAThreadNum;
    Beg = AddrNum * ThreadID;
    End = Beg + AddrNum;
    if( ThreadID == (LIDAThreadNum-1) )
      End = PLIDALength;
    else
      End = Beg + AddrNum;
    if(!ThreadID) Beg = 1; // Beg == 0,
    for( j = Beg; j < End; j++){
      ShdAddr = PLIDAPtrShdAddr[j];
      ShdTrcPtr = *((Trace**)ShdAddr);
      int &CurNum = ShdTrcPtr->CurArrayNum[LRStreamNum];
      ShdSeq = &ShdTrcPtr->ShdArray[LRStreamNum][0]; 

      // Not DoLIAD before, so apply buffer for the Shadow.
      if( !ShdTrcPtr->Precede ){
        // Keep Loop Iteration RWTrace.
#ifdef __OPT1
        //LIDAAddrInterval.insert( ShdAddr );
        //PLIDAShdAddr[ThreadID][PLIDACurNum[ThreadID]] = ShdAddr;
        //PLIDACurNum[ThreadID]++;
        LAccShdAddr[LCurNum] = ShdAddr;
        LCurNum++;
#endif

        ShdTrcPtr->Precede = (LSPrecedeRWTrace*) malloc (sizeof(LSPrecedeRWTrace));
        ShdTrcPtr->Precede->RTrace = 0;
        ShdTrcPtr->Precede->WTrace = 0;
        ShdTrcPtr->Precede->GlobalReadMask = ShdTrcPtr->Precede->GlobalWriteMask = 0;
        for(int j = 0; j < PRESBUFNUM; j++){
          ShdTrcPtr->Present[j] = new LSPresentRWTrace;   
          ShdTrcPtr->Present[j]->WTrace = 0;
          ShdTrcPtr->Present[j]->RTrace = 0;
          ShdTrcPtr->Present[j]->GlobalWriteMask = 0;
          ShdTrcPtr->Present[j]->GlobalReadMask = 0;
          ShdTrcPtr->Present[j]->RWIter =  0; // bug?
        }
        ShdTrcPtr->Present[LWPresNum]->RWIter =  LoopIter; // bug?
      } // end if( ShdTrcPtr->Precede == NULL)


      // 1.2) Read RWTrace info.
      for(int k = 0; k < CurNum; k++  ){
        ShdArrayNode = &ShdSeq[k];
        // 2) Do LIDA and setup presentRWTrace in shadow memory.
        if( ShdArrayNode->RWMask[0] ){
          doLIDAandUpdatePresentRTrace( (void*) ShdArrayNode->PC, (void**)ShdAddr, ShdArrayNode->RWMask[0], LoopIter, 0, ThreadID);
        }
        else{
          doLIDAandUpdatePresentWTrace( (void*) ShdArrayNode->PC, (void**)ShdAddr, ShdArrayNode->RWMask[1], LoopIter, 0, ThreadID);
        }
      }

      CurNum = 0;
      free(ShdTrcPtr->ShdArray[LRStreamNum]);
      PLIDACurNum[ThreadID] = LCurNum;
    }

  }
#endif


  private:
  //std::interval_set<long> ReadAddrInterval;
  // Output;
  class RWTrace* next_rwtrace;
  /*override*/ 
  void* operator()(void*); // why private?
  inline void doReadLIDA(void * PC, unsigned char ReadMask, LSPresentRWTrace &PRWTraceRef, Trace &TraceRef);
#if defined(_GAS_SHADOW_PARLIDA) || defined(_PARLIDA) || defined(_PAROMP)
  inline void doWriteLIDAWithSinkPCAsKey(void * PC, unsigned char WriteMask, LSPresentRWTrace &PRWTraceRef, Trace &TraceRef, int ThreadID);
#endif
#ifdef _DDA_PP
  //inline void doWriteLIDAWithSinkPCAsKey(void * PC, unsigned char WriteMask, LSPresentRWTrace &PRWTraceRef, Trace &TraceRef, int ThreadID);
#endif
  inline void doWriteLIDAWithSinkPCAsKey(void * PC, unsigned char WriteMask, LSPresentRWTrace &PRWTraceRef, Trace &TraceRef);
  inline void doWriteLIDA(void * PC, unsigned char WriteMask,LSPresentRWTrace &PRWTraceRef, Trace &TraceRef);

  private:

  class LICDep *next_lidep;
  interval_set<long> LIDAAddrInterval; // For what?
  long LRStreamNum;
  // For LCDA, Thread local? Cross multi-task.
  long LIDAStackMin, LIDAStackMax, LIDAHeapMax, LIDAHeapMin, LIDAGlobalMin, LIDAGlobalMax;

  // Many Deps happens repeatly, so we keep a memo of these deps.
  // when reset?
  std::map<long,std::set<long> > HLITDep;
  std::map<long,std::set<long> > HLIADep;
  std::map<long,std::set<long> > HLIODep;

#if defined( _GAS_SHADOW_PARLIDA ) 
  std::map<long,std::set<long> > *ThrLITDep;
  std::map<long,std::set<long> > *ThrLIADep;
  std::map<long,std::set<long> > *ThrLIODep;
  int PLIDACurNum[LIDAThreadNum];
  int PLIDALength;
  long *PLIDAShdAddr[LIDAThreadNum];
  long *PLIDAPtrShdAddr; // ShdAddrStream generated by GenerateAddrStream. 
#endif
};

ReadLIDAFilter::ReadLIDAFilter( ):
  filter(serial_in_order)
  //next_rwtrace( RWTraceDep::allocate(0) )
{ 

  LRStreamNum =  RStreamNum;

  //std::cout<<"ReadLIDAFilter \n";
}

ReadLIDAFilter::~ReadLIDAFilter() 
{
  //next_rwtrace->free(); // not need, can be done by the follow filter.

}


/* Look for Loop independent output dependences. 
// Todo:
// 1) Addr1: W R R R R ===> not need to check True-dep every time.
// LIOD & LIAD.
// 2) The results are stored in the ShadowAddr(SinkPC).
// 3) Dist[0]->LIDep, Dist[1]->LCDep;
*/ 
#if defined(_GAS_SHADOW_PARLIDA) 
inline void  ReadLIDAFilter::doWriteLIDAWithSinkPCAsKey(void * PC, unsigned char WriteMask, LSPresentRWTrace &PRWTraceRef, Trace &TraceRef, int ThrID)
{
  unsigned char RWMask;
  unsigned char NewRWMask;

  if( (RWMask = ( WriteMask & PRWTraceRef.GlobalWriteMask)) ) {
    LSPresentRWAddrID *WIDPtr = PRWTraceRef.WTrace;
    std::map<long, std::set<long> > &LIODep = ThrLIODep[ThrID]; // bug?
    //std::map<long, std::set<long> > &LIODep = next_lidep->LIODep; // bug?
    std::map<long, std::set<long> >::iterator it;

    for( ; (WIDPtr != 0 ) && RWMask; WIDPtr = WIDPtr->Next){
      NewRWMask = RWMask;
      RWMask &= RWMask ^ WIDPtr->RWMask;
      // LI-Output.
      // WIDPtr->PC ===> (long)PC
      if( RWMask != NewRWMask ){
#ifdef _DEBUGINFO_LIDA1
        //std::cout<<"doWriteLIDWithSinkPCAsKeyLICDepPA: LIODep SinkPC = " << PC << "\n";
#endif
        long SinkPC = (long) PC;
        long SrcPC = WIDPtr->PC;
        bool inH = 0;
#if 1
        LSDepInfo **DepShadowPtr;
        //LSDepInfo *LIDepPtr;
        DepShadowPtr = (LSDepInfo**) AppAddrToShadow(SinkPC);
        if( *DepShadowPtr != 0 ){
          //LIDepPtr = *DepShadowPtr;
          LSDep *LIDepOut = (*DepShadowPtr)->OutDep; //TraceRef.LIDep->TrueDep; 
          while( LIDepOut != 0 ){
            if(LIDepOut->SrcPC == SrcPC){
              inH = 1;
              break;
            }
            LIDepOut = LIDepOut->next;
          }
        }
#endif
        if( !inH )
          LIODep[SinkPC].insert(SrcPC);
#ifdef _DEBUGINFO
        std::cout<<"doWriteLIDWithSinkPCAsKey: LIODep SinkPC = " << PC << ", SrcPC = " << WIDPtr->PC << "\n";
#endif

      }
    } 
  }


  // LIAD.
  if( (RWMask = ( WriteMask & PRWTraceRef.GlobalReadMask)) ) {

    //std::map<long, std::set<long> > &LIADep = next_lidep->LIADep;
    std::map<long, std::set<long> > &LIADep = ThrLIADep[ThrID];
    //std::map<long, std::set<long> >::iterator it;

    LSPresentRWAddrID *RIDPtr = PRWTraceRef.RTrace;
    for( ; (RIDPtr != 0) && RWMask; RIDPtr = RIDPtr->Next){
      NewRWMask = RWMask;
      RWMask &= RWMask ^ RIDPtr->RWMask;

      // LI-AntiDep.
      // RIDPtr->PC ===>  (long)PC
      if( RWMask != NewRWMask ){
#ifdef _DEBUGINFO1
        //std::cout<<"doWriteLIDWithSinkPCAsKeyLICDepPA: LIADep SinkPC = " << PC << "\n";
#endif
        long SinkPC = (long)PC;
        long SrcPC = RIDPtr->PC;
        bool inH = 0;
#if 1
        LSDepInfo **DepShadowPtr;
        //LSDepInfo *LIDepPtr;
        DepShadowPtr = (LSDepInfo**) AppAddrToShadow(SinkPC);
        if( *DepShadowPtr != 0 ){
          //LIDepPtr = *DepShadowPtr;
          LSDep *LIDepAnti = (*DepShadowPtr)->AntiDep; //TraceRef.LIDep->TrueDep; 
          while( LIDepAnti != 0 ){
            if(LIDepAnti->SrcPC == SrcPC ){
              inH = 1;
              break;
            }
            LIDepAnti = LIDepAnti->next;
          }
        }
#endif
        if(!inH){
          LIADep[SinkPC].insert(SrcPC);
        }
#ifdef _DEBUGINFO
        std::cout<<"doWriteLIDWithSinkPCAsKey: LIADep SinkPC = " << PC << ", SrcPC = " << RIDPtr->PC <<   "\n";
#endif

      }
    } 

  }

  return;
}
#endif

  inline void 
ReadLIDAFilter::doWriteLIDAWithSinkPCAsKey(void * PC, unsigned char WriteMask, LSPresentRWTrace &PRWTraceRef, Trace &TraceRef)
{
  unsigned char RWMask;
  unsigned char NewRWMask;

  if( (RWMask = ( WriteMask & PRWTraceRef.GlobalWriteMask)) ) {
    LSPresentRWAddrID *WIDPtr = PRWTraceRef.WTrace;
#if 1
    //std::map<long, std::set<long> > &LIODep = ThrLIODep[ThreadID]; // bug?
    std::map<long, std::set<long> > &LIODep = next_lidep->LIODep; // bug?
    std::map<long, std::set<long> >::iterator it;
#endif
    for( ; (WIDPtr != 0 ) && RWMask; WIDPtr = WIDPtr->Next){
      NewRWMask = RWMask;
      RWMask &= RWMask ^ WIDPtr->RWMask;
      // LI-Output.
      // WIDPtr->PC ===> (long)PC
      if( RWMask != NewRWMask ){
#ifdef _DEBUGINFO_LIDA1
        //std::cout<<"doWriteLIDWithSinkPCAsKeyLICDepPA: LIODep SinkPC = " << PC << "\n";
#endif
        long SinkPC = (long) PC;
        long SrcPC = WIDPtr->PC;
        bool inH = 0;
#if 1
        LSDepInfo **DepShadowPtr;
        //LSDepInfo *LIDepPtr;
        DepShadowPtr = (LSDepInfo**) AppAddrToShadow(SinkPC);
        if( *DepShadowPtr != 0 ){
          //LIDepPtr = *DepShadowPtr;
          LSDep *LIDepOut = (*DepShadowPtr)->OutDep; //TraceRef.LIDep->TrueDep; 
          while( LIDepOut != 0 ){
            if(LIDepOut->SrcPC == SrcPC){
              inH = 1;
              break;
            }
            LIDepOut = LIDepOut->next;
          }
        }
#endif
        if( !inH )
          LIODep[SinkPC].insert(SrcPC);
#ifdef _DEBUGINFO
        std::cout<<"doWriteLIDWithSinkPCAsKey: LIODep SinkPC = " << PC << ", SrcPC = " << WIDPtr->PC << "\n";
#endif

      }
    } 
  }


  // LIAD.
  if( (RWMask = ( WriteMask & PRWTraceRef.GlobalReadMask)) ) {

#if 1
    std::map<long, std::set<long> > &LIADep = next_lidep->LIADep;
    //std::map<long, std::set<long> > &LIADep = ThrLIADep[TaskId];
    //std::map<long, std::set<long> >::iterator it;
#endif

    LSPresentRWAddrID *RIDPtr = PRWTraceRef.RTrace;
    for( ; (RIDPtr != 0) && RWMask; RIDPtr = RIDPtr->Next){
      NewRWMask = RWMask;
      RWMask &= RWMask ^ RIDPtr->RWMask;

      // LI-AntiDep.
      // RIDPtr->PC ===>  (long)PC
      if( RWMask != NewRWMask ){
#ifdef _DEBUGINFO1
        //std::cout<<"doWriteLIDWithSinkPCAsKeyLICDepPA: LIADep SinkPC = " << PC << "\n";
#endif
        long SinkPC = (long)PC;
        long SrcPC = RIDPtr->PC;
        bool inH = 0;
#if 1
        LSDepInfo **DepShadowPtr;
        //LSDepInfo *LIDepPtr;
        DepShadowPtr = (LSDepInfo**) AppAddrToShadow(SinkPC);
        if( *DepShadowPtr != 0 ){
          //LIDepPtr = *DepShadowPtr;
          LSDep *LIDepAnti = (*DepShadowPtr)->AntiDep; //TraceRef.LIDep->TrueDep; 
          while( LIDepAnti != 0 ){
            if(LIDepAnti->SrcPC == SrcPC ){
              inH = 1;
              break;
            }
            LIDepAnti = LIDepAnti->next;
          }
        }
#endif
        if(!inH){
          LIADep[SinkPC].insert(SrcPC);
        }
#ifdef _DEBUGINFO
        std::cout<<"doWriteLIDWithSinkPCAsKey: LIADep SinkPC = " << PC << ", SrcPC = " << RIDPtr->PC <<   "\n";
#endif

      }
    } 

  }

  return;
}

  inline
void ReadLIDAFilter::doWriteLIDA(void * PC, unsigned char WriteMask, 
    LSPresentRWTrace &PRWTraceRef, Trace &TraceRef )
{


  // 1) Take as the key the Write-Address to store data dependence results.
#ifdef __ADDRKEY
  __doWriteLIDWithAddrAsKey(PC, WriteMask, PRWTraceRef, TraceRef);
#endif
  // 2) Take as the key the SrcPC of dependence pair to store data dependence
  // results.
  //__doWriteLIDWithSinkPCAsKey( PC, WriteMask, PRWTraceRef, TraceRef);

  // 3)
  doWriteLIDAWithSinkPCAsKey( PC, WriteMask, PRWTraceRef, TraceRef);

  return;
}






// Todo:
// 1) Addr1: W R R R R ===> not need to check True-dep every time.
// LITD.
inline void ReadLIDAFilter::doReadLIDA(void * PC, unsigned char ReadMask,
    LSPresentRWTrace &PRWTraceRef, Trace &TraceRef)
{


  // 3) LI/LC Store together. 
  //doReadLIDAWithSinkPCAsKey( PC, ReadMask, PRWTraceRef, TraceRef);

  return;
}
#endif  // _DDA_GAS_SHADOW



#ifdef _DDA_DEFAULT
class ReadLIDAFilter: public tbb::filter{
#ifdef _DDA_DEFAULT_PP_PARLIDA
  struct ParReadLIDA{
    class ReadLIDAFilter *LIDAPtr;
    void operator()(const blocked_range<int>& range) const{
      for(int i = range.begin(); i != range.end(); ++i){
        LIDAPtr->parreadlida(i);
      }
    }
  };

  struct ParLIDA{
    unsigned int LoopIter;
    int Length;
    int ParLIDACurProc;
    class ReadLIDAFilter *LIDAPtr;
    AddrStreamItem *ParLIDAPtrItems;
    void operator()(const blocked_range<int>& range) const{
      for(int i = range.begin(); i != range.end(); ++i){
        LIDAPtr->parcheck(LoopIter, Length, i,ParLIDAPtrItems, ParLIDACurProc);
      }
    }
  };
#endif

  public:
  ReadLIDAFilter();
  ~ReadLIDAFilter();

#if defined(_DDA_DEFAULT_PP_PARLIDA) 
  void doReadLIDAWithSinkPCAsKey(void * PC, unsigned char ReadMask, LSPresentRWTrace &PRWTraceRef, Trace &TraceRef, int ThreadID, int CurProc)
  {
    unsigned char RWMask;

    // 1) check for LIDep.
    if( (RWMask = (ReadMask & PRWTraceRef.GlobalWriteMask)) ) {
      LSPresentRWAddrID * WIDPtr = PRWTraceRef.WTrace;
      unsigned char NewRWMask;
      std::map<long, std::set<long> > &LITDep = ProcThrLITDep[CurProc][ThreadID];
      //std::map<long, std::set<long> > &LITDep = next_lidep->LITDep;

      //for( ; RWMask && (WIDPtr != 0) ; WIDPtr = WIDPtr->Next )
      for( ; WIDPtr != 0 ; WIDPtr = WIDPtr->Next ){
        NewRWMask = RWMask & WIDPtr->RWMask;
        //NewRWMask = RWMask;
        //RWMask &= RWMask ^ WIDPtr->RWMask;

        // LI True Dep?
        // WIDPtr->PC ==> (long)PC
        if( NewRWMask ){
          bool inH = 0;
#ifdef _OPT2
          LSDepInfo **DepShadowPtr;
          //LSDepInfo *LIDepPtr;
          DepShadowPtr = (LSDepInfo**) AppAddrToShadow((long)PC);
          if( *DepShadowPtr != 0 ){
            //LIDepPtr = *DepShadowPtr;
            LSDep *LIDepTrue = (*DepShadowPtr)->TrueDep; //TraceRef.LIDep->TrueDep; 
            while( LIDepTrue != 0 ){
              if(LIDepTrue->SrcPC == WIDPtr->PC){
                inH = 1;
                break;
              }
              LIDepTrue = LIDepTrue->next;
            }
          }
#endif
          if(!inH){
            LITDep[(long)PC].insert(WIDPtr->PC);
          }
#ifdef _DEBUGINFO
          std::cout<<"doReadLIDWithSinkPCAsKey: LITDep SinkPC = " << PC << ", SrcPC = " << WIDPtr->PC << "\n";
#endif
        }
      }
    }

    return ;
  }




  // 20140127.
  // 1) LIDep/LCDep store in the same struct with different Dist;
  // Dist[0] = 0, keep the LID Dist whose value is 0.
  // 2) LIDep: take SinkPC as key.
  // 3) Pipeline Parallel analysis.
  // Anti-LIDep info.
#elif defined(_DDA_DEFAULT_PARLIDA) 
  void doReadLIDAWithSinkPCAsKey(void * PC, unsigned char ReadMask, LSPresentRWTrace &PRWTraceRef, Trace &TraceRef, int ThreadID)
  {
    unsigned char RWMask;

    // 1) check for LIDep.
    if( (RWMask = (ReadMask & PRWTraceRef.GlobalWriteMask)) ) {
      LSPresentRWAddrID * WIDPtr = PRWTraceRef.WTrace;
      unsigned char NewRWMask;
      std::map<long, std::set<long> > &LITDep = ThrLITDep[ThreadID];
      //std::map<long, std::set<long> > &LITDep = next_lidep->LITDep;

      //for( ; RWMask && (WIDPtr != 0) ; WIDPtr = WIDPtr->Next )
      for( ; WIDPtr != 0 ; WIDPtr = WIDPtr->Next ){
        NewRWMask = RWMask & WIDPtr->RWMask;
        //NewRWMask = RWMask;
        //RWMask &= RWMask ^ WIDPtr->RWMask;

        // LI True Dep?
        // WIDPtr->PC ==> (long)PC
        if( NewRWMask ){
          bool inH = 0;
#ifdef _OPT2
          LSDepInfo **DepShadowPtr;
          //LSDepInfo *LIDepPtr;
          DepShadowPtr = (LSDepInfo**) AppAddrToShadow((long)PC);
          if( *DepShadowPtr != 0 ){
            //LIDepPtr = *DepShadowPtr;
            LSDep *LIDepTrue = (*DepShadowPtr)->TrueDep; //TraceRef.LIDep->TrueDep; 
            while( LIDepTrue != 0 ){
              if(LIDepTrue->SrcPC == WIDPtr->PC){
                inH = 1;
                break;
              }
              LIDepTrue = LIDepTrue->next;
            }
          }
#endif
          if(!inH){
            LITDep[(long)PC].insert(WIDPtr->PC);
          }
#ifdef _DEBUGINFO
          std::cout<<"doReadLIDWithSinkPCAsKey: LITDep SinkPC = " << PC << ", SrcPC = " << WIDPtr->PC << "\n";
#endif
        }
      }
    }

    return ;
  }

#else

  void doReadLIDAWithSinkPCAsKey(void * PC, unsigned char ReadMask, LSPresentRWTrace &PRWTraceRef, Trace &TraceRef)
  {
    unsigned char RWMask;

    // 1) check for LIDep.
    if( (RWMask = (ReadMask & PRWTraceRef.GlobalWriteMask)) ) {
      LSPresentRWAddrID * WIDPtr = PRWTraceRef.WTrace;
      unsigned char NewRWMask;
      //std::map<long, std::set<long> > &LITDep = ThrLITDep[ThreadID];
      std::map<long, std::set<long> > &LITDep = next_lidep->LITDep;

      //for( ; RWMask && (WIDPtr != 0) ; WIDPtr = WIDPtr->Next )
      for( ; WIDPtr != 0 ; WIDPtr = WIDPtr->Next ){
        NewRWMask = RWMask & WIDPtr->RWMask;
        //NewRWMask = RWMask;
        //RWMask &= RWMask ^ WIDPtr->RWMask;

        // LI True Dep?
        // WIDPtr->PC ==> (long)PC
        if( NewRWMask ){
          bool inH = 0;
#ifdef _OPT2
          LSDepInfo **DepShadowPtr;
          //LSDepInfo *LIDepPtr;
          DepShadowPtr = (LSDepInfo**) AppAddrToShadow((long)PC);
          if( *DepShadowPtr != 0 ){
            //LIDepPtr = *DepShadowPtr;
            LSDep *LIDepTrue = (*DepShadowPtr)->TrueDep; //TraceRef.LIDep->TrueDep; 
            while( LIDepTrue != 0 ){
              if(LIDepTrue->SrcPC == WIDPtr->PC){
                inH = 1;
                break;
              }
              LIDepTrue = LIDepTrue->next;
            }
          }
#endif
          if(!inH){
            LITDep[(long)PC].insert(WIDPtr->PC);
          }
#ifdef _DEBUGINFO
          std::cout<<"doReadLIDWithSinkPCAsKey: LITDep SinkPC = " << PC << ", SrcPC = " << WIDPtr->PC << "\n";
#endif
        }
      }
    }

    return ;
  }
#endif

  void updatePresentRTrace(void *PC, void **ShadowAddr, unsigned char ReadMask,
      LSPresentRWTrace &PRWTraceRef, int WTEmptyFlag )
  {

    // printf("insert load addr = %p pc = %p\n",ShadowAddr, (void*) PC );
    unsigned char NewReadMask;
    LSPresentRWAddrID *RIDPtr;

    // No Write before within cur iteration.
    unsigned char FirstNewReadMask;
    unsigned char OldGlobalReadMask = PRWTraceRef.GlobalReadMask; 
    unsigned char NewGlobalReadMask = OldGlobalReadMask | ReadMask;

    // 1) Having New-Read bytes.
    if(  (NewReadMask = (NewGlobalReadMask ^ OldGlobalReadMask)) ) {
      // 1.1) The New-Read bytes have not been written before.
      if( FirstNewReadMask = (NewReadMask & (NewReadMask ^ PRWTraceRef.GlobalWriteMask)) ){

        // 1.2) Get the RWTrace buf to insert the New-Read-Not-Written bytes. 
        // 1.2.1) There are free RWTrace buf. NewBuf->next = RTrace; RTrace = NewBuf;
#ifdef _OVERHEAD_PROF
        LSPresentTraceMallocTime++;
#endif
        RIDPtr = (LSPresentRWAddrID*) malloc (sizeof(LSPresentRWAddrID)); // 
        RIDPtr->Next = PRWTraceRef.RTrace;
        PRWTraceRef.RTrace = RIDPtr;

#ifdef _DEBUGINFO
        //printf("Insert Read PresentTable Addr = %p \n",ShadowAddr );
#endif
        // 4) Fill content.
        RIDPtr->PC = (long)PC;
        RIDPtr->RWMask = FirstNewReadMask;
        PRWTraceRef.GlobalReadMask |= FirstNewReadMask;  // 
      }
    }

    return;
  }

  // PresentWTrace: Last Write.
  // 1) The write operation is last, so it must be inserted here.
  // 2) Addr->(PC, RWMask), every item maintains 
  void updatePresentWTrace(void *PC, void **ShadowAddr, unsigned char WriteMask,
      LSPresentRWTrace &PRWTraceRef, int WTEmptyFlag )
  {
    unsigned char DifWriteMask, NewWriteMask, RemainWriteMask;
    unsigned char OldGlobalWriteMask = PRWTraceRef.GlobalWriteMask; 
    unsigned char NewGlobalWriteMask = PRWTraceRef.GlobalWriteMask = OldGlobalWriteMask | WriteMask;
    LSPresentRWAddrID *WIDPtr, *PrevWIDPtr;
    //LSPresentRWAddrID *CurBuf;

    // 1) Firt time to write whole WriteMask....
    // May miss some deps. (different pc have the same WriteMask.
    if( WriteMask == (OldGlobalWriteMask ^ NewGlobalWriteMask) ){
      // 2 Opt me? Need merged with other ReadMask?
      // 2.1) buffer reuse.
#ifdef _OVERHEAD_PROF
      //LSPresentTraceMallocTime++;
#endif
      WIDPtr = (LSPresentRWAddrID*) malloc(sizeof(LSPresentRWAddrID));
      WIDPtr->Next = PRWTraceRef.WTrace;
      PRWTraceRef.WTrace = WIDPtr;

#ifdef _DEBUGINFO
      //printf("Insert Write PresentTable Addr = %p \n",ShadowAddr );
#endif
      WIDPtr->PC = (long)PC;
      WIDPtr->RWMask = WriteMask;   
      return;
    }

    // 2) Check overlap with all preceding write.
    int InsertNew = 1; // label whether has insert the NewWrite info.
    // 1100->1111: del preceding write;
    // 1100->0110: del preced and add (1000, prev-pc), (0110, cur-pc);
    WIDPtr = PRWTraceRef.WTrace;

    // case 1): ==  killer,CurRWMask == NewRWMask.
    if( WriteMask == WIDPtr->RWMask){
      WIDPtr->PC = (long) PC;
      return;
    }

    PrevWIDPtr = WIDPtr;
    NewWriteMask = WriteMask;
    for( ; WIDPtr != 0 && NewWriteMask; ){
      DifWriteMask = WIDPtr->RWMask ^ NewWriteMask;
      RemainWriteMask =  WIDPtr->RWMask & DifWriteMask; // WIDPtr Write-bytes before.
      //NewWriteMask = NewWriteMask & DifWriteMask; // New Write-Mask not overlap. fix me?
      //NewWriteMask = NewWriteMask & DifWriteMask; 

      // case 2) No overlap. No overlap: 1100->0011
      if( RemainWriteMask == WIDPtr->RWMask ){
        PrevWIDPtr = WIDPtr;
        WIDPtr = WIDPtr->Next;
        continue;
      }
      // case 3) < || ==  0110->1110, <
      if( !RemainWriteMask ){
        // The first node of <, changed to new Write operation. 
        if( InsertNew ){
          WIDPtr->PC = (long) PC;
          WIDPtr->RWMask = NewWriteMask; // bug?
          PrevWIDPtr = WIDPtr;
          WIDPtr = PrevWIDPtr->Next;
          InsertNew = 0;
        }
        else{
          PrevWIDPtr->Next = WIDPtr->Next;
          free(WIDPtr);
          WIDPtr = PrevWIDPtr->Next;
        }
#ifdef _OVERHEAD_PROF
        //LSPresentBufFreeTime++;
#endif
        // Free LSPresentBuf. opt ?
        // // need free.
        NewWriteMask = NewWriteMask & DifWriteMask; 
      }
      // case 4) > 1110->1100 || 0110->0011 
      // Just update the pre-write mask first.
      // case 5) Part overlap, NewWriteMask && WriteMask.
      // 0110->0011 ===> 
      else if( RemainWriteMask ){
        WIDPtr->RWMask = RemainWriteMask;
        PrevWIDPtr = WIDPtr;
        WIDPtr = WIDPtr->Next;
        NewWriteMask = NewWriteMask & DifWriteMask; 
      }
      else{
        std::cout<<"updatePresentWTrace error \n";
      }
    }

    if ( InsertNew && NewWriteMask ){
      // 2) Get present WTrace buf.
      // 2.1) There are free WTrace bufs.
#ifdef _OVERHEAD_PROF
      //LSPresentTraceMallocTime++;
#endif
      WIDPtr = (LSPresentRWAddrID *)malloc(sizeof(LSPresentRWAddrID)); 
      WIDPtr->Next = PRWTraceRef.WTrace;  // Insert new buffer in the beginning. 
      PRWTraceRef.WTrace = WIDPtr;
#ifdef _DEBUGINFO
      //printf("Insert Write PresentTable Addr = %p \n",ShadowAddr );
#endif
      // 3) Fill the content.
      WIDPtr->PC = (long) PC;
      WIDPtr->RWMask = NewWriteMask;
    }

    return;
  }

#if defined(_DDA_DEFAULT_PP_PARLIDA) 
  void doLIDAandUpdatePresentRTrace(void *PC, void **ShadowAddr, unsigned char ReadMask, int LoopIter, int TaskNum, int ThrID, int CurProc)
  {
    // 1) Get the right RWTrace Buf.
    Trace  &TraceRef = *((Trace*)(*ShadowAddr)); // hot
    // 1.1) First access the address, no buffer for Trace.
    // Not need?	Should have been allocated in ReadRWTraceFilter.
    // donothing;
    // *ShadowAddr == NULL;

    // 1.2) App has read/written the address before.	
    LSPresentRWTrace &PRWTraceRef = *(TraceRef.Present[ProcWPresNum[CurProc]]);
    // if PRWTraceRef.RWIter != 0, then all the previous PresentRWTrace has been
    // removed.
    // 1.2.1) Enter the next iteration.
    // To remove. 20141129.
    if(  PRWTraceRef.RWIter != LoopIter )  { 

      // Keep Loop Iteration RWTrace.
#ifdef _OPT1
      ProcAccAddrOfIter[CurProc][TaskNum] = 1;
#endif

      PRWTraceRef.RWIter = LoopIter;
      // Not delete the precede iterations's PresentWTrace buffer. free?
      //PRWTraceRef.GlobalWriteMask =  0;
      //PRWTraceRef.GlobalReadMask =	0; 
    }
    // 1.2.2) Read/Write in the same iteration. 
    // do-nothing.

    // 2) Do LIDA and update PresentRTrace table. Opt ?
    if( PRWTraceRef.GlobalWriteMask )
      //doReadLIDA(PC, ReadMask, PRWTraceRef, TraceRef);
      doReadLIDAWithSinkPCAsKey( PC, ReadMask, PRWTraceRef, TraceRef, ThrID, CurProc);

    // 3) Setup or Update PresentTrace table.
    updatePresentRTrace(PC, ShadowAddr, ReadMask, PRWTraceRef, 0);

    return;
  }


  // 1) PresentRWTrace is setup while doing LIDA, and just keep FirstRead LastWrite
  // operations.
  // 2) Every iteration writes its RWTrace into its own Present[idx] buffer.
  // 3) The PresentRWTrace is released when doing the LCDA.
#elif defined(_DDA_DEFAULT_PP_PARLIDA) 
  void doLIDAandUpdatePresentRTrace(void *PC, void **ShadowAddr, unsigned char ReadMask, int LoopIter, int TaskNum, int ThrID)
  {
    // 1) Get the right RWTrace Buf.
    Trace  &TraceRef = *((Trace*)(*ShadowAddr)); // hot
    // 1.1) First access the address, no buffer for Trace.
    // Not need?  Should have been allocated in ReadRWTraceFilter.
    // donothing;
    // *ShadowAddr == NULL;

    // 1.2) App has read/written the address before.  
    LSPresentRWTrace &PRWTraceRef = *(TraceRef.Present[LWPresNum]);
    // if PRWTraceRef.RWIter != 0, then all the previous PresentRWTrace has been
    // removed.
    // 1.2.1) Enter the next iteration.
    // To remove. 20141129.
    if(  PRWTraceRef.RWIter != LoopIter )  { 

      // Keep Loop Iteration RWTrace.
#ifdef _OPT1
      //PLIDAShdAddr[ThreadID][PLIDACurNum[ThreadID]] = (long)ShadowAddr;
      //PLIDACurNum[ThreadID]++;

#ifdef _PARLIDA
      AccAddrOfIter[TaskNum] = 1;
#endif

      // LIDAAddrInterval.insert( (long)ShadowAddr );
#endif

      PRWTraceRef.RWIter = LoopIter;
      // Not delete the precede iterations's PresentWTrace buffer. free?
      //PRWTraceRef.GlobalWriteMask =  0;
      //PRWTraceRef.GlobalReadMask =  0; 
    }
    // 1.2.2) Read/Write in the same iteration. 
    // do-nothing.

    // 2) Do LIDA and update PresentRTrace table. Opt ?
    if( PRWTraceRef.GlobalWriteMask )
      //doReadLIDA(PC, ReadMask, PRWTraceRef, TraceRef);
      doReadLIDAWithSinkPCAsKey( PC, ReadMask, PRWTraceRef, TraceRef, ThrID);

    // 3) Setup or Update PresentTrace table.
    updatePresentRTrace(PC, ShadowAddr, ReadMask, PRWTraceRef, 0);

    return;
  }
#else

  void doLIDAandUpdatePresentRTrace(void *PC, void **ShadowAddr, unsigned char ReadMask, int LoopIter)
  {
    // 1) Get the right RWTrace Buf.
    Trace  &TraceRef = *((Trace*)(*ShadowAddr)); // hot

    // 1.1) First access the address, no buffer for Trace.
    // Not need?  Should have been allocated in ReadRWTraceFilter.
    // donothing;
    // *ShadowAddr == NULL;

    // 1.2) App has read/written the address before.  
    LSPresentRWTrace &PRWTraceRef = *(TraceRef.Present[LWPresNum]);
    // if PRWTraceRef.RWIter != 0, then all the previous PresentRWTrace has been
    // removed.
    // 1.2.1) Enter the next iteration.
    // To remove. 20141129.
    if(  PRWTraceRef.RWIter != LoopIter )  { 

      // Keep Loop Iteration RWTrace.
#ifdef _OPT1
      LIDAAddrInterval.insert( (long)ShadowAddr );
#endif

      PRWTraceRef.RWIter = LoopIter;
      // Not delete the precede iterations's PresentWTrace buffer. free?
      //PRWTraceRef.GlobalWriteMask =  0;
      //PRWTraceRef.GlobalReadMask =  0; 
    }
    // 1.2.2) Read/Write in the same iteration. 
    // do-nothing.

    // 2) Do LIDA and update PresentRTrace table. Opt ?
    if( PRWTraceRef.GlobalWriteMask )
      //doReadLIDA(PC, ReadMask, PRWTraceRef, TraceRef);
      doReadLIDAWithSinkPCAsKey( PC, ReadMask, PRWTraceRef, TraceRef);

    // 3) Setup or Update PresentTrace table.
    updatePresentRTrace(PC, ShadowAddr, ReadMask, PRWTraceRef, 0);

    return;
  }
#endif

  // Present[1] when serial execution; Present[IterationNO] when parallel 
  // execution. 
  // 
#if defined(_DDA_DEFAULT_PP_PARLIDA )
  void doLIDAandUpdatePresentWTrace(void *PC, void **ShadowAddr, unsigned char  WriteMask, int LoopIter, int TaskNum, int ThrID, int CurProc )
  {
    // 1) Get the Present buf.
    Trace &TraceRef =  *((Trace*)(*ShadowAddr));

    // 1.1) First access the address, so create a new present/precede buffer.
    // done in ReadLIDA::operator.

    // 1.2) App has read/written the address before.
    // 1.2.1) The Present[] is 0.

    // 1.2.2) A new iteratrion.
    // To remove.
    if( TraceRef.Present[ProcWPresNum[CurProc]]->RWIter != LoopIter ) {
      // Keep Loop Iteration RWTrace.
#ifdef __OPT1
      ProcAccAddrOfIter[CurProc][TaskNum] = 1;
#endif
      // Opt me?
      // Reset tags of Present-Read-Trace.
      //PRWTraceRef.RTrace.clear();
      TraceRef.Present[ProcWPresNum[CurProc]]->GlobalReadMask = 0;
      TraceRef.Present[ProcWPresNum[CurProc]]->GlobalWriteMask = 0;
      TraceRef.Present[ProcWPresNum[CurProc]]->RWIter = LoopIter;
    }
    // 1.2.2) Still in the same iteration.
    // do-nothing.

    // 4) There are maybe dependences.

    // Not write to the Address before within the current interation.
    LSPresentRWTrace &PRWTraceRef = *(TraceRef.Present[ProcWPresNum[CurProc]]);
    if( PRWTraceRef.GlobalReadMask )
      doWriteLIDAWithSinkPCAsKey( PC, WriteMask, PRWTraceRef, TraceRef, ThrID, CurProc);

    updatePresentWTrace(PC, ShadowAddr, WriteMask, PRWTraceRef, 0);

    return;
  }

  // Present[1] when serial execution; Present[IterationNO] when parallel 
  // execution. 
  // 
#elif defined(_DDA_DEFAULT_PARLIDA) 
  void doLIDAandUpdatePresentWTrace(void *PC, void **ShadowAddr, unsigned char  WriteMask, int LoopIter, int TaskNum, int ThrID )
  {
    // 1) Get the Present buf.
    Trace &TraceRef =  *((Trace*)(*ShadowAddr));

    // 1.1) First access the address, so create a new present/precede buffer.
    // done in ReadLIDA::operator.

    // 1.2) App has read/written the address before.
    // 1.2.1) The Present[] is 0.

    // 1.2.2) A new iteratrion.
    // To remove.
    if( TraceRef.Present[LWPresNum]->RWIter != LoopIter ) {
      // Keep Loop Iteration RWTrace.
#ifdef __OPT1
      AccAddrOfIter[TaskNum] = 1;
#endif
      // Opt me?
      // Reset tags of Present-Read-Trace.
      //PRWTraceRef.RTrace.clear();
      TraceRef.Present[LWPresNum]->GlobalReadMask = 0;
      TraceRef.Present[LWPresNum]->GlobalWriteMask = 0;
      TraceRef.Present[LWPresNum]->RWIter = LoopIter;
    }
    // 1.2.2) Still in the same iteration.
    // do-nothing.

    // 4) There are maybe dependences.

    // Not write to the Address before within the current interation.
    LSPresentRWTrace &PRWTraceRef = *(TraceRef.Present[LWPresNum]);
    if( PRWTraceRef.GlobalReadMask )
      //doWriteLIDA(PC, WriteMask, PRWTraceRef, TraceRef);
      doWriteLIDAWithSinkPCAsKey( PC, WriteMask, PRWTraceRef, TraceRef, ThrID);

    updatePresentWTrace(PC, ShadowAddr, WriteMask, PRWTraceRef, 0);

    return;
  }

#else // serial
  void doLIDAandUpdatePresentWTrace(void *PC, void **ShadowAddr, unsigned char  WriteMask, int LoopIter )
  {
    // 1) Get the Present buf.
    Trace &TraceRef =  *((Trace*)(*ShadowAddr));

    // 1.1) First access the address, so create a new present/precede buffer.
    // done in ReadLIDA::operator.

    // 1.2) App has read/written the address before.
    // 1.2.1) The Present[] is 0.

    // 1.2.2) A new iteratrion.
    // To remove.
    if( TraceRef.Present[LWPresNum]->RWIter != LoopIter ) {
      // Keep Loop Iteration RWTrace.
#ifdef __OPT1
      LIDAAddrInterval.insert( (long)ShadowAddr );
#endif
      // Opt me?
      // Reset tags of Present-Read-Trace.
      //PRWTraceRef.RTrace.clear();
      TraceRef.Present[LWPresNum]->GlobalReadMask = 0;
      TraceRef.Present[LWPresNum]->GlobalWriteMask = 0;
      TraceRef.Present[LWPresNum]->RWIter = LoopIter;
    }
    // 1.2.2) Still in the same iteration.
    // do-nothing.


    // 4) There are maybe dependences.

    // Not write to the Address before within the current interation.
    LSPresentRWTrace &PRWTraceRef = *(TraceRef.Present[LWPresNum]);
    if( PRWTraceRef.GlobalReadMask )
      //doWriteLIDA(PC, WriteMask, PRWTraceRef, TraceRef);
      doWriteLIDAWithSinkPCAsKey( PC, WriteMask, PRWTraceRef, TraceRef);

    updatePresentWTrace(PC, ShadowAddr, WriteMask, PRWTraceRef, 0);

    return;
  }
#endif

#ifdef _DDA_DEFAULT_PP_PARLIDA
  void parreadlida(int CurProc)  
  {
    int Length, LoopIter, LLRStreamNum;
    short int RWFlag;
    AddrStreamItem *LPtrItems;
    #ifdef _TIME_PROF
  tbb::tick_count tstart, pststart;
    #endif

    // Has data to tackle.
    LRStreamNum[CurProc] = ProcRStreamNum[CurProc];
    ThrLRStreamNum = LRStreamNum[CurProc];
    if( atomic_read(&PPStreamIsFull[CurProc][ThrLRStreamNum] ))
    {
      // bug?  Once read a Stream[], will finish it.
      // Init for each process
      Length = Streams[CurProc][ThrLRStreamNum].End;
      LoopIter = Streams[CurProc][ThrLRStreamNum].LoopIter;
      LPtrItems = &Streams[CurProc][ThrLRStreamNum].Items[1];
      //Length = Streams[CurProc][LRStreamNum[CurProc]].End;
      //LoopIter = Streams[CurProc][LRStreamNum[CurProc]].LoopIter;
      //LPtrItems = &Streams[CurProc][LRStreamNum[CurProc]].Items[1];

      LWPresNum[CurProc] = ProcWPresNum[CurProc]; // Thread private.
      ProcNextLidep[CurProc] = new LICDep; //

#ifdef _TIME_PROF
      tbb::tick_count prestart, prostart;
#endif

      // 2th level parallelism.
      // Parallel do LIDA analysis.
      if( Length > 1 ){
        struct ParLIDA PLIDAC;
        PLIDAC.LIDAPtr = this;
        PLIDAC.LoopIter = LoopIter;
        PLIDAC.Length = Length;
        PLIDAC.ParLIDACurProc = CurProc;
        PLIDAC.ParLIDAPtrItems = &Streams[CurProc][ThrLRStreamNum].Items[1]; 
        //PLIDAC.ParLIDAPtrItems = &Streams[CurProc][ThrLRStreamNum[CurProc]].Items[1]; 

        ProcThrLITDep[CurProc] = new std::map<long, std::set<long> > [RLIDATASKNUM];
        ProcThrLIADep[CurProc] = new std::map<long, std::set<long> > [RLIDATASKNUM];
        ProcThrLIODep[CurProc] = new std::map<long, std::set<long> > [RLIDATASKNUM];


#ifdef _TIME_PROF
        prostart = tbb::tick_count::now();
#endif
        //task_scheduler_init LIDAInit(LIDAThreadNum);
        parallel_for( blocked_range<int>(0, RLIDATASKNUM), PLIDAC);

#ifdef _TIME_PROF
        PROTIME += (tbb::tick_count::now() -prostart).seconds();
#endif
#ifdef _TIME_PROF
        prestart = tbb::tick_count::now();
#endif

        // Reduction for each proc.
        // Reduction-1) ShdAddr-Interval for LCDA.
        for( int i = 1; i < Length; LPtrItems++, i++){
          //std::cout<<std::setbase(16)<<"Insert Addr = "<<PLIDAShdAddr[i][j]<<"\n";
          if(ProcAccAddrOfIter[CurProc][i])
            LIDAAddrInterval[CurProc].insert( (long)LPtrItems->Addr );  // PRIVATE
        }

        // Reduction-2) Dep-info results.
        //std::unordered_map<long, std::set<long> >::iterator DepBeg, DepEnd;
        std::map<long, std::set<long> >::iterator DepBeg, DepEnd;
        std::set<long>::iterator SetBeg, SetEnd;
        std::map<long, std::set<long> > ss1;
        for( int i = 0; i <  RLIDATASKNUM; i++){
          // bugs, some procs may has no deps.
          DepBeg = ProcThrLITDep[CurProc][i].begin();  
          DepEnd = ProcThrLITDep[CurProc][i].end();  
          if(!ProcThrLITDep[CurProc][i].empty())
            for( ;DepBeg != DepEnd; ++DepBeg){
              SetBeg = DepBeg->second.begin(); 
              SetEnd = DepBeg->second.end();
              for( ; SetBeg != SetEnd; ++SetBeg) {
                ProcNextLidep[CurProc]->LITDep[(long)DepBeg->first].insert(*SetBeg); 
              }
            }
          //  next_lidep->LITDep.insert(DepBeg, DepEnd);

          DepBeg = ProcThrLIADep[CurProc][i].begin();  
          DepEnd = ProcThrLIADep[CurProc][i].end();  
          for( ;DepBeg != DepEnd; ++DepBeg){
            SetBeg = DepBeg->second.begin(); 
            SetEnd = DepBeg->second.end();
            for( ; SetBeg != SetEnd; ++SetBeg) {
              ProcNextLidep[CurProc]->LIADep[(long)DepBeg->first].insert(*SetBeg); 
            }
          }
          //  next_lidep->LIADep.insert(DepBeg, DepEnd);

          DepBeg = ProcThrLIODep[CurProc][i].begin();  
          DepEnd = ProcThrLIODep[CurProc][i].end();  
          for( ;DepBeg != DepEnd; ++DepBeg){
            SetBeg = DepBeg->second.begin(); 
            SetEnd = DepBeg->second.end();
            for( ; SetBeg != SetEnd; ++SetBeg) {
              ProcNextLidep[CurProc]->LIODep[(long)DepBeg->first].insert(*SetBeg); 
            }
          }
          //    next_lidep->LIODep.insert(DepBeg, DepEnd);
        }

// bug?.
#if 1
        delete []ProcThrLITDep[CurProc];
        delete []ProcThrLIADep[CurProc];
        delete []ProcThrLIODep[CurProc];
#endif
        //delete []AccAddrOfIter;
#ifdef _TIME_PROF
        PRETIME += (tbb::tick_count::now() -prestart).seconds();
#endif
      } // end if(Length > 1)

      RWFlag = Streams[CurProc][ThrLRStreamNum].Items[0].RWFlag;
      //RWFlag = Streams[CurProc][LRStreamNum[CurProc]].Items[0].RWFlag;
      if( RWFlag == 0 ){
        //std::cout<<"ReadLIDAFilter RWFlag = 0, RStreamNum = "<<RStreamNum<<"\n";
        atomic_set(&PPStreamIsFull[CurProc][ThrLRStreamNum], 0);
        //atomic_set(&PPStreamIsFull[CurProc][LRStreamNum[CurProc]], 0);
        ProcRStreamNum[CurProc] = (ProcRStreamNum[CurProc]+1) % STREAMNUM;
        LRStreamNum[CurProc] = ProcRStreamNum[CurProc];
        ProcNextLidep[CurProc]->EndLoopIter = 0;
        ProcNextLidep[CurProc]->StartLCDA = 0;
        ProcNextLidep[CurProc]->CancelPipeline = 0;
#ifdef _TIME_PROF
        //LIDATime += (tbb::tick_count::now() - pststart).seconds();
#endif 

      }
      // Start LCDA.
      else if( RWFlag == 1 ){
        //std::cout<<std::setbase(10)<<"LIDA RWFlag = 1, CurProc " << CurProc <<", LoopIter  "<< LoopIter <<", StreamNum " << LRStreamNum[CurProc] <<", PresNum " << ProcWPresNum[CurProc] << "Addr " << ProcNextLidep[0] << "\n"; 
        atomic_set(&PPStreamIsFull[CurProc][ThrLRStreamNum], 0);

        ProcNextLidep[CurProc]->LoopIter = Streams[CurProc][ThrLRStreamNum].LoopIter;
        ProcNextLidep[CurProc]->CurPresNum = LWPresNum[CurProc];
        ProcNextLidep[CurProc]->StartLCDA = 1;
        ProcNextLidep[CurProc]->CancelPipeline = 0;
        PresIsFull[LWPresNum[CurProc]] = 1;
        ProcNextLidep[CurProc]->EndLoopIter = 0;

        ProcRStreamNum[CurProc] = (ProcRStreamNum[CurProc]+1) % STREAMNUM;
        LRStreamNum[CurProc] = ProcRStreamNum[CurProc];
        // Update PresentRWTrace buffer.
        //WPresNum = (WPresNum+1) % PRESBUFNUM;
        //WPresNum = WPresNum;
        //ProcWPresNumTh[CurProc] += 1;
        //ProcWPresNum[CurProc] += PROCNUM;
        LWPresNum[CurProc] += PROCNUM;
        ProcWPresNum[CurProc] = LWPresNum[CurProc] % PRESBUFNUM;
        LWPresNum[CurProc] = ProcWPresNum[CurProc]; 

#ifdef _TIME_PROF
        tstart = tbb::tick_count::now();
#endif 
        while( PresIsFull[LWPresNum[CurProc]] );
#ifdef _TIME_PROF
        LIDAPTime += (tbb::tick_count::now() - tstart).seconds();
#endif 

        // LCDA address range.   To_OPT: copy-assignment. bug ?
        ProcNextLidep[CurProc]->LCDAAddrInterval = LIDAAddrInterval[CurProc];
        LIDAAddrInterval[CurProc].clear();

        //return next_lidep;
      }
      // Finish the Profing of Cur loop.
      else if(RWFlag == 4){
        //std::cout<<"ReadLIDAFilter RWFlag = 4 CurProc " << CurProc << "\n";
        ProcNextLidep[CurProc]->StartLCDA = 0;
        ProcNextLidep[CurProc]->EndLoopIter = LoopIter;
      }
      // Cancel pipeline, read this buffer again, so not do RStreamNum += 1;
      else if( RWFlag == 2 ){
        //std::cout<<"ReadLIDAFilter RWFlag = 2 CurProc " << CurProc << "\n";
        ProcNextLidep[CurProc]->StartLCDA = 0;
        ProcNextLidep[CurProc]->EndLoopIter = 0;
        ProcNextLidep[CurProc]->CancelPipeline = 2;
        Streams[CurProc][LRStreamNum[CurProc]].Items[0].RWFlag = 3;

        //return next_lidep;
      }

      else if( RWFlag == 3 ){
        std::cout<<"ReadLIDAFilter send CanclePipeline signal \n";
        ProcNextLidep = NULL;
        //ProcNextLidep[CurProc]->CancelPipeline = 3;
        //return NULL;
      }
    }
  }
#endif

#ifdef _DDA_DEFAULT_PP_PARLIDA  // ReadLIDAFilter
  void parcheck(int LoopIter, int Length, int ThrID, AddrStreamItem *PtrItems, int CurProc)
  {
    long ShdAddr, AddrInnerOffset, len1;
    Trace *ShdTrcPtr;
    int ItemNum, AddrIndex, RWFlag, AddrLen, tnum = 0;
    unsigned  char RWMask1;
    AddrStreamItem *LPtrItems = PtrItems;  // Private for 2th level thread par.

    for( ItemNum = 1; ItemNum < Length; LPtrItems++, ItemNum++){
      AddrIndex = ((long) LPtrItems->Addr & PARLIDAMASK)>>4;
      //AddrIndex = AddrIndex>>4;
      if( ThrID == AddrIndex ){
        //std::cout<<"ThrID " << ThrID <<", AddrIndex " << AddrIndex << "\n";
        //tnum++;
        ShdAddr = (long)LPtrItems->Addr & 0x6ffffffffff8;
        AddrInnerOffset = ShdAddr & 0x0000000000000007;
        AddrLen = LPtrItems->AddrLen;
        RWFlag = LPtrItems->RWFlag;
        LPtrItems->Addr = (int*)ShdAddr ;
        len1 = AddrInnerOffset + AddrLen; 
        RWMask1 = (1<<len1) - (1<<AddrInnerOffset); 
        ShdTrcPtr = *((Trace**)ShdAddr);

        ProcAccAddrOfIter[CurProc][ItemNum] = 0;

        // Not DoLIAD before, so apply buffer for the Shadow.
        if( ShdTrcPtr ){
          // Do LIDA.
          if( RWFlag )
            doLIDAandUpdatePresentWTrace(LPtrItems->PC, (void**)ShdAddr, RWMask1, LoopIter, ItemNum, ThrID, CurProc);
          else
            doLIDAandUpdatePresentRTrace(LPtrItems->PC, (void**)ShdAddr, RWMask1, LoopIter, ItemNum, ThrID, CurProc);
        } // end if( ShdTrcPtr->Precede == NULL)

        else{
          // Keep Loop Iteration RWTrace.
#ifdef __OPT1
          //LIDAAddrInterval.insert((long)ShdAddr);
          ProcAccAddrOfIter[CurProc][ItemNum] = 1;
#endif
          Trace *TrcPtr = (Trace*) malloc(sizeof(Trace));
          *((Trace**)ShdAddr) = TrcPtr;
          TrcPtr->Precede = (LSPrecedeRWTrace*) malloc (sizeof(LSPrecedeRWTrace));
          TrcPtr->Precede->RTrace = 0;
          TrcPtr->Precede->WTrace = 0;
          TrcPtr->Precede->GlobalReadMask = TrcPtr->Precede->GlobalWriteMask = 0;

          for(int i = 0; i < PRESBUFNUM; i++){
            TrcPtr->Present[i] = new LSPresentRWTrace;   
            TrcPtr->Present[i]->WTrace = 0;
            TrcPtr->Present[i]->RTrace = 0;
            TrcPtr->Present[i]->GlobalWriteMask = 0;
            TrcPtr->Present[i]->GlobalReadMask = 0;
            TrcPtr->Present[i]->RWIter =  0; // bug?
          }

          // Add the RWTrace into the PresentRWTrace.
          LSPresentRWTrace &PRWTraceRef = *(TrcPtr->Present[ProcWPresNum[CurProc]]);
          PRWTraceRef.RWIter = LoopIter;
          //  TrcPtr->Present[LWPresNum]->RWIter =  LoopIter; // bug?
          if( RWFlag )
            updatePresentWTrace(LPtrItems->PC, (void**)ShdAddr, RWMask1, PRWTraceRef, 0);
          else
            updatePresentRTrace(LPtrItems->PC, (void**)ShdAddr, RWMask1, PRWTraceRef, 0);
        }

      } // if ThrID == Index
    } // for ItemNum = 1;

  }
#endif


  //

  private:
  //std::interval_set<long> ReadAddrInterval;
  // Output;
  class RWTrace* next_rwtrace;
  /*override*/ 
  void* operator()(void*); // why private?
  inline void doReadLIDA(void * PC, unsigned char ReadMask, LSPresentRWTrace &PRWTraceRef, Trace &TraceRef);
#if defined(_DDA_DEFAULT_PP_PARLIDA)
  inline void doWriteLIDAWithSinkPCAsKey(void * PC, unsigned char WriteMask, 
      LSPresentRWTrace &PRWTraceRef, Trace &TraceRef, int ThreadID, int CurProc);
#elif defined(_DDA_DEFAULT_PARLIDA) 
  inline void doWriteLIDAWithSinkPCAsKey(void * PC, unsigned char WriteMask, 
      LSPresentRWTrace &PRWTraceRef, Trace &TraceRef, int ThreadID);
#else   
  inline void doWriteLIDAWithSinkPCAsKey(void * PC, unsigned char WriteMask, 
      LSPresentRWTrace &PRWTraceRef, Trace &TraceRef);
#endif   
  inline void doWriteLIDA(void * PC, unsigned char WriteMask,LSPresentRWTrace &PRWTraceRef, 
      Trace &TraceRef);

  private:
#ifdef _DDA_DEFAULT_PP_PARLIDA
  class LICDep **ProcNextLidep;  // PRIVATE
  class LICDep **ProcNextLidepx;  // PRIVATE
  interval_set<long> *LIDAAddrInterval; // For what?
  //class LICDep *ProcNextLidep[PROCNUM];  // PRIVATE
  //interval_set<long> *LIDAAddrInterval[PROCNUM]; // For what?
  // local opt.
  long LRStreamNum[PROCNUM];
  long LWPresNum[PROCNUM];
#else // _PARLIDA or serial.
  class LICDep *next_lidep;
  interval_set<long> LIDAAddrInterval; // For what?
  long LRStreamNum;
#endif
  // For LCDA, Thread local? Cross multi-task.
  long LIDAStackMin, LIDAStackMax, LIDAHeapMax, LIDAHeapMin, LIDAGlobalMin, LIDAGlobalMax;

  // Many Deps happens repeatly, so we keep a memo of these deps.
  // when reset?
  std::map<long,std::set<long> > HLITDep;
  std::map<long,std::set<long> > HLIADep;
  std::map<long,std::set<long> > HLIODep;

#if defined( _DDA_DEFAULT_PARLIDA)
  std::map<long,std::set<long> > *ThrLITDep;
  std::map<long,std::set<long> > *ThrLIADep;
  std::map<long,std::set<long> > *ThrLIODep;
  int PLIDACurNum[LIDAThreadNum];
  int PLIDALength;
  long *PLIDAShdAddr[LIDAThreadNum];
  long *PLIDAPtrShdAddr; // ShdAddrStream generated by GenerateAddrStream. 
#endif
};

  ReadLIDAFilter::ReadLIDAFilter():
   filter(serial_in_order)
  //next_rwtrace( RWTraceDep::allocate(0) )
{ 
#ifdef _DDA_DEFAULT_PP_PARLIDA
  LIDAAddrInterval = new interval_set<long> [PROCNUM]; // For what?
  //ProcNextLidep = new LICDep* [PROCNUM];
  //ProcNextLidepx = new LICDep* [PROCNUM];
  for( int i = 0; i < PROCNUM; i++){
    LRStreamNum[i] =  ProcRStreamNum[i];
    LWPresNum[i] = 0;
  }
#else
  LRStreamNum =  RStreamNum;
#endif
  //std::cout<<"ReadLIDAFilter \n";
}

ReadLIDAFilter::~ReadLIDAFilter() 
{
  //next_rwtrace->free(); // not need, can be done by the follow filter.

}

/* Look for Loop independent output dependences. 
// Todo:
// 1) Addr1: W R R R R ===> not need to check True-dep every time.
// LIOD & LIAD.
// 2) The results are stored in the ShadowAddr(SinkPC).
// 3) Dist[0]->LIDep, Dist[1]->LCDep;
*/ 
#if defined (_DDA_DEFAULT_PP_PARLIDA) 
inline void  ReadLIDAFilter::doWriteLIDAWithSinkPCAsKey(void * PC, unsigned char WriteMask,
    LSPresentRWTrace &PRWTraceRef, Trace &TraceRef, int ThrID, int CurProc)
{
  unsigned char RWMask;
  unsigned char NewRWMask;

  if( (RWMask = ( WriteMask & PRWTraceRef.GlobalWriteMask)) ) {
    LSPresentRWAddrID *WIDPtr = PRWTraceRef.WTrace;
    std::map<long, std::set<long> > &LIODep = ProcThrLIODep[CurProc][ThrID]; // bug?
    //std::map<long, std::set<long> > &LIODep = next_lidep->LIODep; // bug?
    std::map<long, std::set<long> >::iterator it;

    for( ; (WIDPtr != 0 ) && RWMask; WIDPtr = WIDPtr->Next){
      NewRWMask = RWMask;
      RWMask &= RWMask ^ WIDPtr->RWMask;
      // LI-Output.
      // WIDPtr->PC ===> (long)PC
      if( RWMask != NewRWMask ){
#ifdef _DEBUGINFO_LIDA1
        //std::cout<<"doWriteLIDWithSinkPCAsKeyLICDepPA: LIODep SinkPC = " << PC << "\n";
#endif
        long SinkPC = (long) PC;
        long SrcPC = WIDPtr->PC;
        bool inH = 0;
        LSDepInfo **DepShadowPtr;
        //LSDepInfo *LIDepPtr;
        DepShadowPtr = (LSDepInfo**) AppAddrToShadow(SinkPC);
        if( *DepShadowPtr != 0 ){
          //LIDepPtr = *DepShadowPtr;
          LSDep *LIDepOut = (*DepShadowPtr)->OutDep; //TraceRef.LIDep->TrueDep; 
          while( LIDepOut != 0 ){
            if(LIDepOut->SrcPC == SrcPC){
              inH = 1;
              break;
            }
            LIDepOut = LIDepOut->next;
          }
        }
        if( !inH )
          LIODep[SinkPC].insert(SrcPC);
#ifdef _DEBUGINFO
        std::cout<<"doWriteLIDWithSinkPCAsKey: LIODep SinkPC = " << PC << ", SrcPC = " << WIDPtr->PC << "\n";
#endif

      }
    } 
  }


  // LIAD.
  if( (RWMask = ( WriteMask & PRWTraceRef.GlobalReadMask)) ) {

    //std::map<long, std::set<long> > &LIADep = next_lidep->LIADep;
    std::map<long, std::set<long> > &LIADep = ProcThrLIADep[CurProc][ThrID];
    //std::map<long, std::set<long> >::iterator it;

    LSPresentRWAddrID *RIDPtr = PRWTraceRef.RTrace;
    for( ; (RIDPtr != 0) && RWMask; RIDPtr = RIDPtr->Next){
      NewRWMask = RWMask;
      RWMask &= RWMask ^ RIDPtr->RWMask;

      // LI-AntiDep.
      // RIDPtr->PC ===>  (long)PC
      if( RWMask != NewRWMask ){
#ifdef _DEBUGINFO1
        //std::cout<<"doWriteLIDWithSinkPCAsKeyLICDepPA: LIADep SinkPC = " << PC << "\n";
#endif
        long SinkPC = (long)PC;
        long SrcPC = RIDPtr->PC;
        bool inH = 0;

        LSDepInfo **DepShadowPtr;
        //LSDepInfo *LIDepPtr;
        DepShadowPtr = (LSDepInfo**) AppAddrToShadow(SinkPC);
        if( *DepShadowPtr != 0 ){
          //LIDepPtr = *DepShadowPtr;
          LSDep *LIDepAnti = (*DepShadowPtr)->AntiDep; //TraceRef.LIDep->TrueDep; 
          while( LIDepAnti != 0 ){
            if(LIDepAnti->SrcPC == SrcPC ){
              inH = 1;
              break;
            }
            LIDepAnti = LIDepAnti->next;
          }
        }
        if(!inH){
          LIADep[SinkPC].insert(SrcPC);
        }
#ifdef _DEBUGINFO
        std::cout<<"doWriteLIDWithSinkPCAsKey: LIADep SinkPC = " << PC << ", SrcPC = " << RIDPtr->PC <<   "\n";
#endif

      }
    } 

  }

  return;
}


/* Look for Loop independent output dependences. 
// Todo:
// 1) Addr1: W R R R R ===> not need to check True-dep every time.
// LIOD & LIAD.
// 2) The results are stored in the ShadowAddr(SinkPC).
// 3) Dist[0]->LIDep, Dist[1]->LCDep;
*/ 
#elif defined(_DDA_DEFAULT_PARLIDA) 
inline void  ReadLIDAFilter::doWriteLIDAWithSinkPCAsKey(void * PC, unsigned char WriteMask, LSPresentRWTrace &PRWTraceRef, Trace &TraceRef, int ThrID)
{
  unsigned char RWMask;
  unsigned char NewRWMask;

  if( (RWMask = ( WriteMask & PRWTraceRef.GlobalWriteMask)) ) {
    LSPresentRWAddrID *WIDPtr = PRWTraceRef.WTrace;
    std::map<long, std::set<long> > &LIODep = ThrLIODep[ThrID]; // bug?
    //std::map<long, std::set<long> > &LIODep = next_lidep->LIODep; // bug?
    std::map<long, std::set<long> >::iterator it;

    for( ; (WIDPtr != 0 ) && RWMask; WIDPtr = WIDPtr->Next){
      NewRWMask = RWMask;
      RWMask &= RWMask ^ WIDPtr->RWMask;
      // LI-Output.
      // WIDPtr->PC ===> (long)PC
      if( RWMask != NewRWMask ){
#ifdef _DEBUGINFO_LIDA1
        //std::cout<<"doWriteLIDWithSinkPCAsKeyLICDepPA: LIODep SinkPC = " << PC << "\n";
#endif
        long SinkPC = (long) PC;
        long SrcPC = WIDPtr->PC;
        bool inH = 0;
#if 1
        LSDepInfo **DepShadowPtr;
        //LSDepInfo *LIDepPtr;
        DepShadowPtr = (LSDepInfo**) AppAddrToShadow(SinkPC);
        if( *DepShadowPtr != 0 ){
          //LIDepPtr = *DepShadowPtr;
          LSDep *LIDepOut = (*DepShadowPtr)->OutDep; //TraceRef.LIDep->TrueDep; 
          while( LIDepOut != 0 ){
            if(LIDepOut->SrcPC == SrcPC){
              inH = 1;
              break;
            }
            LIDepOut = LIDepOut->next;
          }
        }
#endif
        if( !inH )
          LIODep[SinkPC].insert(SrcPC);
#ifdef _DEBUGINFO
        std::cout<<"doWriteLIDWithSinkPCAsKey: LIODep SinkPC = " << PC << ", SrcPC = " << WIDPtr->PC << "\n";
#endif

      }
    } 
  }


  // LIAD.
  if( (RWMask = ( WriteMask & PRWTraceRef.GlobalReadMask)) ) {

    //std::map<long, std::set<long> > &LIADep = next_lidep->LIADep;
    std::map<long, std::set<long> > &LIADep = ThrLIADep[ThrID];
    //std::map<long, std::set<long> >::iterator it;

    LSPresentRWAddrID *RIDPtr = PRWTraceRef.RTrace;
    for( ; (RIDPtr != 0) && RWMask; RIDPtr = RIDPtr->Next){
      NewRWMask = RWMask;
      RWMask &= RWMask ^ RIDPtr->RWMask;

      // LI-AntiDep.
      // RIDPtr->PC ===>  (long)PC
      if( RWMask != NewRWMask ){
#ifdef _DEBUGINFO1
        //std::cout<<"doWriteLIDWithSinkPCAsKeyLICDepPA: LIADep SinkPC = " << PC << "\n";
#endif
        long SinkPC = (long)PC;
        long SrcPC = RIDPtr->PC;
        bool inH = 0;
#if 1
        LSDepInfo **DepShadowPtr;
        //LSDepInfo *LIDepPtr;
        DepShadowPtr = (LSDepInfo**) AppAddrToShadow(SinkPC);
        if( *DepShadowPtr != 0 ){
          //LIDepPtr = *DepShadowPtr;
          LSDep *LIDepAnti = (*DepShadowPtr)->AntiDep; //TraceRef.LIDep->TrueDep; 
          while( LIDepAnti != 0 ){
            if(LIDepAnti->SrcPC == SrcPC ){
              inH = 1;
              break;
            }
            LIDepAnti = LIDepAnti->next;
          }
        }
#endif
        if(!inH){
          LIADep[SinkPC].insert(SrcPC);
        }
#ifdef _DEBUGINFO
        std::cout<<"doWriteLIDWithSinkPCAsKey: LIADep SinkPC = " << PC << ", SrcPC = " << RIDPtr->PC <<   "\n";
#endif

      }
    } 

  }

  return;
}

#else
  inline void 
ReadLIDAFilter::doWriteLIDAWithSinkPCAsKey(void * PC, unsigned char WriteMask, LSPresentRWTrace &PRWTraceRef, Trace &TraceRef)
{
  unsigned char RWMask;
  unsigned char NewRWMask;

  if( (RWMask = ( WriteMask & PRWTraceRef.GlobalWriteMask)) ) {
    LSPresentRWAddrID *WIDPtr = PRWTraceRef.WTrace;
#if 1
    //std::map<long, std::set<long> > &LIODep = ThrLIODep[ThreadID]; // bug?
    std::map<long, std::set<long> > &LIODep = next_lidep->LIODep; // bug?
    std::map<long, std::set<long> >::iterator it;
#endif
    for( ; (WIDPtr != 0 ) && RWMask; WIDPtr = WIDPtr->Next){
      NewRWMask = RWMask;
      RWMask &= RWMask ^ WIDPtr->RWMask;
      // LI-Output.
      // WIDPtr->PC ===> (long)PC
      if( RWMask != NewRWMask ){
#ifdef _DEBUGINFO_LIDA1
        //std::cout<<"doWriteLIDWithSinkPCAsKeyLICDepPA: LIODep SinkPC = " << PC << "\n";
#endif
        long SinkPC = (long) PC;
        long SrcPC = WIDPtr->PC;
        bool inH = 0;
#if 1
        LSDepInfo **DepShadowPtr;
        //LSDepInfo *LIDepPtr;
        DepShadowPtr = (LSDepInfo**) AppAddrToShadow(SinkPC);
        if( *DepShadowPtr != 0 ){
          //LIDepPtr = *DepShadowPtr;
          LSDep *LIDepOut = (*DepShadowPtr)->OutDep; //TraceRef.LIDep->TrueDep; 
          while( LIDepOut != 0 ){
            if(LIDepOut->SrcPC == SrcPC){
              inH = 1;
              break;
            }
            LIDepOut = LIDepOut->next;
          }
        }
#endif
        if( !inH )
          LIODep[SinkPC].insert(SrcPC);
#ifdef _DEBUGINFO
        std::cout<<"doWriteLIDWithSinkPCAsKey: LIODep SinkPC = " << PC << ", SrcPC = " << WIDPtr->PC << "\n";
#endif

      }
    } 
  }


  // LIAD.
  if( (RWMask = ( WriteMask & PRWTraceRef.GlobalReadMask)) ) {

#if 1
    std::map<long, std::set<long> > &LIADep = next_lidep->LIADep;
    //std::map<long, std::set<long> > &LIADep = ThrLIADep[TaskId];
    //std::map<long, std::set<long> >::iterator it;
#endif

    LSPresentRWAddrID *RIDPtr = PRWTraceRef.RTrace;
    for( ; (RIDPtr != 0) && RWMask; RIDPtr = RIDPtr->Next){
      NewRWMask = RWMask;
      RWMask &= RWMask ^ RIDPtr->RWMask;

      // LI-AntiDep.
      // RIDPtr->PC ===>  (long)PC
      if( RWMask != NewRWMask ){
#ifdef _DEBUGINFO1
        //std::cout<<"doWriteLIDWithSinkPCAsKeyLICDepPA: LIADep SinkPC = " << PC << "\n";
#endif
        long SinkPC = (long)PC;
        long SrcPC = RIDPtr->PC;
        bool inH = 0;
#if 1
        LSDepInfo **DepShadowPtr;
        //LSDepInfo *LIDepPtr;
        DepShadowPtr = (LSDepInfo**) AppAddrToShadow(SinkPC);
        if( *DepShadowPtr != 0 ){
          //LIDepPtr = *DepShadowPtr;
          LSDep *LIDepAnti = (*DepShadowPtr)->AntiDep; //TraceRef.LIDep->TrueDep; 
          while( LIDepAnti != 0 ){
            if(LIDepAnti->SrcPC == SrcPC ){
              inH = 1;
              break;
            }
            LIDepAnti = LIDepAnti->next;
          }
        }
#endif
        if(!inH){
          LIADep[SinkPC].insert(SrcPC);
        }
#ifdef _DEBUGINFO
        std::cout<<"doWriteLIDWithSinkPCAsKey: LIADep SinkPC = " << PC << ", SrcPC = " << RIDPtr->PC <<   "\n";
#endif

      }
    } 

  }

  return;
}
#endif

  inline
void ReadLIDAFilter::doWriteLIDA(void * PC, unsigned char WriteMask, LSPresentRWTrace &PRWTraceRef, Trace &TraceRef )
{


  // 1) Take as the key the Write-Address to store data dependence results.
#ifdef __ADDRKEY
  __doWriteLIDWithAddrAsKey(PC, WriteMask, PRWTraceRef, TraceRef);
#endif
  // 2) Take as the key the SrcPC of dependence pair to store data dependence
  // results.
  //__doWriteLIDWithSinkPCAsKey( PC, WriteMask, PRWTraceRef, TraceRef);

  // 3) bug? Not called in this file.
  #ifndef _DDA_DEFAULT_PP_PARLIDA
  doWriteLIDAWithSinkPCAsKey( PC, WriteMask, PRWTraceRef, TraceRef, 0);
  #endif

  return;
}






// Todo:
// 1) Addr1: W R R R R ===> not need to check True-dep every time.
// LITD.
inline void ReadLIDAFilter::doReadLIDA(void * PC, unsigned char ReadMask,
    LSPresentRWTrace &PRWTraceRef, Trace &TraceRef)
{


  // 3) LI/LC Store together. 
  //doReadLIDAWithSinkPCAsKey( PC, ReadMask, PRWTraceRef, TraceRef);

  return;
}

#if defined(_DDA_DEFAULT_PP_PARLIDA)

/* 1) Read RWTrace from Streams[];
 ** 2) Do LIDA;
 ** 3) Setup the PresentRWTrace in shadow memory.
 ** 4) Check whether need LCDA.
 ** 5) ProfProc: TPresNum = PROCID+PROCNUM*PresNum;( i = 0, 1, 2, ... )
 proc          proc0                   proc1   proc2   proc3
                |
                |
 thread        read0                   read1   read2   read3
              /   |   \
             /    |    \
 thread  LIDA0 LIDA1...LIDAn
 */
void* ReadLIDAFilter::operator()(void*) 
{

#ifdef _TIME_PROF
  tbb::tick_count tstart, pststart;
  pststart = tbb::tick_count::now();
#endif

  // 1) Read RWTrace from Streams[].
  // 1.1) Read from the Streams[] one by one.
  //LRStreamNum = RStreamNum;
  int j, i;
  for( i = 0, j = 0; ; ++i, i = i % PROCNUM ){
    if( atomic_read(&PPStreamIsFull[i][ProcRStreamNum[i] ]) ){ 
      break;
    }
  }

#ifdef _TIME_PROF
  RCTime += (tbb::tick_count::now() - pststart).seconds();
#endif 

  ProcNextLidep = new LICDep* [PROCNUM];
  for( i = 0; i < PROCNUM; ++i)
    ProcNextLidep[i] = 0;

  struct ParReadLIDA PRLIDA;
  PRLIDA.LIDAPtr = this;
  parallel_for( blocked_range<int>(0, PROCNUM), PRLIDA );

#ifdef _TIME_PROF
  ReadTime += (tbb::tick_count::now() - pststart).seconds();
#endif 

  return ProcNextLidep; // bug
}

#elif defined(_DDA_DEFAULT_PARLIDA) // ifndef _DDA_PP
/* 1) Read RWTrace from Streams[];
 ** 2) Do LIDA;
 ** 3) Setup the PresentRWTrace in shadow memory.
 ** 4) Check whether need LCDA.
 */
void* ReadLIDAFilter::operator()(void*) 
{
  int LoopIter, Length;
  AddrStreamItem *PtrItems;
  short int RWFlag;
  //long LStackMin, LStackMax, LHeapMax, LHeapMin, LGlobalMin, LGlobalMax;

#ifdef _TIME_PROF
  tbb::tick_count tstart, pststart;
  pststart = tbb::tick_count::now();
#endif

  // 1) Read RWTrace from Streams[].
  // 1.1) Read from the Streams[] one by one.
  LRStreamNum = RStreamNum;
#ifdef _TIME_PROF
  tstart = tbb::tick_count::now();
#endif 
  while( !StreamIsFull[LRStreamNum] ); // printf("RCTime RStreamNum = %d \n", RStreamNum); // OH: worst.
#ifdef _TIME_PROF
  RCTime += (tbb::tick_count::now() - tstart).seconds();
#endif 
  // bug?  Once read a Stream[], will finish it.
  Length = Streams[LRStreamNum].End;
  LoopIter = Streams[LRStreamNum].LoopIter;
  PtrItems = &Streams[LRStreamNum].Items[1];

  // 
  next_lidep = new LICDep; //


#if defined(_PARLIDA)

#ifdef _TIME_PROF
  tbb::tick_count prestart, prostart;
#endif

  // Par process.
  if( Length > 1 ){
    struct ParLIDA PLIDAC;
    PLIDAC.LoopIter = LoopIter;
    PLIDAC.LIDAPtr = this;
    PLIDAC.LRStreamNum = LRStreamNum;
    PLIDAC.LWPresNum = LWPresNum;
    PLIDALength = Length;
    PLIDAPtrItems = &Streams[LRStreamNum].Items[1]; 

    ThrLITDep = new std::map<long, std::set<long> > [RLIDATASKNUM];
    ThrLIADep = new std::map<long, std::set<long> > [RLIDATASKNUM];
    ThrLIODep = new std::map<long, std::set<long> > [RLIDATASKNUM];


#ifdef _TIME_PROF
    prostart = tbb::tick_count::now();
#endif
    //task_scheduler_init LIDAInit(LIDAThreadNum);
    parallel_for( blocked_range<int>(0, RLIDATASKNUM), PLIDAC);

#ifdef _TIME_PROF
    PROTIME += (tbb::tick_count::now() -prostart).seconds();
#endif
    //  num++;
    // std::cout<<"num " << num <<", PROTIME " << PROTIME <<"\n";

#ifdef _TIME_PROF
    prestart = tbb::tick_count::now();
#endif


    // Reduction.
    // Reduction-1) ShdAddr-Interval for LCDA.
    for( int i = 1; i < Length; PtrItems++, i++){
      //std::cout<<std::setbase(16)<<"Insert Addr = "<<PLIDAShdAddr[i][j]<<"\n";
      if(AccAddrOfIter[i])
        LIDAAddrInterval.insert( (long)PtrItems->Addr );
    }

#if 1
    // Reduction-2) Dep-info results.
    //std::unordered_map<long, std::set<long> >::iterator DepBeg, DepEnd;
    std::map<long, std::set<long> >::iterator DepBeg, DepEnd;
    std::set<long>::iterator SetBeg, SetEnd;
    for( int i = 0; i <  RLIDATASKNUM; i++){
      DepBeg = ThrLITDep[i].begin();  
      DepEnd = ThrLITDep[i].end();  
#if 1
      for( ;DepBeg != DepEnd; ++DepBeg){
        SetBeg = DepBeg->second.begin(); 
        SetEnd = DepBeg->second.end();
        for( ; SetBeg != SetEnd; ++SetBeg) {
          next_lidep->LITDep[(long)DepBeg->first].insert(*SetBeg); 
        }
      }
#endif
      //  next_lidep->LITDep.insert(DepBeg, DepEnd);

      DepBeg = ThrLIADep[i].begin();  
      DepEnd = ThrLIADep[i].end();  
#if 1
      for( ;DepBeg != DepEnd; ++DepBeg){
        SetBeg = DepBeg->second.begin(); 
        SetEnd = DepBeg->second.end();
        for( ; SetBeg != SetEnd; ++SetBeg) {
          next_lidep->LIADep[(long)DepBeg->first].insert(*SetBeg); 
        }
      }
#endif
      //  next_lidep->LIADep.insert(DepBeg, DepEnd);

      DepBeg = ThrLIODep[i].begin();  
      DepEnd = ThrLIODep[i].end();  
      for( ;DepBeg != DepEnd; ++DepBeg){
        SetBeg = DepBeg->second.begin(); 
        SetEnd = DepBeg->second.end();
        for( ; SetBeg != SetEnd; ++SetBeg) {
          next_lidep->LIODep[(long)DepBeg->first].insert(*SetBeg); 
        }
      }
#if 0
      ThrLITDep[i].clear();
      ThrLIADep[i].clear();
      ThrLIODep[i].clear();
#endif
      //    next_lidep->LIODep.insert(DepBeg, DepEnd);
    }
#endif

#if 1
    delete []ThrLITDep;
    delete []ThrLIADep;
    delete []ThrLIODep;
#endif
    //delete []AccAddrOfIter;
#ifdef _TIME_PROF
    PRETIME += (tbb::tick_count::now() -prestart).seconds();
#endif


  }


#else
  for( i = 1; i < Length; PtrItems++, i++){
    // 1.2) Read RWTrace info.
    ShdAddr = (long)ptritems->addr & 0x6ffffffffff8;  
    RWFlag = PtrItems->RWFlag;
    long addrinneroffset = ShdAddr & 0x0000000000000007;
    long len1, len2, len3;
    len3 = addrinneroffset + AddrLen; 
    //if( len3 <= 8 ) 
    RWmask1 = (1<<len3) - (1<<addrinneroffset); 

    // 1.3) Train Address Ranges.
    // LIDAAddrInterval.insert(ShdAddr);

    // 2) Do LIDA and setup presentRWTrace in shadow memory.
    ShdTrcPtr = *((Trace**)ShdAddr);
    if( ShdTrcPtr != 0) 
    {
      // Do LIDA.
      if( RWFlag == 0 ){
        doLIDAandUpdatePresentRTrace(PtrItems->PC, (void**)ShdAddr, RWMask1, LoopIter);
      }
      else{
        doLIDAandUpdatePresentWTrace(PtrItems->PC, (void**)ShdAddr, RWMask1, LoopIter);
      }
    } // end if
    // Apply buffer for the Shadow.
    else{
      // Keep Loop Iteration RWTrace.
#ifdef __OPT1
      LIDAAddrInterval.insert((long)ShdAddr);
#endif

      Trace *TrcPtr = (Trace*) malloc(sizeof(Trace));
      *((Trace**)ShdAddr) = TrcPtr;
      TrcPtr->Precede = (LSPrecedeRWTrace*) malloc (sizeof(LSPrecedeRWTrace));
      TrcPtr->Precede->RTrace = 0;
      TrcPtr->Precede->WTrace = 0;
      TrcPtr->Precede->GlobalReadMask = TrcPtr->Precede->GlobalWriteMask = 0;

      for(int i = 0; i < PRESBUFNUM; i++){
        TrcPtr->Present[i] = new LSPresentRWTrace;   
        TrcPtr->Present[i]->WTrace = 0;
        TrcPtr->Present[i]->RTrace = 0;
        TrcPtr->Present[i]->GlobalWriteMask = 0;
        TrcPtr->Present[i]->GlobalReadMask = 0;
        TrcPtr->Present[i]->RWIter =  0; // bug?
      }

      // Add the RWTrace into the PresentRWTrace.
      LSPresentRWTrace &PRWTraceRef = *(TrcPtr->Present[LWPresNum]);
      PRWTraceRef.RWIter = LoopIter;
      if(RWFlag == 0 ){
        updatePresentRTrace( PtrItems->PC, (void**)ShdAddr, RWMask1, PRWTraceRef, 0);
      }
      else{
        updatePresentWTrace(PtrItems->PC, (void**)ShdAddr, RWMask1, PRWTraceRef, 0);
      }
    } // end else
  } // end for

#endif
  RWFlag = Streams[LRStreamNum].Items[0].RWFlag;

  if( RWFlag == 0 ){
    //std::cout<<"ReadLIDAFilter RWFlag = 0, RStreamNum = "<<RStreamNum<<"\n";
    StreamIsFull[LRStreamNum] = 0;
    RStreamNum = (RStreamNum+1) % STREAMNUM;
    LRStreamNum = RStreamNum;
    next_lidep->StartLCDA = 0;
    next_lidep->CancelPipeline = 0;
#ifdef _TIME_PROF
    LIDATime += (tbb::tick_count::now() - pststart).seconds();
#endif 
    return next_lidep;

  }
  // Start LCDA.
  else if( RWFlag == 1 ){
    //std::cout<<"ReadLIDAFilter RWFlag = 1, RStreamNum = "<<RStreamNum<<"\n";
    //std::cout<<"ReadLIDAFilter RWFlag = 1, LoopIter = "<< LoopIter <<"\n";
    StreamIsFull[LRStreamNum] = 0;
    next_lidep->LoopIter = Streams[LRStreamNum].LoopIter;
    RStreamNum = (RStreamNum+1) % STREAMNUM;
    LRStreamNum = RStreamNum;
    next_lidep->StartLCDA = 1;
    next_lidep->CancelPipeline = 0;
    PresIsFull[LWPresNum] = 1;

    // Update PresentRWTrace buffer.
    WPresNum = (WPresNum+1) % PRESBUFNUM;
    LWPresNum = WPresNum;

#ifdef _TIME_PROF
    tstart = tbb::tick_count::now();
#endif 
    while( PresIsFull[LWPresNum] );
#ifdef _TIME_PROF
    LIDAPTime += (tbb::tick_count::now() - tstart).seconds();
#endif 

    // LCDA address range.   To_OPT: copy-assignment. 
    next_lidep->LCDAAddrInterval = LIDAAddrInterval;
    LIDAAddrInterval.clear();

#ifdef _TIME_PROF
    ReadTime += (tbb::tick_count::now() - pststart).seconds();
#endif 
    return next_lidep;
  }
  // Cancel pipeline, read this buffer again, so not do RStreamNum += 1;
  else if( RWFlag == 2 ){
    // std::cout<<"ReadLIDAFilter RWFlag = 2, RStreamNum = "<<RStreamNum<<"\n";
    next_lidep->StartLCDA = 0;
    next_lidep->CancelPipeline = 2;
    Streams[LRStreamNum].Items[0].RWFlag = 3;
#ifdef _TIME_PROF
    ReadTime += (tbb::tick_count::now() - pststart).seconds();
#endif 
    return next_lidep;
  }

  else if( RWFlag == 3 ){
    //std::cout<<"ReadLIDAFilter send CanclePipeline signal \n";
    return NULL;
  }


}
#endif // end #ifdef _DDA_PP

#endif // _DDA_DEFAULT


/*
 * Global: RShadowStrmNum, 
 *
 */


//! Filter that changes each decimal number to its square.
class LCDAFilter: public tbb::filter {
  public:
    LCDAFilter();
    /*override*/
    void* operator()( void* item );
    //void addWriteToPrecedeTracePA(Trace* TPtr, int LoopIter, long RWAddr);
    //void addReadToPrecedeTracePA(Trace* TPtr, int LoopIter, long RWAddr);
    void doLCDAandUpdatePrecedeTraceWithSinkPCAsKeyPA( class LICDep &LCDep);
    void doLCDAandUpdatePrecedeTraceWithSinkPCAsKey( class LICDep &LCDep);

    // FirstRead: 
    // It keep the latest written-iteration to the same bytes of the same PC.
    // Thread safe.
    //
    void addReadToPrecedeTrace(Trace* TPtr, int LoopIter, long RWAddr)
    {
      //return;
      LSPresentRWAddrID * PresentRIDPtr = TPtr->Present[RPresNum]->RTrace; 
      LSPrecedeRWTrace *PrecedePtr = TPtr->Precede;
      LSPrecedeRWAddrID *PrecedeRIDPtr; 

#ifdef _OVERHEAD_PROF
      LSAddRWToPrecedeTraceTime++; 
#endif
      while( PresentRIDPtr != 0 )
      {
        PrecedeRIDPtr = PrecedePtr->RTrace; 
        // Look up for the <RWMask, PC> of present RTrace.
        while(PrecedeRIDPtr){
          if( (PrecedeRIDPtr->PC == PresentRIDPtr->PC) && (PrecedeRIDPtr->RWMask == PresentRIDPtr->RWMask) ){
            break;
          }
          PrecedeRIDPtr = PrecedeRIDPtr->Next;
        }
        // Find.
        if( PrecedeRIDPtr ){
          PrecedeRIDPtr->RWIter = LoopIter; // Thread Safe.
        }else{
#ifdef _OVERHEAD_PROF
          LSPrecedeTraceMallocTime++; 
#endif
          PrecedeRIDPtr = (LSPrecedeRWAddrID*) malloc(sizeof(LSPrecedeRWAddrID));
          PrecedeRIDPtr->PC = PresentRIDPtr->PC;
          PrecedeRIDPtr->RWMask = PresentRIDPtr->RWMask;
          PrecedeRIDPtr->RWIter = LoopIter;
          PrecedePtr->GlobalReadMask |= PresentRIDPtr->RWMask;
          PrecedeRIDPtr->Next = PrecedePtr->RTrace;
          TPtr->Precede->RTrace = PrecedeRIDPtr;
#ifdef _DEBUGINFO
          //printf("insert Precede->RTrace  Iter = %d addr = %p\n", LoopIter, RWAddr );
#endif
        }
#ifdef _OVERHEAD_PROF
        LSPresentBufFreeTime++;
#endif
        // Add the PresentRIDPtr to LSPresentBuf.
        TPtr->Present[RPresNum]->RTrace = PresentRIDPtr->Next;

        free(PresentRIDPtr); // bug ? 20140905 no.

        PresentRIDPtr = TPtr->Present[RPresNum]->RTrace;
      }

#ifdef _DEBUGINFO
      if( TPtr->Present[RPresNum]->RTrace != 0 )
        printf("error in __addReadToPrecedeTrace \n");
#endif

      return;
    }

    void addWriteToPrecedeTrace(Trace* TPtr, int LoopIter, long RWAddr)
    {
      LSPresentRWAddrID * PresentWIDPtr = TPtr->Present[RPresNum]->WTrace;
      LSPrecedeRWAddrID * WIDPtr; 

      //return ;
#ifdef _OVERHEAD_PROF
      LSAddRWToPrecedeTraceTime++; 
#endif
      while(PresentWIDPtr != 0 ){
        WIDPtr = TPtr->Precede->WTrace;

        // The same PC access the same memory before.
        while( WIDPtr ){
          if( WIDPtr->PC == PresentWIDPtr->PC && WIDPtr->RWMask == PresentWIDPtr->RWMask){
            WIDPtr->RWIter = LoopIter;
            break;
          }
          WIDPtr = WIDPtr->Next;
        }
        if(WIDPtr == 0){
#ifdef _OVERHEAD_PROF
          LSPrecedeTraceMallocTime++; 
#endif
          WIDPtr = (LSPrecedeRWAddrID *) malloc (sizeof(LSPrecedeRWAddrID));
          WIDPtr->Next = TPtr->Precede->WTrace;
          TPtr->Precede->WTrace = WIDPtr;
          TPtr->Precede->GlobalWriteMask |= PresentWIDPtr->RWMask;
          WIDPtr->PC = PresentWIDPtr->PC;
          WIDPtr->RWMask = PresentWIDPtr->RWMask;
          WIDPtr->RWIter = LoopIter;
        }
        // Add PresentWIDPtr to LSPresentRWBuf.
#ifdef _OVERHEAD_PROF
        LSPresentBufFreeTime++;
#endif

        // Add PresentWIDPtr to LSPresentBuf.
        TPtr->Present[RPresNum]->WTrace = PresentWIDPtr->Next;
        //PresentWIDPtr->next = LSPresentRWBuf; 
        //LSPresentRWBuf = PresentWIDPtr;

#if 0
        // free the PresentWIDPtr.
        //CSLSPresentBuf.lock();
        PresentWIDPtr->Next = LSPresentBuf;
        LSPresentBuf = PresentWIDPtr;
        CSLSPresentBuf.unlock();
#endif
        // free the PresentWIDPtr.
        free(PresentWIDPtr); // bug 20140904

#ifdef _DEBUGINFO
        //printf("insert Precede->WTrace  Iter = %d Addr = %p \n", LoopIter, RWAddr );
#endif

        PresentWIDPtr = TPtr->Present[RPresNum]->WTrace;
      }
      // Remove Present[].xx flag.
      TPtr->Present[RPresNum]->GlobalReadMask = 0;
      TPtr->Present[RPresNum]->GlobalWriteMask = 0;
      TPtr->Present[RPresNum]->RWIter = 0;


#ifdef _DEBUGINFO
      if( TPtr->Present[RPresNum]->WTrace != 0 )
        printf("error in __addReadToPrecedeTrace \n");
#endif
      return;
    }



    // FirstRead: 
    // It keep the latest written-iteration to the same bytes of the same PC.
    // Thread safe.
    //
    void addReadToPrecedeTracePA(Trace* TPtr, int LoopIter, long RWAddr)
    {
      //return;
      LSPresentRWAddrID * PresentRIDPtr = TPtr->Present[RPresNum]->RTrace; 
      LSPrecedeRWTrace *PrecedePtr = TPtr->Precede;
      LSPrecedeRWAddrID *PrecedeRIDPtr; 

#ifdef _OVERHEAD_PROF
      LSAddRWToPrecedeTraceTime++; 
#endif
      while( PresentRIDPtr != 0 )
      {
        PrecedeRIDPtr = PrecedePtr->RTrace; 
        // Look up for the <RWMask, PC> of present RTrace.
        while(PrecedeRIDPtr){
          if( (PrecedeRIDPtr->PC == PresentRIDPtr->PC) && (PrecedeRIDPtr->RWMask == PresentRIDPtr->RWMask) ){
            break;
          }
          PrecedeRIDPtr = PrecedeRIDPtr->Next;
        }
        // Find.
        if( PrecedeRIDPtr ){
          PrecedeRIDPtr->RWIter = LoopIter; // Thread Safe.
        }else{
#ifdef _OVERHEAD_PROF
          LSPrecedeTraceMallocTime++; 
#endif
          PrecedeRIDPtr = (LSPrecedeRWAddrID*) malloc(sizeof(LSPrecedeRWAddrID));
          PrecedeRIDPtr->PC = PresentRIDPtr->PC;
          PrecedeRIDPtr->RWMask = PresentRIDPtr->RWMask;
          PrecedeRIDPtr->RWIter = LoopIter;
          PrecedePtr->GlobalReadMask |= PresentRIDPtr->RWMask;
          PrecedeRIDPtr->Next = PrecedePtr->RTrace;
          TPtr->Precede->RTrace = PrecedeRIDPtr;
#ifdef _DEBUGINFO
          //printf("insert Precede->RTrace  Iter = %d addr = %p\n", LoopIter, RWAddr );
#endif
        }
#ifdef _OVERHEAD_PROF
        LSPresentBufFreeTime++;
#endif
        // Add the PresentRIDPtr to LSPresentBuf.
        TPtr->Present[RPresNum]->RTrace = PresentRIDPtr->Next;

#if 0
        CSLSPresentBuf.lock();
        PresentRIDPtr->Next = LSPresentBuf;
        LSPresentBuf = PresentRIDPtr;
        CSLSPresentBuf.unlock();
#endif
        free(PresentRIDPtr); // bug ? 20140905 no.

        PresentRIDPtr = TPtr->Present[RPresNum]->RTrace;
      }
#ifdef _DEBUGINFO
      if( TPtr->Present[RPresNum]->RTrace != 0 )
        printf("error in __addReadToPrecedeTrace \n");
#endif

      return;
    }

    void addWriteToPrecedeTracePA(Trace* TPtr, int LoopIter, long RWAddr)
    {
      LSPresentRWAddrID * PresentWIDPtr = TPtr->Present[RPresNum]->WTrace;
      LSPrecedeRWAddrID * WIDPtr; 

      //return ;
#ifdef _OVERHEAD_PROF
      LSAddRWToPrecedeTraceTime++; 
#endif
      while(PresentWIDPtr != 0 ){
        WIDPtr = TPtr->Precede->WTrace;

        // The same PC access the same memory before.
        while( WIDPtr ){
          if( WIDPtr->PC == PresentWIDPtr->PC && WIDPtr->RWMask == PresentWIDPtr->RWMask){
            WIDPtr->RWIter = LoopIter;
            break;
          }
          WIDPtr = WIDPtr->Next;
        }
        if(WIDPtr == 0){
#ifdef _OVERHEAD_PROF
          LSPrecedeTraceMallocTime++; 
#endif
          WIDPtr = (LSPrecedeRWAddrID *) malloc (sizeof(LSPrecedeRWAddrID));
          WIDPtr->Next = TPtr->Precede->WTrace;
          TPtr->Precede->WTrace = WIDPtr;
          TPtr->Precede->GlobalWriteMask |= PresentWIDPtr->RWMask;
          WIDPtr->PC = PresentWIDPtr->PC;
          WIDPtr->RWMask = PresentWIDPtr->RWMask;
          WIDPtr->RWIter = LoopIter;
        }
        // Add PresentWIDPtr to LSPresentRWBuf.
#ifdef _OVERHEAD_PROF
        LSPresentBufFreeTime++;
#endif

        // Add PresentWIDPtr to LSPresentBuf.
        TPtr->Present[RPresNum]->WTrace = PresentWIDPtr->Next;
        //PresentWIDPtr->next = LSPresentRWBuf; 
        //LSPresentRWBuf = PresentWIDPtr;

#if 0
        // free the PresentWIDPtr.
        //CSLSPresentBuf.lock();
        PresentWIDPtr->Next = LSPresentBuf;
        LSPresentBuf = PresentWIDPtr;
        CSLSPresentBuf.unlock();
#endif
        // free the PresentWIDPtr.
        free(PresentWIDPtr); // bug 20140904

#ifdef _DEBUGINFO
        //printf("insert Precede->WTrace  Iter = %d Addr = %p \n", LoopIter, RWAddr );
#endif

        PresentWIDPtr = TPtr->Present[RPresNum]->WTrace;
      }

#ifdef _DEBUGINFO
      if( TPtr->Present[RPresNum]->WTrace != 0 )
        printf("error in __addReadToPrecedeTrace \n");
#endif
      return;
    }

  private:
    //int RPresNum; // Label the PresentTable to be read.
    std::map<long,std::set<SrcPCDist> > HLCTDep;
    std::map<long,std::set<SrcPCDist> > HLCADep;
    std::map<long,std::set<SrcPCDist> > HLCODep;
    std::map<int,int> LoopIterPresNum;
    int FinishedLoopIter; // the NumTh of finished loop iter.
    int PendLoopIter; // Number of loop iter.
    int EndLoopIter;

};

LCDAFilter::LCDAFilter() : 
  filter(serial_in_order),
  FinishedLoopIter(0),
  PendLoopIter(0),
  EndLoopIter(0)
  //tbb::filter(parallel) 
{
  //RPresNum = 0;
}  


/*
 * 1) Analyze Loop carried dependence;
 * 2) Add the present trace table into the precede trace table.
 * 3) Store the data-dependence results in ShadowAdd(SinkPC)-pointed memory.
 */
  inline
void LCDAFilter::doLCDAandUpdatePrecedeTraceWithSinkPCAsKey( class LICDep &input )
{

  int LoopIter = input.LoopIter; 
  // printf("doLCDA: input.LCDAAddrInterval = %d LoopIter = %d\n", input.LCDAAddrInterval.size(), LoopIter );

  //long Min[3], Max[3];
  long Beg, End, SrcPC, SinkPC;
  Trace *TracePtr;
  std::map<long, std::set<SrcPCDist> >&LCTDep = input.LCTDep;
  std::map<long, std::set<SrcPCDist> >&LCADep = input.LCADep;
  std::map<long, std::set<SrcPCDist> >&LCODep = input.LCODep;

  std::map<long, std::set<SrcPCDist> >::iterator DepBeg, DepEnd;
  std::set<SrcPCDist>::iterator SrcPCBeg;

  LSPresentRWAddrID * PresentRIDPtr, *PresentWIDPtr;
  LSPrecedeRWAddrID * PrecedeWIDPtr, *PrecedeRIDPtr;

  std::interval_set<long>::iterator beg, end;
  //fprintf(stderr, "LCDA.size = %d \n", input.LCDAAddrInterval.size() );
  for(beg = input.LCDAAddrInterval.begin(), end = input.LCDAAddrInterval.end(); beg != end; beg++){

    Beg = beg.interval().first;
    End = beg.interval().second;
    if( Beg == 0 ) continue;
    for( ; Beg <= End; Beg += 8 ){
      TracePtr = *((Trace**)(Beg)); 
      if ( 0 == TracePtr || 0 == TracePtr->Present[RPresNum])   {
        continue;
      }
      // Not need?
      //if ( (TracePtr->Present[RPresNum]->RTrace == 0) && (TracePtr->Present[RPresNum]->WTrace == 0) )
      //  continue;

      // Not need?
      //if( TracePtr->Present[RPresNum]->RWIter != LoopIter )// Bug?
      //  continue;

      unsigned char ReadMask = 0;
      PresentRIDPtr = TracePtr->Present[RPresNum]->RTrace;

      // 1. Analyzing LCTD.
      if( PresentRIDPtr != 0 ){
        PrecedeWIDPtr = TracePtr->Precede->WTrace;
        if ( PrecedeWIDPtr != 0 ){
          // Write->Read: Have overlap bytes.
          if( TracePtr->Present[RPresNum]->GlobalReadMask & TracePtr->Precede->GlobalWriteMask ){
            while( PresentRIDPtr != 0 ){
              ReadMask = PresentRIDPtr->RWMask;
              // Iterate over Preced-write-trace. 
              for( PrecedeWIDPtr = TracePtr->Precede->WTrace ; 
                  PrecedeWIDPtr != 0; PrecedeWIDPtr = PrecedeWIDPtr->Next){
                // Exist True dep.
                if( ReadMask & PrecedeWIDPtr->RWMask ) {
                  int LCDDist = LoopIter - PrecedeWIDPtr->RWIter;
                  // Store true deps. Todo....
                  // (long)PrecedeWIDPtr->PC ===> (long)PresentRIDPtr->PC
                  SrcPC = (long)PrecedeWIDPtr->PC;
                  SinkPC = (long)PresentRIDPtr->PC;
#ifdef _DEBUGINFO_LCDA
                  //std::cout<< std::setbase(16) << " doLCDAandUpdatePrecedeTraceWithSinkPCAsKeyPA LCTDep SinkPC = " << SinkPC  << ", SrcPC = " << SrcPC << ", Dist = " << LCDDist << "\n";
#endif
                  // 1.1) Store LCTD.
                  SrcPCDist SPCD = {SrcPC, LCDDist};
                  bool inH = 0;

                  // Happens before not insert LCDep.
                  DepBeg = HLCTDep.find( SinkPC ); 
                  if( DepBeg !=  HLCODep.end() ){
                    // SrcPC -> SinkPC exists.
                    std::set<SrcPCDist> &SrcPCRef = DepBeg->second;
                    SrcPCBeg = SrcPCRef.find( SPCD );
                    if( SrcPCBeg != SrcPCRef.end() ){
                      if( SrcPCBeg->LCDDist == LCDDist )
                        inH = 1;
                    }
                  }
                  // 1.1.1) Having some deps.
                  if(!inH){
                    DepBeg = LCTDep.find( SinkPC ); 
                    if( DepBeg !=  LCTDep.end() ){
                      // SrcPC -> SinkPC exists.
                      std::set<SrcPCDist> &SrcPCRef = DepBeg->second;
                      SrcPCBeg = SrcPCRef.find( SPCD );
                      if( SrcPCBeg != SrcPCRef.end() ){
                        if( SrcPCBeg->LCDDist > LCDDist )
                          SrcPCRef.erase(SrcPCBeg);
                        SrcPCRef.insert(SPCD);
                      }
                      // New SrcPC: SrcPC -> SinkPC.
                      else{
                        SrcPCRef.insert(SPCD);
                      }
                    }
                    else{
                      std::set<SrcPCDist> tset;
                      tset.insert(SPCD);
                      LCTDep.insert(std::pair<long, std::set<SrcPCDist> >(SinkPC, tset )); 
                    } 
                  }// end Store LCTD.
                } //
              } // Iterate over Precede[].WTrace.
              PresentRIDPtr = PresentRIDPtr->Next;
            } // Iterate over Present[].RTrace.
          } 
        }
      }


      // 2. Anzlyzing LCOD.
      PresentWIDPtr = TracePtr->Present[RPresNum]->WTrace;
      if( PresentWIDPtr != 0 ){
        PrecedeWIDPtr = TracePtr->Precede->WTrace;
        if ( PrecedeWIDPtr != 0 ){
          // Write->Read: Have overlap bytes.
          if( TracePtr->Present[RPresNum]->GlobalWriteMask & TracePtr->Precede->GlobalWriteMask ){
            for( ; PresentWIDPtr != 0; PresentWIDPtr = PresentWIDPtr->Next){
              ReadMask = PresentWIDPtr->RWMask;
              // Iterate over Preced-write-trace. 
              for(PrecedeWIDPtr = TracePtr->Precede->WTrace; PrecedeWIDPtr != 0; 
                  PrecedeWIDPtr = PrecedeWIDPtr->Next ){
                // Exist Out dep.
                if( ReadMask & PrecedeWIDPtr->RWMask ) {
                  int LCDDist = LoopIter - PrecedeWIDPtr->RWIter;
                  // (long)PrecedeWIDPtr->PC ===> (long) PresentWIDPtr->PC 
                  SrcPC = (long)PrecedeWIDPtr->PC;
                  SinkPC = (long)PresentWIDPtr->PC;
#ifdef _DEBUGINFO_LCDA
                  std::cout<< " doLCDAandUpdatePrecedeTraceWithSinkPCAsKey LCODep SinkPC = " << SinkPC  << ", SrcPC = " << SrcPC << ", Dist = " << LCDDist  <<"\n";
#endif
                  // 1.1) Store LCOD.
#if 1
                  SrcPCDist SPCD = {SrcPC, LCDDist};
                  bool inH = 0;

                  // Happens before not insert LCDep.
                  DepBeg = HLCODep.find( SinkPC ); 
                  if( DepBeg !=  HLCODep.end() ){
                    // SrcPC -> SinkPC exists.
                    std::set<SrcPCDist> &SrcPCRef = DepBeg->second;
                    SrcPCBeg = SrcPCRef.find( SPCD );
                    if( SrcPCBeg != SrcPCRef.end() ){
                      if( SrcPCBeg->LCDDist == LCDDist )
                        inH = 1;
                    }
                  }

                  // 1.1.1) Having some deps.
                  if(!inH){
                    DepBeg = LCODep.find( SinkPC ); 
                    if( DepBeg != LCODep.end() ){
                      std::set<SrcPCDist> &SrcPCRef = DepBeg->second;
                      SrcPCBeg = SrcPCRef.find(SPCD); 
                      if( SrcPCBeg != SrcPCRef.end() ){
                        if( SrcPCBeg->LCDDist > LCDDist )
                          SrcPCRef.erase(SrcPCBeg);
                        SrcPCRef.insert(SPCD);
                      }else{
                        SrcPCRef.insert(SPCD);
                      }     
                    }
                    // 1.1.2) No dep before.
                    else{
                      std::set<SrcPCDist> tset;
                      tset.insert(SPCD);
                      LCODep.insert(std::pair<long, std::set<SrcPCDist> >(SinkPC, tset )); 
                    }
                  }
#endif

                }
              }
            }
          } 
        }
      }

      // 3. Analyzing LCAD.
      PresentWIDPtr = TracePtr->Present[RPresNum]->WTrace;
      if( PresentWIDPtr != 0 ){
        PrecedeRIDPtr = TracePtr->Precede->RTrace;
        if ( PrecedeRIDPtr != 0 ){
          // Write->Read: Have overlap bytes.
          if( TracePtr->Present[RPresNum]->GlobalWriteMask & TracePtr->Precede->GlobalReadMask ){
            for( ; PresentWIDPtr != 0;  PresentWIDPtr = PresentWIDPtr->Next){
              ReadMask = PresentWIDPtr->RWMask;
              // Iterate over Preced-write-trace. 
              for( PrecedeRIDPtr = TracePtr->Precede->RTrace; PrecedeRIDPtr != 0; PrecedeRIDPtr = PrecedeRIDPtr->Next){
                // Exist Anti dep.
                if( ReadMask & PrecedeRIDPtr->RWMask ) {
                  int LCDDist = LoopIter - PrecedeRIDPtr->RWIter;
                  // (long)PrecedeRIDPtr->PC ===> (long) PresentWIDPtr->PC 
                  SrcPC = (long)PrecedeRIDPtr->PC;
                  SinkPC = (long)PresentWIDPtr->PC;
                  //std::cout<< " doLCDA LCADep SinkPC = " << SinkPC <<", SrcPC = "<< SrcPC<< ", Dist = "<< LCDDist  <<"\n";
                  // 1.1) Store LCAD.
#if 1
                  SrcPCDist SPCD = {SrcPC, LCDDist};
                  bool inH = 0;

                  // Happens before not insert LCDep.
                  DepBeg = HLCADep.find( SinkPC ); 
                  if( DepBeg !=  HLCADep.end() ){
                    // SrcPC -> SinkPC exists.
                    std::set<SrcPCDist> &SrcPCRef = DepBeg->second;
                    SrcPCBeg = SrcPCRef.find( SPCD );
                    if( SrcPCBeg != SrcPCRef.end() ){
                      if( SrcPCBeg->LCDDist == LCDDist )
                        inH = 1;
                    }
                  }


                  // 1.1.1) Having some deps.
                  if(!inH){
                    DepBeg = LCADep.find( SinkPC ); 
                    if( DepBeg != LCADep.end() ){
                      std::set<SrcPCDist> &SrcPCRef = DepBeg->second;
                      SrcPCBeg = SrcPCRef.find(SPCD); 
                      if( SrcPCBeg != SrcPCRef.end() ){
                        if( SrcPCBeg->LCDDist > LCDDist ){
                          SrcPCRef.erase(SrcPCBeg);
                          SrcPCRef.insert(SPCD);
                        }
                      }else{
                        SrcPCRef.insert(SPCD);
                      }     
                    }
                    // 1.1.2) No dep before.
                    else{
                      std::set<SrcPCDist> tset;
                      tset.insert(SPCD);
                      LCADep.insert(std::pair<long, std::set<SrcPCDist> >(SinkPC, tset )); 
                    }
                  }
#endif

                }
              }
            }
          } 
        }
      }

      addReadToPrecedeTrace( TracePtr, LoopIter, Beg ); 
      addWriteToPrecedeTrace( TracePtr, LoopIter, Beg );

    }
  }
  //  RBClear(LDDRWTrace, LDDRWTrace->Root);
  return ;
}




/*
 * 1) Analyze Loop carried dependence;
 * 2) Add the present trace table into the precede trace table.
 * 3) Store the data-dependence results in ShadowAdd(SinkPC)-pointed memory.
 */
  inline
void LCDAFilter::doLCDAandUpdatePrecedeTraceWithSinkPCAsKeyPA( class LICDep &input )
{

  int LoopIter = input.LoopIter; 
  //printf("doLCDA: input.LCDAAddrInterval = %d LoopIter = %d\n", input.LCDAAddrInterval.size(), LoopIter );

  //long Min[3], Max[3];
  long Beg, End;
  long SrcPC, SinkPC;
  Trace *TracePtr;
  //std::map<long, std::set<SrcPCDist> >&LCTDep = input.LCTDep;
  //std::map<long, std::set<SrcPCDist> >&LCADep = input.LCADep;
  //std::map<long, std::set<SrcPCDist> >&LCODep = input.LCODep;
  std::map<long, std::set<SrcPCDist> >::iterator DepBeg;
  std::map<long, int>::iterator SrcPCBeg;

  LSPresentRWAddrID * PresentRIDPtr, *PresentWIDPtr;
  LSPrecedeRWAddrID * PrecedeWIDPtr, *PrecedeRIDPtr;
#if 0
  Min[0] = input.LCDAStackMin;
  Max[0] = input.LCDAStackMax;
  Min[1] = input.LCDAHeapMin;
  Max[1] = input.LCDAHeapMax;
  Min[2] = input.LCDAGlobalMin;
  Max[2] = input.LCDAGlobalMax;
#endif

#if 0
  if(-1 == __check_interval_overlap(input.LCDAAddrInterval))
    std::cout<<" There are overlap \n";
#endif

  std::interval_set<long>::iterator beg, end;
  //fprintf(stderr, "LCDA.size = %d \n", input.LCDAAddrInterval.size() );
  for(beg = input.LCDAAddrInterval.begin(), end = input.LCDAAddrInterval.end(); beg != end; beg++){

    //std::pair<long, long> __range = beg.interval();
    //std::cout<<"first = " << __range.first <<", second = " << __range.second << std::endl;
    //for( i = 0; i < 3; i++ ){
    //
    Beg = beg.interval().first;
    End = beg.interval().second;
    if( Beg == 0 ) continue;
    for( ; Beg <= End; Beg += 8 ){
      TracePtr = *((Trace**)(Beg)); 

      //printf("doLCDA: Addr = %p LoopIter = %d\n", Beg, LoopIter);

      // Iterate the present trace, and analyze the LCD and add Present to  Precede trace.
      //  No Access within current iteration in the Present Trace.
      // if( 0 == TracePtr )
      //   continue;

      // c11 standard.
      //std::cerr<<std::setbase(10) <<"RPresNum = " << RPresNum << "\n";
      if ( 0 == TracePtr->Present[RPresNum])   {
        continue;
      }
      // Not need?
      //if ( (TracePtr->Present[RPresNum]->RTrace == 0) && (TracePtr->Present[RPresNum]->WTrace == 0) )
      //  continue;

      // Not need?
      //if( TracePtr->Present[RPresNum]->RWIter != LoopIter )// Bug?
      //  continue;

      unsigned char ReadMask = 0;
      //LSDepInfo **DepShadowPtr;
      //LSDepInfo *LCDepPtr;
      PresentRIDPtr = TracePtr->Present[RPresNum]->RTrace;

      // 1. Analyzing LCTD.
      if( PresentRIDPtr != 0 ){
        PrecedeWIDPtr = TracePtr->Precede->WTrace;
        if ( PrecedeWIDPtr != 0 ){
          // Write->Read: Have overlap bytes.
          if( TracePtr->Present[RPresNum]->GlobalReadMask & TracePtr->Precede->GlobalWriteMask ){
            for( ; PresentRIDPtr != 0;  PresentRIDPtr = PresentRIDPtr->Next){
              ReadMask = PresentRIDPtr->RWMask;
              // Iterate over Preced-write-trace. 
              for( PrecedeWIDPtr = TracePtr->Precede->WTrace ; 
                  PrecedeWIDPtr != 0; PrecedeWIDPtr = PrecedeWIDPtr->Next){
                // Exist True dep.
                if( ReadMask & PrecedeWIDPtr->RWMask ) {
                  //int LCDDist = LoopIter - PrecedeWIDPtr->RWIter;
                  // Store true deps. Todo....
                  // (long)PrecedeWIDPtr->PC ===> (long)PresentRIDPtr->PC
                  SrcPC = (long)PrecedeWIDPtr->PC;
                  SinkPC = (long)PresentRIDPtr->PC;
                  //          std::cout<< std::setbase(16) << " doLCDA LCTDep SinkPC = " << SinkPC  << ", SrcPC = " << SrcPC << ", Dist = " << LCDDist <<", LoopIter =" << LoopIter << "PrecedeIter = " << PrecedeWIDPtr->RWIter  << "\n";
#ifdef _DEBUGINFO_LCDA
                  //std::cout<< std::setbase(16) << " doLCDAandUpdatePrecedeTraceWithSinkPCAsKeyPA LCTDep SinkPC = " << SinkPC  << ", SrcPC = " << SrcPC << ", Dist = " << LCDDist << "\n";
#endif
                  // 1.1) Store LCTD.
#if 0
                  DepBeg = LCTDep.find( SinkPC ); 
                  // 1.1.1) Having some deps.
                  if( DepBeg != LCTDep.end() ){
                    std::map<long, int> &SrcPCDep = LCTDep[SinkPC]; 
                    SrcPCBeg = SrcPCDep.find(SrcPC); 
                    if( SrcPCBeg != SrcPCDep.end() ){
                      if( SrcPCDep[SrcPC] > LCDDist )
                        SrcPCDep[SrcPC] = LCDDist;
                    }else{
                      SrcPCDep.insert(std::pair<long, int>(SrcPC, LCDDist)); 
                    }     
                  }
                  // 1.1.2) No dep before.
                  else{
                    std::map<long, int> mp1; 
                    mp1.insert(std::pair<long, int>(SrcPC, LCDDist));
                    LCTDep.insert(std::pair<long, std::map<long, int> > (SinkPC, mp1) );
                  }
#endif

                }
              }
            }
          } 
        }
      }


      // 2. Anzlyzing LCOD.
      PresentWIDPtr = TracePtr->Present[RPresNum]->WTrace;
      if( PresentWIDPtr != 0 ){
        PrecedeWIDPtr = TracePtr->Precede->WTrace;
        if ( PrecedeWIDPtr != 0 ){
          // Write->Read: Have overlap bytes.
          if( TracePtr->Present[RPresNum]->GlobalWriteMask & TracePtr->Precede->GlobalWriteMask ){
            for( ; PresentWIDPtr != 0; PresentWIDPtr = PresentWIDPtr->Next){
              ReadMask = PresentWIDPtr->RWMask;
              // Iterate over Preced-write-trace. 
              for(PrecedeWIDPtr = TracePtr->Precede->WTrace; PrecedeWIDPtr != 0; 
                  PrecedeWIDPtr = PrecedeWIDPtr->Next ){
                // Exist Out dep.
                if( ReadMask & PrecedeWIDPtr->RWMask ) {
                  //int LCDDist = LoopIter - PrecedeWIDPtr->RWIter;
                  // (long)PrecedeWIDPtr->PC ===> (long) PresentWIDPtr->PC 
                  SrcPC = (long)PrecedeWIDPtr->PC;
                  SinkPC = (long)PresentWIDPtr->PC;
                  //std::cout<< " doLCDA LCODep SinkPC = " << SinkPC  << ", SrcPC = " << SrcPC << ", Dist = " << LCDDist  <<"\n";
#ifdef _DEBUGINFO
                  //std::cout<< " doLCDAandUpdatePrecedeTraceWithSinkPCAsKeyPA LCODep SinkPC = " << SinkPC  << ", SrcPC = " << SrcPC << ", Dist = " << LCDDist  <<"\n";
#endif
                  // 1.1) Store LCOD.
#if 0
                  DepBeg = LCODep.find( SinkPC ); 
                  // 1.1.1) Having some deps.
                  if( DepBeg != LCODep.end() ){
                    std::map<long, int> &SrcPCDep = LCODep[SinkPC]; 
                    SrcPCBeg = SrcPCDep.find(SrcPC); 
                    if( SrcPCBeg != SrcPCDep.end() ){
                      if( SrcPCDep[SrcPC] > LCDDist )
                        SrcPCDep[SrcPC] = LCDDist;
                    }else{
                      SrcPCDep.insert(std::pair<long, int>(SrcPC, LCDDist)); 
                    }     
                  }
                  // 1.1.2) No dep before.
                  else{
                    std::map<long, int> mp1; 
                    mp1.insert(std::pair<long, int>(SrcPC, LCDDist));
                    LCODep.insert(std::pair<long, std::map<long, int> >(SinkPC, mp1) );
                  }
#endif

                }
              }
            }
          } 
        }
      }

      // 3. Analyzing LCAD.
      PresentWIDPtr = TracePtr->Present[RPresNum]->WTrace;
      if( PresentWIDPtr != 0 ){
        PrecedeRIDPtr = TracePtr->Precede->RTrace;
        if ( PrecedeRIDPtr != 0 ){
          // Write->Read: Have overlap bytes.
          if( TracePtr->Present[RPresNum]->GlobalWriteMask & TracePtr->Precede->GlobalReadMask ){
            for( ; PresentWIDPtr != 0;  PresentWIDPtr = PresentWIDPtr->Next){
              ReadMask = PresentWIDPtr->RWMask;
              // Iterate over Preced-write-trace. 
              for( PrecedeRIDPtr = TracePtr->Precede->RTrace; PrecedeRIDPtr != 0; PrecedeRIDPtr = PrecedeRIDPtr->Next){
                // Exist Anti dep.
                if( ReadMask & PrecedeRIDPtr->RWMask ) {
                  //int LCDDist = LoopIter - PrecedeRIDPtr->RWIter;
                  // (long)PrecedeRIDPtr->PC ===> (long) PresentWIDPtr->PC 
                  SrcPC = (long)PrecedeRIDPtr->PC;
                  SinkPC = (long)PresentWIDPtr->PC;
                  //std::cout<< " doLCDA LCADep SinkPC = " << SinkPC <<", SrcPC = "<< SrcPC<< ", Dist = "<< LCDDist  <<"\n";
                  // 1.1) Store LCAD.
#if 0
                  DepBeg = LCADep.find( SinkPC ); 
                  // 1.1.1) Having some deps.
                  if( DepBeg != LCADep.end() ){
                    std::map<long, int> &SrcPCDep = LCODep[SinkPC]; 
                    SrcPCBeg = SrcPCDep.find(SrcPC); 
                    if( SrcPCBeg != SrcPCDep.end() ){
                      if( SrcPCDep[SrcPC] > LCDDist )
                        SrcPCDep[SrcPC] = LCDDist;
                    }else{
                      SrcPCDep.insert(std::pair<long, int>(SrcPC, LCDDist)); 
                    }     
                  }
                  // 1.1.2) No dep before.
                  else{
                    std::map<long, int> mp1; 
                    mp1.insert(std::pair<long, int>(SrcPC, LCDDist));
                    LCADep.insert(std::pair<long, std::map<long, int> >(SinkPC, mp1) );
                  }

#endif

                }
              }
            }
          } 
        }
      }

      addReadToPrecedeTracePA( TracePtr, LoopIter, Beg ); 
      addWriteToPrecedeTracePA( TracePtr, LoopIter, Beg );

    }
  }
  //  RBClear(LDDRWTrace, LDDRWTrace->Root);
  return ;
  }




  /* Do LCDA.
   * 
   */
  void* LCDAFilter::operator()( void* item ) 
  {
    if( !item ){
      //std::cout<<"LCDAFilter NULL operator \n";
      return 0;
    }

#ifdef _DDA_DEFAULT_PP_PARLIDA

#ifdef _TIME_PROF
    tbb::tick_count tstart, pststart;
    pststart = tbb::tick_count::now();
#endif
    int i;
    int LoopIter, CurPresNum;
    class LICDep **ProcInput, *input;
    ProcInput = static_cast<class LICDep**>(item);
    for(i = 0; i < PROCNUM; ++i){ 
      if( ProcInput[i] == 0 ) continue;
      input = ProcInput[i];
      //class LICDep& input = *static_cast<class LICDep**>(item[i]);
      if( input->StartLCDA  ){
        LoopIter = input->LoopIter;
        CurPresNum = input->CurPresNum;
        LoopIterPresNum[LoopIter] = CurPresNum; 
        std::map<int, int>::iterator beg, end;
        end = LoopIterPresNum.end();
        //std::cout<<"LCDAFilter::operator doLCDA LoopIter " << LoopIter <<", Finish " << FinishedLoopIter  <<"\n";
        //if( LoopIter == (FinishedLoopIter+1) )
        if( LoopIter == (FinishedLoopIter+1) ) {
          for( beg = LoopIterPresNum.find(LoopIter);  beg != end;   )
          { 
            //std::cout<<"LCDAFilter::operator doLCDA LoopIter " << LoopIter <<"\n";
            RPresNum = LoopIterPresNum[LoopIter];

            FinishedLoopIter = LoopIter;
            LoopIter++;
            if( LoopIterPresNum.size() )
              beg = LoopIterPresNum.find(LoopIter);
            else
              beg = end;


            //std::cout<<"within LCDAFilter myself = " << pthread_self() << "\n"; 

#ifdef _TIME_PROF 
            tstart = tbb::tick_count::now();
#endif
            // not need.
            while( !PresIsFull[RPresNum] );

#ifdef _TIME_PROF 
            LCDACTime += (tbb::tick_count::now() - tstart).seconds();
#endif

#ifdef __DOALL_ANALYSIS
            doLCDAandUpdatePrecedeTraceWithSinkPCAsKeyPA( *input );
#else
            doLCDAandUpdatePrecedeTraceWithSinkPCAsKey( *input );
#endif
            // Done by one thread. 
            PresIsFull[RPresNum] = 0; // Free the Present[RPresNum].
#ifdef _DEBUGINFO_LCDA
            fprintf(stderr,"LCDAFilter::operator: PresIsFull[%d] = 0, RPresNum = %d \n", RPresNum, (RPresNum+1)%PRESBUFNUM );
#endif
            //RPresNum = (RPresNum+1) % PRESBUFNUM;
#ifdef _DEBUGINFO_LIDA
            //fprintf(stdout,"LCDAFilter::operator:  RPresNum = %d \n", RPresNum );
#endif
          }
        }
      }
      else if( input->EndLoopIter ){
        
      }
#if 0
      if( input.CancelPipeline ){
        std::cout<<"LCDAFilter CancelPipeline \n"; 
      }
#endif

    }
#ifdef _TIME_PROF 
      LCDATime += (tbb::tick_count::now() - pststart).seconds();
#endif
    return ProcInput;
#else // no _DDA_PP
    class LICDep& input = *static_cast<class LICDep*>(item);

    //std::cout<<"within LCDAFilter myself = " << pthread_self() << "\n"; 
#ifdef _TIME_PROF
    tbb::tick_count tstart, pststart;
    pststart = tbb::tick_count::now();
#endif

    if( input.StartLCDA ){
      //std::cout<<"within LCDAFilter myself = " << pthread_self() << "\n"; 

#ifdef _TIME_PROF 
      tstart = tbb::tick_count::now();
#endif
      // not need.
      while( !PresIsFull[RPresNum] );
#ifdef _TIME_PROF 
      LCDACTime += (tbb::tick_count::now() - tstart).seconds();
#endif

#ifdef __DOALL_ANALYSIS
      doLCDAandUpdatePrecedeTraceWithSinkPCAsKeyPA( input );
#else
      doLCDAandUpdatePrecedeTraceWithSinkPCAsKey( input );
#endif
      // Done by one thread. 
      PresIsFull[RPresNum] = 0; // Free the Present[RPresNum].
#ifdef _DEBUGINFO_LCDA
      fprintf(stderr,"LCDAFilter::operator: PresIsFull[%d] = 0, RPresNum = %d \n", RPresNum, (RPresNum+1)%PRESBUFNUM );
#endif
      RPresNum = (RPresNum+1) % PRESBUFNUM;
#ifdef _DEBUGINFO_LIDA
      //fprintf(stdout,"LCDAFilter::operator:  RPresNum = %d \n", RPresNum );
#endif
    }

    if( input.CancelPipeline ){
      std::cout<<"LCDAFilter CancelPipeline \n"; 
    }

#ifdef _TIME_PROF 
    LCDATime += (tbb::tick_count::now() - pststart).seconds();
#endif

    return &input;
#endif
  }

  //! Filter that writes each buffer to a file.
  class OutputLCDepFilter: public tbb::filter {
    public:


      long LSDepSinkPCMin, LSDepSinkPCMax; // DepSinkPC-scope. Thread safe?
      OutputLCDepFilter(  );

      void OutputLITDep(class LICDep &LIDep ); 
      void OutputLIAODep( class LICDep &LIDep); 
      void OutputLCTDep( class LICDep &LCDep); 
      void OutputLCADep( class LICDep &LCDep); 
      void OutputLCODep( class LICDep &LCDep); 
      /*override*/
      void* operator()( void* item );
  };

  OutputLCDepFilter::OutputLCDepFilter(  ) : 
    tbb::filter(serial_in_order),
    LSDepSinkPCMin(LDDGlobalMin),
    LSDepSinkPCMax(0x6cf000000000)
  {
  }

  /*
   *
   */
  void OutputLCDepFilter::OutputLITDep( class LICDep &LIDep )  
  {
#if 1
    std::map<long, std::set<long> > &LITDep = LIDep.LITDep;
    std::map<long, std::set<long> >::iterator DepBeg, DepEnd;
    std::set<long>::iterator SrcBeg, SrcEnd;
#endif
#if 0
    concurrent_unordered_map<long, concurrent_unordered_set<long> > &LITDep = LIDep.LITDep;
    concurrent_unordered_map<long, concurrent_unordered_set<long> >::iterator DepBeg, DepEnd;
    concurrent_unordered_set<long>::iterator SrcBeg, SrcEnd;
#endif
    long SrcPC, SinkPC;
    //std::cout<<"Eneter OutputLCDepFilter::OutputLITDep\n";
    if( LITDep.empty() ){
#ifdef _DEBUGINFO
      std::cout<<"OutputLCDepFilter LITDep No LITDep Info \n";
#endif
      return;
    }
    // 1) Output LITDep.
    for( DepBeg = LITDep.begin(), DepEnd = LITDep.end(); DepBeg != DepEnd; ++DepBeg ){
      SinkPC = DepBeg->first;  

      // 2) Setup Dep buf.
      LSDepInfo **DepShadowPtr;
      LSDepInfo *LIDepPtr;
      DepShadowPtr = (LSDepInfo**) AppAddrToShadow((long)SinkPC);

      if( (long)DepShadowPtr > LSDepSinkPCMax )
        LSDepSinkPCMax = (long)DepShadowPtr;
      if( (long)DepShadowPtr < LSDepSinkPCMin)
        LSDepSinkPCMin = (long) DepShadowPtr;


      // 2.1) The First time to Insert LIDep info. 
      if( *DepShadowPtr == 0 ){
#ifdef _OVERHEAD_PROF
        LSPCShadowTime++;  // x * ( 8 + );
        LIDepInfoMallocTime++;
#endif
        LIDepPtr = (LSDepInfo*) malloc(sizeof(LSDepInfo));
        *DepShadowPtr = LIDepPtr;
        LIDepPtr->TrueDep = 0;
        LIDepPtr->OutDep = 0;
        LIDepPtr->AntiDep = 0;
        // bug? Not thread safe.
#if 0
        if( (long)DepShadowPtr > LSLIDepSinkPCMax )
          LSLIDepSinkPCMax = (long)DepShadowPtr;
        else if( (long)DepShadowPtr < LSLIDepSinkPCMin )
          LSLIDepSinkPCMin =(long) DepShadowPtr;
#endif

      } 
      // 2.2) Has inserted LIDep info before.
      else
        LIDepPtr = *DepShadowPtr;

      // 3) Insert LIDep into the SinkPC-Shadow-Space.
      for( SrcBeg = DepBeg->second.begin(), SrcEnd = DepBeg->second.end(); SrcBeg != DepBeg->second.end(); ++SrcBeg){
        SrcPC = *SrcBeg;
        LSDep *LIDepTrue = LIDepPtr->TrueDep; //TraceRef.LIDep->TrueDep; 
        while( LIDepTrue != 0 ){
          if(LIDepTrue->SrcPC == SrcPC && LIDepTrue->SinkPC == (long)SinkPC){
            if( LIDepTrue->Dist[0] != 0 ) // need this?
              LIDepTrue->Dist[0] = 0;
#ifdef _DEBUGINFO
            std::cout<<"OutputLCDepFilter LITDep SinkPC = "<< SinkPC << ", SrcPC = "<< SrcPC << " \n";
#endif
            break;
          }
          LIDepTrue = LIDepTrue->next;
        }
        // Not find in LID set, add the new LID-True.
        if( LIDepTrue == 0 ){
#ifdef _OVERHEAD_PROF
          LIDepMallocTime++;
#endif
          LIDepTrue = (LSDep*) malloc( sizeof(LSDep));
          LIDepTrue->next =  LIDepPtr->TrueDep;
          LIDepPtr->TrueDep = LIDepTrue; 
          LIDepTrue->SrcPC =(long) SrcPC;
          LIDepTrue->SinkPC =(long) SinkPC;
          LIDepTrue->Dist[0] = LIDepTrue->Dist[1] = 0;
#ifdef _DEBUGINFO
          std::cout<<"OutputLCDepFilter LITDep SinkPC = "<< SinkPC << ", SrcPC = "<< SrcPC << " \n";
#endif
        }
      }
    } 

    //std::cout<<"LSDepSinkPCMin "  << LSDepSinkPCMin<<", LSDepSinkPCMax = "<< LSDepSinkPCMax <<"\n";
    return;
  }

  // Read/Write -> Write
  void OutputLCDepFilter::OutputLIAODep( class LICDep &LIDep )  
  {

#if 1
    std::map<long, std::set<long> > &LIADep = LIDep.LIADep, &LIODep = LIDep.LIODep;
    std::map<long, std::set<long> >::iterator DepBeg, DepEnd;
    std::set<long>::iterator SrcBeg, SrcEnd;
#endif
#if 0
    concurrent_unordered_map<long, concurrent_unordered_set<long> > &LIADep = LIDep.LIADep, &LIODep = LIDep.LIODep;
    concurrent_unordered_map<long, concurrent_unordered_set<long> >::iterator DepBeg, DepEnd;
    concurrent_unordered_set<long>::iterator SrcBeg, SrcEnd;
#endif

    long SrcPC, SinkPC;
    LSDepInfo **DepShadowPtr;
    LSDepInfo *LIDepPtr;

    // 1) output LIODep.
    if( !LIODep.empty() ){
      for( DepBeg = LIODep.begin(), DepEnd = LIODep.end(); DepBeg != DepEnd; ++DepBeg ){
        SinkPC = DepBeg->first;

        // 1.1) Setup SinkPC-Shadow space.  opt: output the tri-dep together. 
        // 1.1.1) The first time to insert LIDep.
        DepShadowPtr = (LSDepInfo **)AppAddrToShadow((long)SinkPC);

        if( (long)DepShadowPtr > LSDepSinkPCMax )
          LSDepSinkPCMax = (long)DepShadowPtr;
        if( (long)DepShadowPtr < LSDepSinkPCMin)
          LSDepSinkPCMin = (long) DepShadowPtr;

        if( *DepShadowPtr == 0 ){
#ifdef _OVERHEAD_PROF
          LSPCShadowTime++;
          LCDepInfoMallocTime++;
#endif
          LIDepPtr = (LSDepInfo*) malloc(sizeof(LSDepInfo));
          *DepShadowPtr = LIDepPtr;
          LIDepPtr->TrueDep = 0;
          LIDepPtr->AntiDep = 0;
          LIDepPtr->OutDep = 0;

#if  0
          if( (long)DepShadowPtr > LSLIDepSinkPCMax )
            LSLIDepSinkPCMax = (long)DepShadowPtr;
          else if( (long)DepShadowPtr < LSLIDepSinkPCMin)
            LSLIDepSinkPCMin = (long) DepShadowPtr;
#endif
        } 
        else 
          LIDepPtr =*DepShadowPtr;

        // 1.2) Insert the LIODep info.
        if( !DepBeg->second.empty() ){
          //std::cout<<"DepBeg->second.size() = " << DepBeg->second.size() << "\n"; 
          for( SrcBeg = DepBeg->second.begin(), SrcEnd = DepEnd->second.end(); SrcBeg != DepBeg->second.end(); ++SrcBeg ){
            // LI-Output.
            SrcPC = *SrcBeg; 
            LSDep* LIDepOut = LIDepPtr->OutDep;
            while( LIDepOut != 0){
              if( LIDepOut->SrcPC == SrcPC && LIDepOut->SinkPC == (long)SinkPC ){
                if( LIDepOut->Dist[0] != 0 )
                  LIDepOut->Dist[0] = 0;
#ifdef _DEBUGINFO
                std::cout<<"OutputLCDepFilter LIODep SinkPC = "<< SinkPC << ", SrcPC = "<< SrcPC << " \n";
#endif
                break;
              }
              LIDepOut = LIDepOut->next;
            }
            // find the node, but the node store LCD only.
            if( LIDepOut == 0 ){
#ifdef _OVERHEAD_PROF
              LIDepMallocTime++;
#endif
              LIDepOut = (LSDep*) malloc( sizeof(LSDep));
              LIDepOut->next = LIDepPtr->OutDep;
              LIDepPtr->OutDep = LIDepOut;
              LIDepOut->SrcPC = SrcPC;
              LIDepOut->SinkPC = (long)SinkPC;
              LIDepOut->Dist[0] = 0;
              LIDepOut->Dist[1] = 0;
#ifdef _DEBUGINFO
              std::cout<<"OutputLCDepFilter LIODep SinkPC = "<< SinkPC << ", SrcPC = "<< SrcPC << " \n";
#endif
            }
          }
        }
      }
    }
    else{
#ifdef _DEBUGINFO
      //std::cout<<"OutputLCDepFilter LIAODep No LIODep Info \n";
#endif

    }

    // 2) Output LIADep.
    if( LIADep.empty() ){
#ifdef _DEBUGINFO
      //std::cout<<"OutputLCDepFilter LIAODep No LIADep Info \n";
#endif
      return;
    }
    for( DepBeg = LIADep.begin(), DepEnd = LIADep.end(); DepBeg != DepEnd; ++DepBeg){
      SinkPC = DepBeg->first;
      // 2.1) Setup SinkPC-Shadow space.  opt: output the tri-dep together. 
      // 2.1.1) The first time to insert LIDep.
      DepShadowPtr = (LSDepInfo **)AppAddrToShadow((long)SinkPC);

      if( (long)DepShadowPtr > LSDepSinkPCMax )
        LSDepSinkPCMax = (long)DepShadowPtr;
      if( (long)DepShadowPtr < LSDepSinkPCMin)
        LSDepSinkPCMin = (long) DepShadowPtr;



      if( *DepShadowPtr == 0 ){
#ifdef _OVERHEAD_PROF
        LSPCShadowTime++;
        LCDepInfoMallocTime++;
#endif
        LIDepPtr = (LSDepInfo*) malloc(sizeof(LSDepInfo));
        *DepShadowPtr = LIDepPtr;
        LIDepPtr->TrueDep = 0;
        LIDepPtr->AntiDep = 0;
        LIDepPtr->OutDep = 0;

#if 0
        if( (long)DepShadowPtr > LSLIDepSinkPCMax )
          LSLIDepSinkPCMax = (long)DepShadowPtr;
        if( (long)DepShadowPtr < LSLIDepSinkPCMin)
          LSLIDepSinkPCMin = (long) DepShadowPtr;
#endif
      } 
      else 
        LIDepPtr =*DepShadowPtr;


      // 2.2) Insert the LIADep.
      // LIAD.
      if( !DepBeg->second.empty() )
        for( SrcBeg = DepBeg->second.begin(), SrcEnd = DepBeg->second.end(); SrcBeg != DepBeg->second.end(); ++SrcBeg){
          // LI-AntiDep.
          SrcPC = *SrcBeg;

          LSDep* LIDepAnti = LIDepPtr->AntiDep;
          while( LIDepAnti != 0){
            if( LIDepAnti->SrcPC == SrcPC && LIDepAnti->SinkPC == (long)SinkPC ){
              if( LIDepAnti->Dist[0] != 0 )
                LIDepAnti->Dist[0] = 0;
#ifdef _DEBUGINFO
              std::cout<<"OutputLCDepFilter LIADep SinkPC = "<< SinkPC << ", SrcPC = "<< SrcPC << " \n";
#endif
              break;
            }
            LIDepAnti = LIDepAnti->next;
          }

          if( LIDepAnti == 0 ){
#ifdef _OVERHEAD_PROF
            LIDepMallocTime++;
#endif
#ifdef _DEBUGINFO
            std::cout<<"OutputLCDepFilter LIADep SinkPC = "<< SinkPC << ", SrcPC = "<< SrcPC << " \n";
#endif
            LIDepAnti = (LSDep*) malloc( sizeof(LSDep));
            LIDepAnti->next = LIDepPtr->AntiDep;
            LIDepPtr->AntiDep = LIDepAnti;
            LIDepAnti->SrcPC = SrcPC;
            LIDepAnti->SinkPC = (long)SinkPC;
            LIDepAnti->Dist[0] = LIDepAnti->Dist[1] = 0;
          }   
        }
    }

    //std::cout<<"OutputLIAODep LSDepSinkPCMin "  << LSDepSinkPCMin<<", LSDepSinkPCMax  "<< LSDepSinkPCMax <<"\n";
    return;
  }

  /* (Src,Sink, 1), (Src, Sink, 2) ---> the second deleted.
   *
   */
  void OutputLCDepFilter::OutputLCTDep( class LICDep & Dep)
  {
    long SrcPC, SinkPC;
    int LCDDist = 0;
    std::map<long, std::set<SrcPCDist> >&LCTDep = Dep.LCTDep;
    std::map<long, std::set<SrcPCDist> >::iterator DepBeg, DepEnd;
    std::set<SrcPCDist>::iterator SrcPCBeg, SrcPCEnd;
    LSDepInfo** DepShadowPtr; 
    LSDepInfo* LCDepPtr;
    if( !LCTDep.empty() ){
      for( DepBeg = LCTDep.begin(), DepEnd = LCTDep.end(); DepBeg != DepEnd; ++DepBeg){
        SinkPC = DepBeg->first;
        DepShadowPtr =(LSDepInfo**) AppAddrToShadow(  SinkPC );
        if( (long)DepShadowPtr > LSDepSinkPCMax )
          LSDepSinkPCMax = (long)DepShadowPtr;
        if( (long)DepShadowPtr < LSDepSinkPCMin)
          LSDepSinkPCMin = (long) DepShadowPtr;

        for( SrcPCBeg = DepBeg->second.begin(), SrcPCEnd = DepBeg->second.end();
            SrcPCBeg != SrcPCEnd;  ++SrcPCBeg ){
          SrcPC = SrcPCBeg->SrcPC;
          LCDDist = SrcPCBeg->LCDDist;

          if( *DepShadowPtr == 0 ){
#ifdef _OVERHEAD_PROF
            LCDepInfoMallocTime++;
            LSPCShadowTime++;
#endif
            LCDepPtr = (LSDepInfo*) malloc (sizeof(LSDepInfo));
            *DepShadowPtr = LCDepPtr;
            LCDepPtr->TrueDep = 0;
            LCDepPtr->AntiDep = 0;
            LCDepPtr->OutDep = 0;

            // Not thread safe.
#if 0
            if( (long)DepShadowPtr > LDDLSResultMax )
              LDDLSResultMax = (long)DepShadowPtr;
            else if( (long)DepShadowPtr < LDDLSResultMin)
              LDDLSResultMin = (long) DepShadowPtr;
#endif
          }
          else
            LCDepPtr = *DepShadowPtr;

          LSDep *LCDTrue = LCDepPtr->TrueDep;
          while( LCDTrue != 0){
            if( (LCDTrue->SrcPC == SrcPC) && (LCDTrue->SinkPC == SinkPC) ){
              if( 0 == LCDTrue->Dist[1] ){
                LCDTrue->Dist[1] = LCDDist;
              }
              else if( LCDTrue->Dist[1] > LCDDist ){
                LCDTrue->Dist[1] = LCDDist;
              }
              break;
            }
            LCDTrue = LCDTrue->next;
          }
          if( LCDTrue == 0 ){
#ifdef _OVERHEAD_PROF
            LCDepMallocTime++;
#endif
            LCDTrue = (LSDep*) malloc( sizeof(LSDep) );
            LCDTrue->next = LCDepPtr->TrueDep;
            LCDepPtr->TrueDep = LCDTrue; 
            LCDTrue->SrcPC = SrcPC;
            LCDTrue->SinkPC = SinkPC; 
            LCDTrue->Dist[0] = 1;
            LCDTrue->Dist[1] = LCDDist;
#ifdef _DEBUGINFO
            //std::cout<<"OutputLCDepFilter::LCTDep SinkPC = " << SinkPC << ", SrcPC = "<< SrcPC << ", Dist = " << LCDDist << "\n";
#endif
          }

          // end 1.1)
        }
      }
    }
    else{
#ifdef _DEBUGINFO
      //std::cout<<"OutputLCDepFilter::OutputLCTDep() No LCTDep info"<<std::endl;
#endif
    }

    //std::cout<<"OutputLCTDep LSDepSinkPCMin "  << LSDepSinkPCMin<<", LSDepSinkPCMax = "<< LSDepSinkPCMax <<"\n";
    return;

  }
  void OutputLCDepFilter::OutputLCADep( class LICDep & Dep)
  {
    long SrcPC, SinkPC;
    int LCDDist = 0;
    std::map<long, std::set<SrcPCDist> >&LCADep = Dep.LCADep;
    std::map<long, std::set<SrcPCDist> >::iterator DepBeg, DepEnd;
    std::set<SrcPCDist>::iterator SrcPCBeg, SrcPCEnd;
    LSDepInfo** DepShadowPtr; 
    LSDepInfo* LCDepPtr;

    if( LCADep.empty() ){
#ifdef _DEBUGINFO
      //std::cout<<"OutputLCDepFilter::OutputLCADep() No LCADep info"<<std::endl;
#endif
      return;
    }

    for( DepBeg = LCADep.begin(), DepEnd = LCADep.end(); DepBeg != DepEnd; ++DepBeg){
      SinkPC = DepBeg->first;
      DepShadowPtr = (LSDepInfo**) AppAddrToShadow( SinkPC );

      if( (long)DepShadowPtr > LSDepSinkPCMax )
        LSDepSinkPCMax = (long)DepShadowPtr;
      if( (long)DepShadowPtr < LSDepSinkPCMin)
        LSDepSinkPCMin = (long) DepShadowPtr;

      for( SrcPCBeg = DepBeg->second.begin(), SrcPCEnd = DepBeg->second.end();
          SrcPCBeg != SrcPCEnd;  ++SrcPCBeg ){
        SrcPC = SrcPCBeg->SrcPC;
        LCDDist = SrcPCBeg->LCDDist;

        if( *DepShadowPtr == 0 ){
#ifdef _OVERHEAD_PROF
          LSPCShadowTime++;
          LCDepInfoMallocTime++;
#endif
          LCDepPtr = (LSDepInfo*) malloc (sizeof(LSDepInfo));
          *DepShadowPtr = LCDepPtr;
          LCDepPtr->TrueDep = 0;
          LCDepPtr->AntiDep = 0;
          LCDepPtr->OutDep = 0;
#if 0
          if( (long)DepShadowPtr > LDDLSResultMax )
            LDDLSResultMax = (long)DepShadowPtr;
          else if( (long)DepShadowPtr < LDDLSResultMin)
            LDDLSResultMin = (long) DepShadowPtr;

#endif
        }
        else
          LCDepPtr = *DepShadowPtr;

        LSDep *LCDAnti = LCDepPtr->AntiDep;
        while( LCDAnti != 0){
          if( LCDAnti->SrcPC == SrcPC && LCDAnti->SinkPC == SinkPC ){
            if( LCDAnti->Dist[1] == 0 ){
              LCDAnti->Dist[1] = LCDDist;
            }
            if( LCDAnti->Dist[1] > LCDDist ){
              LCDAnti->Dist[1] = LCDDist;
            }
            break;
          }
          LCDAnti = LCDAnti->next;
        }

#ifdef _DEBUGINFO
        //std::cout<<"OutputLCDepFilter::LCADep SinkPC = " << SinkPC << ", SrcPC = "<< SrcPC << ", Dist = "<< LCDDist << "\n";
#endif

        if( LCDAnti == 0 ){
#ifdef _OVERHEAD_PROF
          LCDepMallocTime++;
#endif
          LCDAnti = (LSDep*) malloc( sizeof(LSDep) );
          LCDAnti->next = LCDepPtr->AntiDep;
          LCDepPtr->AntiDep = LCDAnti; 
          LCDAnti->SrcPC = SrcPC;
          LCDAnti->SinkPC = SinkPC; 
          LCDAnti->Dist[1] = LCDDist;
          LCDAnti->Dist[0] = 1;
#ifdef _DEBUGINFO
          //std::cout<<"OutputLCDepFilter::LCADep SinkPC = " << SinkPC << ", SrcPC = "<< SrcPC << ", Dist = "<< LCDDist << "\n";
#endif
        }
      }
    }

    //std::cout<<"OutputLCADep LSDepSinkPCMin "  << LSDepSinkPCMin<<", LSDepSinkPCMax = "<< LSDepSinkPCMax <<"\n";
    return;
  }

  void OutputLCDepFilter::OutputLCODep( class LICDep &Dep)
  {
    long SrcPC, SinkPC;
    int LCDDist = 0;
    std::map<long, std::set<SrcPCDist> >&LCODep = Dep.LCODep;
    std::map<long, std::set<SrcPCDist> >::iterator DepBeg, DepEnd;
    std::set<SrcPCDist>::iterator SrcPCBeg, SrcPCEnd;
    LSDepInfo** DepShadowPtr; 
    LSDepInfo* LCDepPtr;

    if( LCODep.empty() ){
#ifdef _DEBUGINFO
      //std::cout<<"OutputLCDepFilter::OutputLCODep() No LCODep info"<<std::endl;
#endif
      return;
    }

    for( DepBeg = LCODep.begin(), DepEnd = LCODep.end(); DepBeg != DepEnd; ++DepBeg){
      SinkPC = DepBeg->first;
#ifdef _DEBUGINFO
      //std::cout<<"OutputLCODep() LCOD SinkPC = " <<  SinkPC << ", Num = " << DepBeg->second.size() <<std::endl;
#endif
      for( SrcPCBeg = DepBeg->second.begin(), SrcPCEnd = DepBeg->second.end();
          SrcPCBeg != SrcPCEnd;  ++SrcPCBeg ){
        SrcPC = SrcPCBeg->SrcPC;
        LCDDist = SrcPCBeg->LCDDist;
#ifdef _DEBUGINFO
        //std::cout<<"OutputLCODep() LCOD SinkPC = " << SinkPC << ", SrcPC = " <<  SrcPC << std::endl;
#endif

        DepShadowPtr =(LSDepInfo**) AppAddrToShadow( SinkPC );
        if( (long)DepShadowPtr > LSDepSinkPCMax )
          LSDepSinkPCMax = (long)DepShadowPtr;
        if( (long)DepShadowPtr < LSDepSinkPCMin)
          LSDepSinkPCMin = (long) DepShadowPtr;

        if( *DepShadowPtr == 0 ){
#ifdef _OVERHEAD_PROF
          LSPCShadowTime++;
          LCDepInfoMallocTime++;
#endif
          LCDepPtr = (LSDepInfo*) malloc (sizeof(LSDepInfo));
          *DepShadowPtr = LCDepPtr;
          LCDepPtr->TrueDep = 0;
          LCDepPtr->AntiDep = 0;
          LCDepPtr->OutDep = 0;
#if 0
          if( (long)DepShadowPtr > LDDLSResultMax )
            LDDLSResultMax = (long)DepShadowPtr;
          else if( (long)DepShadowPtr < LDDLSResultMin)
            LDDLSResultMin = (long) DepShadowPtr;
#endif
        }
        else
          LCDepPtr = *DepShadowPtr;

        LSDep *LCDOut = LCDepPtr->OutDep;
        while( LCDOut != 0){
          if( LCDOut->SrcPC == SrcPC && LCDOut->SinkPC == SinkPC ){
            if( LCDOut->Dist[1] == 0 ){
              LCDOut->Dist[1] = LCDDist; 
            } 
            else if( LCDOut->Dist[1] > LCDDist ){
              LCDOut->Dist[1] = LCDDist;
            }
            break;
          }
          LCDOut = LCDOut->next;
        }              

        if( LCDOut == 0 ){
#ifdef _OVERHEAD_PROF
          LCDepMallocTime++;
#endif
          LCDOut = (LSDep*) malloc( sizeof(LSDep) );
          LCDOut->next = LCDepPtr->OutDep;
          LCDepPtr->OutDep = LCDOut; 
          LCDOut->SrcPC = SrcPC;
          LCDOut->SinkPC = SinkPC; 
          LCDOut->Dist[1] = LCDDist;
          LCDOut->Dist[0] = 1;
#ifdef _DEBUGINFO
          //std::cout<<"OutputLCDepFilter::LCODep SinkPC = " << SinkPC << ", SrcPC = "<< SrcPC << ", Dist = " << LCDDist << "\n";
#endif

        }
      }
    }
    //std::cout<<"OutputLCODep LSDepSinkPCMin "  << LSDepSinkPCMin<<", LSDepSinkPCMax = "<< LSDepSinkPCMax <<"\n";
    return;
  }



  void* OutputLCDepFilter::operator()( void* item ) 
  {

#ifdef _DDA_DEFAULT_PP_PARLIDA
    class LICDep **ProcInput = static_cast<class LICDep**> (item);
    int i;
    for( i = 0; i < PROCNUM; ++i){
      if( ProcInput[i] == NULL )
        continue;
      class LICDep & input = *ProcInput[i];
      //std::cout<<"within OutputLCDepFilter myself =  " << pthread_self() << " \n"; 
#ifdef _TIME_PROF
      tbb::tick_count pststart;
      pststart = tbb::tick_count::now();
#endif
    
      // Write LIDep into the buffer.
      OutputLITDep( input ); 
      OutputLIAODep( input ); 

      if( input.StartLCDA ){
        //std::cout<<"StartLCDA = 1: LSDepSinkPCMin = "<< LSDepSinkPCMin <<", LSDepSinkPCMax = " << LSDepSinkPCMax << "\n";
        OutputLCTDep( input );
        OutputLCADep( input );
        OutputLCODep( input );
      }
      if( input.CancelPipeline ){
        //std::cout<<"OutputLCDepFilter::operator() CancelPipeline \n";
        LSLCDepSinkPCMin = LSDepSinkPCMin;
        LSLCDepSinkPCMax = LSDepSinkPCMax;
        //std::cout<<"LSDepSinkPCMin = "<< LSDepSinkPCMin <<", LSDepSinkPCMax = " << LSDepSinkPCMax << "\n";
        LSOutputLCDepFinish = 1; // signal master that pipeline has finished all work.
      }


      //delete(&input); // bugs?
      //ProcInput[i] = 0;

#ifdef _TIME_PROF 
      OutputTime += (tbb::tick_count::now() - pststart).seconds();
#endif

    } // end for
#if 0
    for( i = 0; i < PROCNUM; ++i){ // bug: memory leak.
      if( ProcInput[i] != NULL )
        delete ProcInput[i];
    }
#endif
        
    return 0;

#else
    class LICDep & input = *static_cast<class LICDep*>(item);
    //std::cout<<"within OutputLCDepFilter myself =  " << pthread_self() << " \n"; 
#ifdef _TIME_PROF
    tbb::tick_count pststart;
    pststart = tbb::tick_count::now();
#endif

    // Write LIDep into the buffer.
    OutputLITDep( input ); 
    OutputLIAODep( input ); 

    if( input.StartLCDA ){
      //std::cout<<"StartLCDA = 1: LSDepSinkPCMin = "<< LSDepSinkPCMin <<", LSDepSinkPCMax = " << LSDepSinkPCMax << "\n";
      OutputLCTDep( input );
      OutputLCADep( input );
      OutputLCODep( input );
    }
    if( input.CancelPipeline ){
      //std::cout<<"OutputLCDepFilter::operator() CancelPipeline \n";
      LSLCDepSinkPCMin = LSDepSinkPCMin;
      LSLCDepSinkPCMax = LSDepSinkPCMax;
      //std::cout<<"LSDepSinkPCMin = "<< LSDepSinkPCMin <<", LSDepSinkPCMax = " << LSDepSinkPCMax << "\n";
      LSOutputLCDepFinish = 1; // signal master that pipeline has finished all work.
    }


    delete(&input);

#ifdef _TIME_PROF 
    OutputTime += (tbb::tick_count::now() - pststart).seconds();
#endif

#if 0
    std::map<long , std::set<long> >::iterator mbeg, mend;
    std::set<long>::iterator sbeg, send;
    for(mbeg = AddrPC.begin(), mend = AddrPC.end(); mbeg != mend; mbeg++){
      std::cout<<"set.size " << mbeg->second.size() <<"\n";
    }
#endif


    return 0;
#endif
  }


  bool silent = false;
  int run_pipeline( int nthreads )
  {

#ifdef _TIME_PROF
    tbb::tick_count tstart;
    float PipeClearTime = 0.0;
#endif
    //int p = 1;
    //int p = 4;
    //tbb::task_scheduler_init init_serial( PipeThreadNum  );
    tbb::task_scheduler_init LIDAInit(LIDAThreadNum);

    //std::cout<< "within run_pipeline threadid  \n";
    // Create the pipeline
    tbb::pipeline pipeline;

    // Create RWTrace-reading stage and add it to the pipeline
#if 0
    ReadFilter read_filter;
    pipeline.add_filter( read_filter );

    // Create LIDA stage and add it to the pipeline
    LIDAFilter lida_filter; 
    pipeline.add_filter( lida_filter );
#endif

#if 1
    ReadLIDAFilter rlida_filter;
    pipeline.add_filter( rlida_filter );
#endif


    // Create output LIDep stage and add it to the pipeline
    //OutputLIDepFilter outputlidep_filter;
    //OutputLIDepFilter outputlidep_filter( output_file );
    //pipeline.add_filter( outputlidep_filter );
#if 1
    // Create LCDA stage and add it to the pipeline
    LCDAFilter lcda_filter; 
    pipeline.add_filter( lcda_filter );

    // Create output LCDep stage and add it to the pipeline
    OutputLCDepFilter outputlcdep_filter;
    //OutputLCDepFilter outputlcdep_filter( output_file );
    pipeline.add_filter( outputlcdep_filter );

#endif

    // Run the pipeline
    tbb::tick_count t0 = tbb::tick_count::now();

    std::cout<< "before pipeline.run \n";
#if 0
    SpawnThread = new std::thread( pipeline.run, p );
#endif
    pipeline.run(PipeThreadNum);

#ifdef _TIME_PROF
    tstart = tbb::tick_count::now(); 
#endif
    pipeline.clear();

#ifdef _TIME_PROF
    PipeClearTime += (tbb::tick_count::now() - tstart).seconds(); 
    fprintf(stdout, "pipecleartime = %f \n", PipeClearTime);
#endif
    //pipeline.run( nthreads*4 );
    std::cout<< "after pipeline.clear thread_id = " << pthread_self() << "\n";
    tbb::tick_count t1 = tbb::tick_count::now();


    if ( !silent ) printf("pipline run time = %g\n", (t1-t0).seconds());

    return 1;
  }

  /* Prepare the input for the pipeling.
   * Called by __checLoad/__checkStore functions.
   */
  int LDDPADriver(int *Ptr, long AddrLen, long LoopIter)
  {

    return 0;
  }

  extern inline int GenerateAddrStream(int *Ptr, short int AddrLen, int LoopIter, short int Flag, void *pc );


  /*  Setup and start the pipeline.
   *
   */
  int LDDPASetUpPiepline( ) 
  {
    int i, j, k;
    WStreamNum = 0; //0,1,...N,1,...,N,1
    RItemNum = 0;
    LCDAReady = 0;

#if defined( _DDA_DEFAULT )
#if defined(_DDA_DEFAULT_PP_PARLIDA)
    // Init for Streams[]. 
    Streams =(AddrStream**) shm_malloc(sizeof(AddrStream*) * PROCNUM);
    PPStreamIsFull = (atomic_t**) shm_malloc(sizeof(atomic_t*)*PROCNUM);
    for( i = 0; i < PROCNUM; i++){
      Streams[i] = (AddrStream*) shm_malloc(sizeof(AddrStream)*STREAMNUM);
      PPStreamIsFull[i] = (atomic_t*) shm_malloc(sizeof(atomic_t)*STREAMNUM);
      ProcWPresNum[i] = i;
      ProcRStreamNum[i] = 0;
      for( j = 0; j < STREAMNUM; j++)
        atomic_set(&PPStreamIsFull[i][j], 0);
    }
#else
    RStreamNum = 0; //0,1,...N,1,...,N,1
    // Init for Streams[]. 
    Streams =(AddrStream*) malloc(sizeof(AddrStream) * STREAMNUM);
    PtrItems = &Streams[0].Items[0];
    Streams[0].End = 1;
    PtrItems->RWFlag = 0;
    PtrItems++;
    for( i = 0; i < STREAMNUM; i++){
      StreamIsFull[i] = 0;
#endif
#endif

    WItemNum = 1;
    CurLoopIter = LDDProfLoopIter;

    // Init for Streams[]. 


#ifdef _DDA_GAS_SHADOW
#ifdef _DDA_GAS_SHADOW_PP_PARLIDA
    AccShdAddrStream = (long***) shm_malloc (sizeof(long**)*PROCNUM);
    AccShdNum = (int**) shm_malloc (sizeof(int*)*PROCNUM);
    AccShdLoopIter = (int**) shm_malloc (sizeof(int*)*PROCNUM);
    PPStreamIsFull = (atomic_t **) shm_malloc(sizeof(atomic_t*)*PROCNUM);
    for( i = 0; i < PROCNUM; ++i){
      AccShdAddrStream[i] = (long**) shm_malloc(sizeof(long*)*STREAMNUM)
        AccShdNum[i] = (int*) shm_malloc(sizeof(int)*STREAMNUM)
        AccShdLoopIter[i] = (int*) shm_malloc(sizeof(int)*STREAMNUM)
        PPStreamIsFull[i] = (int*) shm_malloc(sizeof(atomic_t)*STREAMNUM)
        for( j = 0; i < STREAMNUM; ++j){
          Streams[i][j] =(AddrStreamItem*) shm_malloc(sizeof(long)*STREAMITEMNUM);
          PPStreamIsFull[i][j] = 0;
        }
    }
#else
    WItemNum = 1;
    AccNum = 1;
    UseArrayNum = 0;
    AccShdAddrStream[0][0] = 0;
    CurLoopIter = LDDProfLoopIter;

    for( i = 0; i < STREAMNUM; i++){
      AccShdNum[i] = 0;
      AccShdLoopIter[i] = CurLoopIter;
      //ShdArrayPool[i] =  (RWTraceShdArray*) malloc (sizeof(RWTraceShdArray)*STREAMITEMNUM );
#ifdef _MUTEX
      pthread_mutex_init(&GRMutex[i], NULL);
      pthread_cond_init(&GRCond[i], NULL);
#endif

#ifdef _DDA_GAS_SHADOW_PARLIDA
      ThrLITDep = new std::map<long, std::set<long> > [RLIDATASKNUM];
      ThrLIADep = new std::map<long, std::set<long> > [RLIDATASKNUM];
      ThrLIODep = new std::map<long, std::set<long> > [RLIDATASKNUM];
#endif

    }

#endif
#endif // end _GAS_SHADOW


// Common statements.
    SpawnThread = 0;
#ifndef _DDA_DEFAULT  // bug ?
    LSStreamItemBuf = 0;
#endif
    LSPresentBuf = 0;

    WShadowStrmNum = 0;
    RShadowStrmNum = 0;
    RPresNum = 0;
    WPresNum = 0;

    for( i = 0; i < PRESBUFNUM; i++ )
      PresIsFull[i] = 0;

    int p = 4;
#if 1
    SpawnThread = new std::thread( run_pipeline, p );
    //std::thread SpawnThread( run_pipeline, p );
    std::cout<< "Spawn_thread_id  = " << SpawnThread->get_id() << "\n";
#endif
    //run_pipeline(p);
    //SpawnThread.join();

    return 1;
  }

  /* Called within OutputDep()
   *   1) This function is called after __addPresentToPrecedeTrace, so
   *   the buffer must have been applied correctly.
   * 2) Set the write value.
   *
   */
  void LDDPACancelPipeline()
  {
#ifdef _TIME_PROF
    tbb::tick_count tstart;  
    tstart = tbb::tick_count::now(); 
#endif
    std::cout<<"LDDPACancelPipeline WStreamNum =  " << WStreamNum <<"\n";
#ifndef _GAS_SHADOW
#ifdef _DDA_PP
    GLStreams[WStreamNum].Items[0].RWFlag = 2;
    GLStreams[WStreamNum].End = 1;
    GLStreams[WStreamNum].LoopIter = CurLoopIter;
    atomic_set(&GLPPStreamIsFull[WStreamNum], 1);
#endif
#ifndef _DDA_PP
    Streams[WStreamNum].Items[0].RWFlag = 2;
    Streams[WStreamNum].End = 1;
    Streams[WStreamNum].LoopIter = CurLoopIter;
    StreamIsFull[WStreamNum] = 1;
#endif

#else // _GAS_SHADOW
    AccShdAddrStream[WStreamNum][0] = 2;
    AccShdNum[WStreamNum] = 1;
    AccShdLoopIter[WStreamNum] = CurLoopIter;
#ifndef _GEN
    StreamIsFull[WStreamNum] = 1;
#endif

#ifdef _MUTEX
    pthread_mutex_lock(&GRMutex[WStreamNum]);
    pthread_cond_signal(&GRCond[WStreamNum]);
    pthread_mutex_unlock(&GRMutex[WStreamNum]);
#endif


#endif


#ifdef _DEBUGINFO
    std::cerr<< std::setbase(10) <<"LDDPACancelPipeline before: WStreamNum = " << WStreamNum << "\n";
#endif

    //  Wait until the buf is not full.
#ifdef _TIME_PROF
    CancelWaitTime += ( tbb::tick_count::now() - tstart ).seconds(); 
#endif



#ifdef _DEBUGINFO
    std::cerr<< std::setbase(10) <<"LDDPACancelPipeline : WStreamNum = " << WStreamNum << "\n";
#endif

    //std::cout<<"before join  master_thread_id = " << pthread_self() <<"\n";
#ifdef _TIME_PROF
    tstart = tbb::tick_count::now();
#endif 
    if( SpawnThread != 0 ){
      if( SpawnThread->joinable() ){
        SpawnThread->join();
        std::cout<<"SpawnThread is joinable \n";
        //delete SpawnThread;
      }
      else
        std::cout<<"SpawnThread is not joinable \n";
    }
    // Free memory setup in LDDPASetUpPiepline().
#ifndef _GAS_SHADOW
#ifdef _DDA_PP
    for( int i = 0; i < PROCNUM; i++)
      shm_free(Streams[i]);
    shm_free(Streams);
#else
    free(Streams);
#endif
#else // GAS_SHADOW
#ifdef _DA_PP
#else
    free(Streams);
#endif
#endif

#ifdef _GAS_SHADOW
#ifdef _MUTEX
    for(int i = 0; i < STREAMNUM; i++){
      pthread_mutex_destroy(&GRMutex[i]);
      pthread_cond_destroy(&GRCond[i]);
    }
#endif
#endif

#ifdef _TIME_PROF
    JoinWaitTime += (tbb::tick_count::now() - tstart).seconds();
#endif 
    std::cout<<"after join  master_thread_id = " << pthread_self() <<"\n";


#ifdef _TIME_PROF
    fprintf(stdout, "GPTime = %f, RCTime = %f, RPTime = %f LIDAPTime = %f, LCDACTime = %f \n", 
        GPTime, RCTime, RPTime, LIDAPTime, LCDACTime);
    fprintf(stdout, "CancelWaitTime = %f, JoinWaitTime = %f \n", CancelWaitTime, JoinWaitTime);
    fprintf(stdout, "GTime = %f, ReadTime = %f, LIDATime = %f, LCDATime = %f  OutputTime = %f\n", 
        GTime, ReadTime, LIDATime, LCDATime, OutputTime );
    fprintf(stdout, "ReadPreTime = %f ReadUpdateTime = %f KAddrTime = %f, KCacheTime = %f, KRBTime = %f\n", 
        ReadPreTime, ReadUpdateTime, KAddrTime, KCacheTime, KRBTime );
    //printf("ReadRWTrace tasknum = %d realloctime = %d\n", tasknum, realloctime);
    fprintf(stdout, "PRETIME = %f, PROTIME = %f \n", PRETIME, PROTIME);
#endif


    return ;

  }

  // Common APIs.
#ifdef _DDA_PP
  // Do initialization for each process. 
  // 1) Do local opts.
  // 2) PROCID = ftp_get_proc_id(); 
  void __init_proc()
  {
#ifdef _DDA_GAS_SHADOW_PP_PARLIDA
    PROCID = ftp_get_proc_id();
    GLPPStreamIsFull = PPStreamIsFull[PROCID];
    GLAccShdAddrStream = AccShdAddrStream[PROCID];
#endif

#ifdef _DDA_DEFAULT_PP_PARLIDA
    PROCID = ftp_get_proc_id();
    std::cout<<"ProcID = " <<PROCID << "\n";
    GLStreams = Streams[PROCID];
    GLPPStreamIsFull = PPStreamIsFull[PROCID];

    PtrItems = &GLStreams[0].Items[0];
    GLStreams[0].End = 1;
    PtrItems->RWFlag = 0;
    PtrItems++;
#endif

  }
#endif


/*
 * 1) Tell the pipeline the EndLoopIter for this loop.
 * 2) Release the RW-Shadow memory trace. 
 */

void ReInitPipelineState()
{
  GLStreams[WStreamNum].Items[0].RWFlag = 1; // new iteration.
  GLStreams[WStreamNum].End = WItemNum;
  GLStreams[WStreamNum].LoopIter = CurLoopIter;

  atomic_set(&GLPPStreamIsFull[WStreamNum], 1);

// Apply a new buffer for the new iteration.
  CurLoopIter = LDDProfLoopIter;
  WStreamNum = (WStreamNum+1) % STREAMNUM;
  WItemNum = 1;
  PtrItems = &GLStreams[WStreamNum].Items[0];

#ifdef _TIME_PROF2
  tbb::tick_count tstart;
  tstart = tbb::tick_count::now();
#endif
  while( atomic_read(&GLPPStreamIsFull[WStreamNum]) ) ;
#ifdef _TIME_PROF2
  GPTime += (tbb::tick_count::now() - tstart).seconds();
#endif

  PtrItems->RWFlag = 0; // 
  PtrItems++;

  return ;

  // 
  //
  // Release RW Memory states: Present, Previous.
  // AddrRange setup by LCDAFilter::operator.

  return ;
}

#endif // end _DDA_PA


