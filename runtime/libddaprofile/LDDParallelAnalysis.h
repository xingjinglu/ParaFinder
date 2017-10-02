#ifndef __LDDPARALLELANALYSIS_H
#define __LDDPARALLELANALYSIS_H

#ifdef _DDA_PA

#include<iostream>
#include<iomanip>

#include "LDDLightShadowRT.h"
#include "LDDCommonConst.h"
#include"tbb/atomic.h"

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

#include<stdlib.h>

#include<pthread.h>

#include "atomic.h"

using namespace tbb;


// size = 192bit == 24bytes
typedef struct __AddrStreamItem{
  int *Addr;
  void *PC;
  short int AddrLen; // <= 8. CancelPipeline = 0 ||  RWFlag = 6.
  short int RWFlag;  // R = 0; W = 1; Items[0].RWFlag: StartLCDA = 1, CancelPipeline = 2.
#ifndef _DDA_PA
//  short int MaskNum;
 // char ReadMask[2];
#endif
  // opt me? move RWFlag to AddrStream.
}AddrStreamItem;

typedef struct __AddrStream{
  //AddrStreamItem *Items;  
  AddrStreamItem Items[STREAMITEMNUM];  
  unsigned int LoopIter;
  unsigned int End; // the buf past the last ele;
}AddrStream;


typedef struct __SrcPCDist{
  long SrcPC;
  int LCDDist;
}SrcPCDist;

typedef struct __DepPair{
  long SinkPC;
  long SrcPC;
}DepPair;



#ifdef _DDA_DEFAULT
//extern AddrStream Streams[STREAMNUM];
#ifdef _DDA_DEFAULT_PP_PARLIDA
extern AddrStream **Streams;
extern atomic_t **PPStreamIsFull;
extern AddrStream *GLStreams;
extern atomic_t *GLPPStreamIsFull;
extern int PROCID;
extern tbb::atomic<long> ProcRStreamNum[PROCNUM];
#else // _PARLIDA or serial.
extern AddrStream *Streams;
extern tbb::atomic<long> StreamIsFull[STREAMNUM];
extern tbb::atomic<long> RStreamNum;
#endif
#endif // end _DDA_DEFAULT

extern tbb::atomic<long> WShadowStrmNum,  RShadowStrmNum, WPresNum, RPresNum,  RItemNum;
//extern tbb::atomic<long> WItemNum, WStreamNum;
extern long WItemNum, WStreamNum;
extern unsigned int CurLoopIter; // The current Loop Iter of Stream.
// Ready to do LCDA. 
extern int LCDAReady; 

extern LSAddrShadowStrmItem *LSRWStreamItemBuf;

// Label the OutputLCDepFilter finished output DDA results.
extern volatile bool LSOutputLCDepFinish;
extern  long LSLIDepSinkPCMax, LSLIDepSinkPCMin, LSLCDepSinkPCMax, LSLCDepSinkPCMin;

#ifdef _OVERHEAD_PROF
extern long LSRWStreamItemBufReuseTime, LSRWStreamItemMallocTime;
extern long MasterThrStart, MasterThrEnd; // Time master thread.
#endif

#ifdef _TIME_PROF
extern volatile float GPTime;
extern volatile float RCTime, RPTime ;
extern volatile float LIDAPTime, LIDACTime, LCDAPTime, LCDACTime;
extern volatile float CancelWaitTime, JoinWaitTime;
extern volatile float GTime, ReadTime, LIDATime, LCDATime,  OutputTime;
#endif

#ifdef _GAS_SHADOW
// Keep ShdAddr accessed.
// [x][0] == 1, new iteration, start LCDA.
extern long AccShdAddrStream[STREAMNUM][STREAMITEMNUM]; //[][1,2,..]
extern int AccShdNum[STREAMNUM]; // Length. 0, 1,2, ...
extern int AccShdLoopIter[STREAMNUM];// Keep LoopIter number.  
extern int AccNum; // ith of ShdAddr;

#if 0
extern RWTraceShdNode *ShdNodePool[STREAMNUM];
#endif
extern RWTraceShdArrayEle *ShdArrayPool[STREAMNUM];
extern RWTraceShdArrayEle *PtrShdArrayPool;
extern int UseArrayNum, FreeArrayNum;

#ifdef _MUTEX
extern pthread_mutex_t GRMutex[STREAMNUM];                                                                                   
extern pthread_cond_t GRCond[STREAMNUM];
#endif

#endif

#ifdef _DDA_PP
void __init_proc();
#endif
void ReInitPipelineState();
int LDDPASetUpPiepline( );
int LDDPADriver(int *Ptr, long AddrLen, unsigned int LoopIter);
//inline 
//int GenerateAddrStream(int *Ptr, short int AddrLen, int LoopIter, short int RWFlag, void *pc);
inline 
void UpdatePresentStream(void *PC, void **ShadowAddr, unsigned char RWMask, unsigned int LoopIter , int RWFlag);


void __doReadLIDWithSinkPCAsKeyLICDepPA(void * PC, unsigned char *ReadMask,LSPresentRWTrace &PRWTraceRef, Trace &TraceRef);


void __checkLIDandUpdatePresentRTracePA(void *PC, void **ShadowAddr, 
                     unsigned char * ReadMask, unsigned int LoopIter);


void __doWriteLIDWithSinkPCAsKeyLICDepPA(void * PC, unsigned char WriteMask, 
                        LSPresentRWTrace &PRWTraceRef, Trace &TraceRef);
void __determinWriteLIDPA(void * PC, unsigned char WriteMask, 
                        LSPresentRWTrace &PRWTraceRef, Trace &TraceRef);
void __checkLIDandUpdatePresentWTracePA(void *PC, void **ShadowAddr, 
                     unsigned char WriteMask, unsigned int LoopIter);

void LDDPACancelPipeline();




//int __determinInner8bytesInfo(void *ptr, long AddrLen, 
 //   long *AddrInnerOffset, long *Len1, unsigned char* ReadMask1, 
  //  long *Len2, unsigned char * ReadMask2 );


 /* When change to new stream.
   * 1) the current stream is full.
   * 2) The addr belong to a new iteration, and Start LCDA(RWFlag=5).
   * 3) WStreamNum: 0, 1, ... STREAMNUM.
   *
   * To test if StreamIsFull[RWStreamNum] is delayed to the time to insert new
   * addr.
   * Output: Streams[WStreamNum].item.
   * Not thread safe.
   * 
   */
extern AddrStreamItem *PtrItems;
extern int NoReturn;
extern long AddrNum;


#ifdef _DDA_DEFAULT

#ifdef _DDA_DEFAULT_PP_PARLIDA
// Streams[WStreamNum].Items[0] represents whether starts a new iteration.
// Items[0].RWFlag: 0 -> in present iteration, 
inline  
int GenerateAddrStream(int *Ptr, short int AddrLen, unsigned  int LoopIter, short int Flag, void *pc )
{

#ifdef _TIME_PROF2
  tbb::tick_count tstart, tend, pststart;
#endif

#ifdef _TIME_PROF2
  pststart = tbb::tick_count::now();
#endif
 
  //std::cout<<"addr " << Ptr <<"\n";
  // 1) Still in the iteration && Cur Buf isnot full && ;
  if( WItemNum < STREAMITEMNUM  ){
    PtrItems->Addr = Ptr;
    PtrItems->PC = pc;
    PtrItems->AddrLen = AddrLen;
    PtrItems->RWFlag = Flag;
    PtrItems++;
    WItemNum++; 
  }
  // Cur buf is full and apply the next buf.
  else{
   // GLStreams[WStreamNum].LoopIter = CurLoopIter;
    GLStreams[WStreamNum].LoopIter = LoopIter;
    GLStreams[WStreamNum].End = WItemNum;
#ifndef _GEN
    atomic_set(&GLPPStreamIsFull[WStreamNum], 1);
#endif

    WStreamNum = (WStreamNum+1) % STREAMNUM;
    //CurLoopIter = LoopIter;
    WItemNum = 1; //
    PtrItems = &GLStreams[WStreamNum].Items[0];
#ifdef _TIME_PROF2
    tstart = tbb::tick_count::now();
#endif
    while( atomic_read(&GLPPStreamIsFull[WStreamNum]) ) ;
    //std::cout<<"after PROCID " << PROCID <<", WStreamNum " << WStreamNum << "\n";
#ifdef _TIME_PROF2
    GPTime += (tbb::tick_count::now() - tstart).seconds();
#endif
    PtrItems->RWFlag = 0; // 
    PtrItems++;
  }

#ifdef _TIME_PROF2
  GTime += (tbb::tick_count::now() - pststart).seconds();
#endif

  return 0;
}
#endif


#ifndef _DDA_PP
// Streams[WStreamNum].Items[0] represents whether starts a new iteration.
// Items[0].RWFlag: 0 -> in present iteration, 
//

inline  
int GenerateAddrStream(int *Ptr, short int AddrLen, unsigned  int LoopIter, short int Flag, void *pc )
{

#ifdef _TIME_PROF2
  tbb::tick_count tstart, tend, pststart;
#endif

#ifdef _TIME_PROF2
  pststart = tbb::tick_count::now();
#endif
 
  //std::cout<<"addr " << Ptr <<"\n";
  // 1) Still in the iteration && Cur Buf isnot full && ;
  if( WItemNum < STREAMITEMNUM  ){
    PtrItems->Addr = Ptr;
    PtrItems->PC = pc;
    PtrItems->AddrLen = AddrLen;
    PtrItems->RWFlag = Flag;
    PtrItems++;
    WItemNum++; 
  }
  // Cur buf is full and apply the next buf.
  else{
    Streams[WStreamNum].LoopIter = CurLoopIter;
    Streams[WStreamNum].End = WItemNum;
#ifndef _GEN
    StreamIsFull[WStreamNum] = 1;
#endif

    WStreamNum = (WStreamNum+1) % STREAMNUM;
    CurLoopIter = LoopIter;
    WItemNum = 1; //
    PtrItems = &Streams[WStreamNum].Items[0];
#ifdef _TIME_PROF2
    tstart = tbb::tick_count::now();
#endif
    while( StreamIsFull[WStreamNum] ) ;
#ifdef _TIME_PROF2
    GPTime += (tbb::tick_count::now() - tstart).seconds();
#endif
    PtrItems->RWFlag = 0; // 
    PtrItems++;
  }

#ifdef _TIME_PROF2
  GTime += (tbb::tick_count::now() - pststart).seconds();
#endif

  return 0;
}
#endif
#endif // end _DDA_DEFAULT


#ifdef _GAS_SHADOW // GAS_SHADOW

#ifndef _DDA_PP
// 1) Addr -> ShdAddr;
// 2) ShdAddr + AddrLen -> RWMask
// 3) RWTrace -> ShdRWTrace , malloc ShdRWTrace memory.
// 4) Streams[] store the ShAddr Has been accessed. 
//
inline  
int GenerateAddrStream(int *Ptr, short int AddrLen, unsigned  int LoopIter, short int Flag, void *PC )
{

#ifdef _TIME_PROFx
  tbb::tick_count tstart, tend, pststart;
  pststart = tbb::tick_count::now();
#endif

  long ShdAddr, AddrInnerOffset;
  int Len;
  unsigned char RWMask;
  Trace *ShdTrcPtr;
  ShdAddr = (long)Ptr & 0x6FFFFFFFFFF8;  
  AddrInnerOffset = ShdAddr & 0x0000000000000007;
  Len = AddrInnerOffset + AddrLen;
  RWMask = (1<<Len) - (1<<AddrInnerOffset);
  ShdTrcPtr = *((Trace**)ShdAddr);

  //std::cout<<"Addr "<<ShdAddr <<", Flag" << Flag <<", PC" << PC <<"\n";
  if( WItemNum >= STREAMITEMNUM){
    AccShdLoopIter[WStreamNum] = CurLoopIter;
    AccShdNum[WStreamNum] = AccNum;
    StreamIsFull[WStreamNum] = 1;
#ifdef _MUTEX
    pthread_mutex_lock(&GRMutex[WStreamNum]);                                   
    pthread_cond_signal(&GRCond[WStreamNum]);
    pthread_mutex_unlock(&GRMutex[WStreamNum]);
#endif

    WStreamNum = (WStreamNum+1) % STREAMNUM;
    CurLoopIter = LoopIter; // To-Opt: remove, LoopIter not changes.
    WItemNum = 1; //
    AccNum =1;
    //UseArrayNum = 0;
#ifdef _TIME_PROFx
    tstart = tbb::tick_count::now();
#endif

#ifndef _MUTEX
     while( StreamIsFull[WStreamNum] ) ;
#else
    if( StreamIsFull[WStreamNum] ){ 
      pthread_mutex_lock(&GRMutex[WStreamNum]);                                                                   
      pthread_cond_wait(&GRCond[WStreamNum], &GRMutex[WStreamNum]);
      pthread_mutex_unlock(&GRMutex[WStreamNum]);
    } 
#endif

#ifdef _TIME_PROFx
    GPTime += (tbb::tick_count::now() - tstart).seconds();
#endif
    //PtrShdArrayPool = &ShdArrayPool[WStreamNum][1];
    AccShdAddrStream[WStreamNum][0] = 0;
  }

  // This ShdAddr has been accessed.
  if( ShdTrcPtr ){
    int &CurNum = ShdTrcPtr->CurArrayNum[WStreamNum];
    int &MaxSize = ShdTrcPtr->MaxSize[WStreamNum];
    RWTraceShdArrayEle *ShdArray = &ShdTrcPtr->ShdArray[WStreamNum][0];
    bool NeedInsert = 1;
    int p, q;

    // First access in a new WStreamNum.
    if( CurNum ){
      if( CurNum <= MaxSize ){ // some addr set not accessed by the same pc.
      #if 1
        int NPos = 0, TargetPos = 0; 
        // Find whether need to insert the new RWTrace.
        // case 1) Flag == 0
        if( Flag == 0 ){
          for( p = CurNum-1; p > 0; --p ){
            if( ShdArray[p].RWMask[0] ){
              if( ShdArray[p].PC == (long)PC ){
                TargetPos = p;
                break;
              }
            }
            else if( !NPos )
              NPos = p; 
          }
          //
          if( TargetPos ){
            if(!NPos)  NeedInsert = 0;
            else{ // W between TargetPos and CurPos.
            // check Ws happen before the previous Read operation by PC.
              q  = TargetPos+1;
              for( p = NPos ; p < TargetPos && q; ++p  ){
                if( ShdArray[p].RWMask[1])
                  for(; q > 0; --q){
                    if( ShdArray[q].RWMask[1] && (ShdArray[p].PC == ShdArray[q].PC) ) break;
                  }
              }
            if( p == TargetPos ) NeedInsert = 0; 
            }
          }

        }
        // case 2) Flag == 1
        else{
          for( p = CurNum-1; p > 0; --p ){
            if( ShdArray[p].RWMask[1] ){
              if( ShdArray[p].PC == (long)PC ){
                TargetPos = p;
                break;
              }
            }
            else if( !NPos )
              NPos = p; 
          
          } // end for
          if( TargetPos ){
            if(!NPos)  NeedInsert = 0;
            else{ // W between TargetPos and CurPos.
              // check Ws happen before the previous Read operation by PC.
              q  = TargetPos+1;
              for( p = NPos ; p < TargetPos && q; ++p  ){
                if( ShdArray[p].RWMask[0])
                  for(; q > 0; --q){
                    if( ShdArray[q].RWMask[0] && (ShdArray[q].PC == ShdArray[q].PC) ) break;
                  }
              }
              if( p == TargetPos ) NeedInsert = 0; 
            }
          }
        } // end else
      #endif
      }
      if( NeedInsert ){
        if ( CurNum >= MaxSize ){
          MaxSize = MaxSize * 2;
          ShdTrcPtr->ShdArray[WStreamNum]  = (RWTraceShdArrayEle*)realloc(ShdTrcPtr->ShdArray[WStreamNum], MaxSize*sizeof(RWTraceShdArrayEle));
          //std::cout<< std::setbase(10) <<"realloc MaxSize = "<< MaxSize << "\n";
        }
      #if 1
        RWTraceShdArrayEle &ShdArray = ShdTrcPtr->ShdArray[WStreamNum][CurNum];
        ShdArray.PC = (long) PC;
        ShdArray.RWMask[Flag] = RWMask;
        ShdArray.RWMask[1-Flag] = 0;
      #endif
      #if 0
        ShdTrcPtr->ShdArray[WStreamNum][CurNum].PC = (long)PC; 
        ShdTrcPtr->ShdArray[WStreamNum][CurNum].RWMask[Flag] = RWMask; 
        ShdTrcPtr->ShdArray[WStreamNum][CurNum].RWMask[1-Flag] = 0; 
      #endif
        CurNum++;
      }
    }
    else{ // First access in a new WStreamNum.
      AccShdAddrStream[WStreamNum][AccNum] = ShdAddr;
      ++AccNum;
      ShdTrcPtr->ShdArray[WStreamNum] = (RWTraceShdArrayEle*) malloc(sizeof(RWTraceShdArrayEle)* MaxSize );

#if 0 // worse.
    RWTraceShdArrayEle &ShdArray = ShdTrcPtr->ShdArray[WStreamNum][0];
    ShdArray.PC = (long) PC;
    ShdArray.RWMask[Flag] = RWMask;
    ShdArray.RWMask[1-Flag] = 0;
#endif
#if 1
      ShdTrcPtr->ShdArray[WStreamNum][0].PC = (long)PC; 
      ShdTrcPtr->ShdArray[WStreamNum][0].RWMask[Flag] = RWMask; 
      ShdTrcPtr->ShdArray[WStreamNum][0].RWMask[1-Flag] = 0;
#endif
      ++CurNum;
    }

  }
  else{ // The ShdAddr has never been accessed before.
    AccShdAddrStream[WStreamNum][AccNum] = ShdAddr;
    AccNum++;

    Trace *TrcPtr = (Trace*) malloc(sizeof(Trace));
    *((Trace**)ShdAddr) = TrcPtr;
    TrcPtr->Precede = 0;

    for( int i = 0; i < STREAMNUM; i++){
      TrcPtr->CurArrayNum[i] = 0;
      TrcPtr->MaxSize[i] = SHDMAXSIZE;
    }

    TrcPtr->ShdArray[WStreamNum] = (RWTraceShdArrayEle*) malloc(sizeof(RWTraceShdArrayEle)*SHDMAXSIZE);
#if 1
    RWTraceShdArrayEle &ShdArray = TrcPtr->ShdArray[WStreamNum][0];
    ShdArray.PC = (long) PC;
    ShdArray.RWMask[Flag] = RWMask;
    ShdArray.RWMask[1-Flag] = 0;
#endif
#if 0
    TrcPtr->ShdArray[WStreamNum][0].PC = (long)PC; 
    TrcPtr->ShdArray[WStreamNum][0].RWMask[Flag] = RWMask; 
    TrcPtr->ShdArray[WStreamNum][0].RWMask[1-Flag] = 0; 
#endif
    TrcPtr->CurArrayNum[WStreamNum] = 1;
  }
  WItemNum++;

#ifdef _TIME_PROFx
  GTime += (tbb::tick_count::now() - pststart).seconds();
#endif
  return 0;
}
#endif // end #ifndef _DDA_PP

#ifdef _DDA_PP
// 1) Addr -> ShdAddr;
// 2) ShdAddr + AddrLen -> RWMask
// 3) RWTrace -> ShdRWTrace , malloc ShdRWTrace memory.
// 4) Streams[] store the ShAddr Has been accessed. 
//
inline  
int GenerateAddrStream(int *Ptr, short int AddrLen, unsigned  int LoopIter, short int Flag, void *PC )
{

#ifdef _TIME_PROFx
  tbb::tick_count tstart, tend, pststart;
  pststart = tbb::tick_count::now();
#endif

  long ShdAddr, AddrInnerOffset;
  int Len;
  unsigned char RWMask;
  Trace *ShdTrcPtr;
  ShdAddr = (long)Ptr & 0x6FFFFFFFFFF8;  
  AddrInnerOffset = ShdAddr & 0x0000000000000007;
  Len = AddrInnerOffset + AddrLen;
  RWMask = (1<<Len) - (1<<AddrInnerOffset);
  ShdTrcPtr = *((Trace**)ShdAddr);

  //std::cout<<"Addr "<<ShdAddr <<", Flag" << Flag <<", PC" << PC <<"\n";
  if( WItemNum >= STREAMITEMNUM){
    AccShdLoopIter[WStreamNum] = CurLoopIter;
    AccShdNum[WStreamNum] = AccNum;
    StreamIsFull[WStreamNum] = 1;
#ifdef _MUTEX
    pthread_mutex_lock(&GRMutex[WStreamNum]);                                   
    pthread_cond_signal(&GRCond[WStreamNum]);
    pthread_mutex_unlock(&GRMutex[WStreamNum]);
#endif

    WStreamNum = (WStreamNum+1) % STREAMNUM;
    CurLoopIter = LoopIter; // To-Opt: remove, LoopIter not changes.
    WItemNum = 1; //
    AccNum =1;
    //UseArrayNum = 0;
#ifdef _TIME_PROFx
    tstart = tbb::tick_count::now();
#endif

#ifndef _MUTEX
     while( GLPPStreamIsFull[WStreamNum] ) ;
#else
    if( StreamIsFull[WStreamNum] ){ 
      pthread_mutex_lock(&GRMutex[WStreamNum]);                                                                   
      pthread_cond_wait(&GRCond[WStreamNum], &GRMutex[WStreamNum]);
      pthread_mutex_unlock(&GRMutex[WStreamNum]);
    } 
#endif

#ifdef _TIME_PROFx
    GPTime += (tbb::tick_count::now() - tstart).seconds();
#endif
    //PtrShdArrayPool = &ShdArrayPool[WStreamNum][1];
    GLAccShdAddrStream[WStreamNum][0] = 0;
  }

  // This ShdAddr has been accessed.
  if( ShdTrcPtr ){
    int &CurNum = ShdTrcPtr->CurArrayNum[PROCID][WStreamNum];
    int &MaxSize = ShdTrcPtr->MaxSize[PROCID][WStreamNum];
    RWTraceShdArrayEle *ShdArray = &ShdTrcPtr->ShdArray[PROCID][WStreamNum][0];
    bool NeedInsert = 1;
    int p, q;

    // First access in a new WStreamNum.
    if( CurNum ){
      if( CurNum <= MaxSize ){ // some addr set not accessed by the same pc.
      #if 1
        int NPos = 0, TargetPos = 0; 
        // Find whether need to insert the new RWTrace.
        // case 1) Flag == 0
        if( Flag == 0 ){
          for( p = CurNum-1; p > 0; --p ){
            if( ShdArray[p].RWMask[0] ){
              if( ShdArray[p].PC == (long)PC ){
                TargetPos = p;
                break;
              }
            }
            else if( !NPos )
              NPos = p; 
          }
          //
          if( TargetPos ){
            if(!NPos)  NeedInsert = 0;
            else{ // W between TargetPos and CurPos.
            // check Ws happen before the previous Read operation by PC.
              q  = TargetPos+1;
              for( p = NPos ; p < TargetPos && q; ++p  ){
                if( ShdArray[p].RWMask[1])
                  for(; q > 0; --q){
                    if( ShdArray[q].RWMask[1] && (ShdArray[p].PC == ShdArray[q].PC) ) break;
                  }
              }
            if( p == TargetPos ) NeedInsert = 0; 
            }
          }

        }
        // case 2) Flag == 1
        else{
          for( p = CurNum-1; p > 0; --p ){
            if( ShdArray[p].RWMask[1] ){
              if( ShdArray[p].PC == (long)PC ){
                TargetPos = p;
                break;
              }
            }
            else if( !NPos )
              NPos = p; 
          
          } // end for
          if( TargetPos ){
            if(!NPos)  NeedInsert = 0;
            else{ // W between TargetPos and CurPos.
              // check Ws happen before the previous Read operation by PC.
              q  = TargetPos+1;
              for( p = NPos ; p < TargetPos && q; ++p  ){
                if( ShdArray[p].RWMask[0])
                  for(; q > 0; --q){
                    if( ShdArray[q].RWMask[0] && (ShdArray[q].PC == ShdArray[q].PC) ) break;
                  }
              }
              if( p == TargetPos ) NeedInsert = 0; 
            }
          }
        } // end else
      #endif
      }
      if( NeedInsert ){
        if ( CurNum >= MaxSize ){
          MaxSize = MaxSize * 2;
          ShdTrcPtr->ShdArray[PROCID][WStreamNum]  = (RWTraceShdArrayEle*)realloc(ShdTrcPtr->ShdArray[PROCID][WStreamNum], MaxSize*sizeof(RWTraceShdArrayEle));
          //std::cout<< std::setbase(10) <<"realloc MaxSize = "<< MaxSize << "\n";
        }
      #if 1
        RWTraceShdArrayEle &ShdArray = ShdTrcPtr->ShdArray[PROCID][WStreamNum][CurNum];
        ShdArray.PC = (long) PC;
        ShdArray.RWMask[Flag] = RWMask;
        ShdArray.RWMask[1-Flag] = 0;
      #endif
      #if 0
        ShdTrcPtr->ShdArray[WStreamNum][CurNum].PC = (long)PC; 
        ShdTrcPtr->ShdArray[WStreamNum][CurNum].RWMask[Flag] = RWMask; 
        ShdTrcPtr->ShdArray[WStreamNum][CurNum].RWMask[1-Flag] = 0; 
      #endif
        CurNum++;
      }
    }
    else{ // First access in a new WStreamNum.
      GLAccShdAddrStream[WStreamNum][AccNum] = ShdAddr;
      ++AccNum;
      ShdTrcPtr->ShdArray[PROCID][WStreamNum] = (RWTraceShdArrayEle*) malloc(sizeof(RWTraceShdArrayEle)* MaxSize );

#if 0 // worse.
    RWTraceShdArrayEle &ShdArray = ShdTrcPtr->ShdArray[WStreamNum][0];
    ShdArray.PC = (long) PC;
    ShdArray.RWMask[Flag] = RWMask;
    ShdArray.RWMask[1-Flag] = 0;
#endif
#if 1
      ShdTrcPtr->ShdArray[PROCID][WStreamNum][0].PC = (long)PC; 
      ShdTrcPtr->ShdArray[PROCID][WStreamNum][0].RWMask[Flag] = RWMask; 
      ShdTrcPtr->ShdArray[PROCID][WStreamNum][0].RWMask[1-Flag] = 0;
#endif
      ++CurNum;
    }

  }
  else{ // The ShdAddr has never been accessed before.
    AccShdAddrStream[WStreamNum][AccNum] = ShdAddr;
    AccNum++;

    Trace *TrcPtr = (Trace*) malloc(sizeof(Trace));
    *((Trace**)ShdAddr) = TrcPtr;
    TrcPtr->Precede = 0;

    for( int i = 0; i < STREAMNUM; i++){
      TrcPtr->CurArrayNum[i] = 0;
      TrcPtr->MaxSize[i] = SHDMAXSIZE;
    }

    TrcPtr->ShdArray[WStreamNum] = (RWTraceShdArrayEle*) malloc(sizeof(RWTraceShdArrayEle)*SHDMAXSIZE);
#if 1
    RWTraceShdArrayEle &ShdArray = TrcPtr->ShdArray[WStreamNum][0];
    ShdArray.PC = (long) PC;
    ShdArray.RWMask[Flag] = RWMask;
    ShdArray.RWMask[1-Flag] = 0;
#endif
#if 0
    TrcPtr->ShdArray[WStreamNum][0].PC = (long)PC; 
    TrcPtr->ShdArray[WStreamNum][0].RWMask[Flag] = RWMask; 
    TrcPtr->ShdArray[WStreamNum][0].RWMask[1-Flag] = 0; 
#endif
    TrcPtr->CurArrayNum[WStreamNum] = 1;
  }
  WItemNum++;

#ifdef _TIME_PROFx
  GTime += (tbb::tick_count::now() - pststart).seconds();
#endif
  return 0;
}

#endif

#endif // end #ifdef _GAS_SHADOW


//extern inline
void __checkLoadLShadow(int *Ptr, long AddrLen);


#endif

#endif
