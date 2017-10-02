#include"TimerProfilingRuntime.h"
#include<stdio.h>
#include<stdlib.h>
#include<map>

// <ID, time>
// ID: the static defined number for each loop.
std::map<long , unsigned long long > TimerResults; 

// LoopName: File_Func_LineNo, <ID, LoopName>.
std::map<long, std::string > LoopID2Name; 

// <TimeVal, LoopID>
std::multimap<unsigned long long, long> SortedResults;

// Called only once for each outermost-loop.
//inline 
void
__recordLoopID2Name( char *Name, long LoopID)
{
  std::string LoopName( Name ); 
  LoopID2Name[LoopID] = LoopName;
}

//inline 
void 
__recordTimerResults(long LoopID, unsigned long long Time)
{
  // Is this sound when first visit TimerResults[LoopID]? Yes.
  //std::cout <<" time: "<< LoopID << "  "  << Time << std::endl;
  TimerResults[LoopID] += Time;
}

// Output the LoopID: Loop Name pair;
//inline 
void
__outputLoopID2Name()
{
  std::map<long, std::string>::iterator LoopNameB, LoopNameE;  
  for( LoopNameB = LoopID2Name.begin(), LoopNameE = LoopID2Name.end(); LoopNameB != LoopNameE; ++LoopNameB ){
    std::cout<< LoopNameB->first<<"  " << LoopNameB->second  <<std::endl;
  }

}

//
//inline 
void
__outputTimerResults()
{
  std::map<long, unsigned long long>::iterator TRB, TRE; 
  std::cout<< "<time  LoopID  LoopName>, time:number of cpu cycles." << std::endl;
  for(TRB = TimerResults.begin(), TRE = TimerResults.end(); TRB != TRE; ++TRB){
    //std::cout<< TRB->first << "  " << TRB->second << "  " << LoopID2Name[TRB->first] << std::endl;
    SortedResults.insert( std::pair<unsigned long long,long>(TRB->second,TRB->first) );
  }
  std::multimap<unsigned long long, long>::reverse_iterator SRB, SRE; 
  for(SRB = SortedResults.rbegin(), SRE = SortedResults.rend(); SRB != SRE; ++SRB){
    std::cout<< SRB->first << "  " << SRB->second << "  " << LoopID2Name[SRB->second] << std::endl;
  }
  

}

// us  = Return_Value / ( Frequency_of_Processor * 1000 )
// return value: cycles of cpu
//inline 
unsigned long long 
__rdtsc()
{
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




