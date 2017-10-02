#define DEBUG_TYPE "LDDPROFSHADOW"

#include "LDDHashRT.h"

#include"llvm/Support/raw_ostream.h"
#include "llvm/Support/Debug.h"

#include<fstream>
#include<iostream>

#include<stdio.h>
#include<stdlib.h>


// Commnunicate with instrumentation.
// Labeled entering the profiling scope and current ProfilingLoopID
int DDAProfilingFlag = 0; 
std::map<int, int> MaxProfilingLoopID; // Record current max Profiled Loop ID.
int ProfilingLoopIterID = 0;


//  To be done. 20130921
//int ProfilingLoopID = 0; // not add now.

// 
std::map<int, RWTraceInfo*> HashRWTrace;

//RWTraceInfo *HashRWTrace; // size_t:24B
//
LDDepInfo LDDepResult;

#define N 100000
#define TRUEN  1000 
#define ANTIN  1000 
#define OUTPUTN  1000 


using namespace llvm;

void __initprofiling()
{
  //printf("init for instrumentations \n");
  // Not do the init after enter profiled loop.
#if 0
  RWTraceInfo *HashNode;
  HashNode = (RWTraceInfo*) malloc(sizeof(RWTraceInfo) * N);
  HashRWTrace[DDAProfilingFlag].second = HashNode; 
#endif

  return ;
}

// if loop not executed in order.
void __addnewprofilingbuffer(char* FuncName, char* LoopPos)
{
   std::map<int, int>::iterator cur, end;
   cur = MaxProfilingLoopID.find(DDAProfilingFlag);
   end = MaxProfilingLoopID.end();

  // Setup a buffer when Profiling a new loop.
  if( cur == end ){
    RWTraceInfo *HashNode;
    HashNode = (RWTraceInfo*) malloc(sizeof(RWTraceInfo) * N);
    HashRWTrace[DDAProfilingFlag] = HashNode; 
    MaxProfilingLoopID[DDAProfilingFlag] = DDAProfilingFlag;
    std::cout<< "new prof loop: " << DDAProfilingFlag << "  "
            << FuncName << "  " << LoopPos << std::endl;
    //DEBUG(dbgs()<< "new prof loop: " << DDAProfilingFlag << "  "<< FuncName 
    //            << "  " << LoopPos << "\n");
 
  }
  else{
  }

  // Init Loop Info:  FuncName && File_Line
  std::map<int, DepInfo>::iterator curResult, endResult;
  curResult = LDDepResult.find(DDAProfilingFlag);
  endResult = LDDepResult.end();
  if( curResult == endResult ){
    LDDepResult[DDAProfilingFlag].FuncName = FuncName;
    LDDepResult[DDAProfilingFlag].LoopPos = LoopPos;
  
  }
  else{
  }


  return ;
}


/* If found without hash conflictions, return the node;
** Else if has confilictions, create a new node and return the node;
**
*/
RWTraceInfo * lookupRWTraceInfo(size_t key, int *Addr)
{
  //  std::cout<<"profilingflag "<< DDAProfilingFlag << std::endl;

  // Not insert any addr with key "key" before or inserted without 
  // hash conflictions.
  if((0 == HashRWTrace[DDAProfilingFlag][key].Addr) 
      || ( HashRWTrace[DDAProfilingFlag][key].Addr ==  Addr))
    return &HashRWTrace[DDAProfilingFlag][key];

  else{
    // Tackle hash confilictions.
    // Look for the node in the extended buckets.
    RWTraceInfo *NewRWTraceInfo, *CurRWTraceInfo;
    CurRWTraceInfo = &HashRWTrace[DDAProfilingFlag][key];
    // If there is no conflictions before, .Next = NULL.
    NewRWTraceInfo = HashRWTrace[DDAProfilingFlag][key].Next; 
    while( NewRWTraceInfo ){
      if( NewRWTraceInfo->Addr ==  Addr)
        break;
      else { 
        CurRWTraceInfo = NewRWTraceInfo;
        NewRWTraceInfo = NewRWTraceInfo->Next;            
      }
    }
    // Find the node in the extended buckets.
    if( NewRWTraceInfo ) 
      return NewRWTraceInfo;
    // Not fin, Create a new node.
    else{
      NewRWTraceInfo = new RWTraceInfo;
      CurRWTraceInfo->Next = NewRWTraceInfo;
      NewRWTraceInfo->Next = NULL;
      return NewRWTraceInfo;
    }
  }  
}

unsigned findRWAddrIDIndex(std::vector<RWAddrID> &RWTrace, std::string AddrPos, std::string VarName)
{ 
  unsigned i;
  std::vector<RWAddrID>::iterator B, E;
  //DEBUG(dbgs() << "findRWAddrIDIndex: "<< RWTrace.size() << "\n");

  for( i = 0,  B = RWTrace.begin(), E = RWTrace.end(); B != E; ++B,++i ){
    if( (B->AddrPos == AddrPos) && ( B->VarName == VarName) ){
      break;
    }
  }
  return  i;
}


//
// Return: the index ReadAddr in RTrace. 
unsigned  addWritetoHashTraceInfo( void *Addr, long AddrLen, RWTraceInfo *CurRWAddrNode , std::string AddrPos, std::string VarName)
{
  unsigned WTraceIndex;
  size_t WTraceSize;
  RWAddrID WriteAddrID; // What ?

  // The address has not been inserted before.
  if( CurRWAddrNode->Addr != Addr ){
    CurRWAddrNode->Addr = Addr;
  }

  // Find the index of RWAddrID in RTrace.
  WTraceIndex = findRWAddrIDIndex(CurRWAddrNode->WTrace, AddrPos, VarName);

  // The RWTrace has not been inserted before.
  if( ((WTraceSize = CurRWAddrNode->WTrace.size()) == 0) || (WTraceIndex == WTraceSize) ){
    WriteAddrID.AddrPos = AddrPos;
    WriteAddrID.VarName = VarName;
    WriteAddrID.AddrLen = AddrLen;

    //
   // if( VarName == "pdSwaptionPrice.addr" )
    //  std::cout << "add pdSwaptionPrice.addr to hash: " << ProfilingLoopIterID << std::endl;
    WriteAddrID.RWIterNo.resize(2, 0);
    WriteAddrID.RWIterNo[1] =  ProfilingLoopIterID; // update the history iter no.
    WriteAddrID.RWIterNo[0] =  ProfilingLoopIterID;  // 
    CurRWAddrNode->WTrace.push_back(WriteAddrID);
  }
  // The RWTrace has been inserted already.
  else{
    if( CurRWAddrNode->WTrace[WTraceIndex].RWIterNo[0] != ProfilingLoopIterID ){
      CurRWAddrNode->WTrace[WTraceIndex].RWIterNo[1] = CurRWAddrNode->WTrace[WTraceIndex].RWIterNo[0];
      CurRWAddrNode->WTrace[WTraceIndex].RWIterNo[0] = ProfilingLoopIterID;
  
      //CurRWAddrNode->WTrace[WTraceIndex].RWIterNo.push_back(  ProfilingLoopIterID);
      }
  }

  return WTraceIndex;
}

/* Return: the index ReadAddr in RTrace. 
**
**
*/

unsigned  addReadtoHashTraceInfo( void *Addr, long AddrLen, RWTraceInfo *CurRWAddrNode , std::string AddrPos, std::string VarName)
{
  unsigned RTraceIndex;
  size_t RTraceSize;
  RWAddrID RAddrID; // what ?


  // The address has not been inserted before.
  if( CurRWAddrNode->Addr != Addr ){
    CurRWAddrNode->Addr = Addr;
  }

  // Find the index of RWAddrID in RTrace.
  // Classify a address with both AddrPos & VarName. 
  RTraceIndex = findRWAddrIDIndex(CurRWAddrNode->RTrace, AddrPos, VarName);

  // Not read before.
  if( ((RTraceSize = CurRWAddrNode->RTrace.size()) == 0) || (RTraceIndex == RTraceSize) ){
    RAddrID.AddrPos = AddrPos;
    RAddrID.VarName = VarName;
    RAddrID.AddrLen = AddrLen;
    //
    RAddrID.RWIterNo.resize(2, 0);  //
    RAddrID.RWIterNo[0] = ProfilingLoopIterID ; //
    RAddrID.RWIterNo[1] =  ProfilingLoopIterID;  // 

    //RAddrID.RWIterNo.push_back(  ProfilingLoopIterID );
    CurRWAddrNode->RTrace.push_back(RAddrID);
  }
  else{
    if( CurRWAddrNode->RTrace[RTraceIndex].RWIterNo[0]!= ProfilingLoopIterID ){

      // update the history iter no.
      CurRWAddrNode->RTrace[RTraceIndex].RWIterNo[1] =  CurRWAddrNode->RTrace[RTraceIndex].RWIterNo[0];
      CurRWAddrNode->RTrace[RTraceIndex].RWIterNo[0] =   ProfilingLoopIterID;
      //CurRWAddrNode->RTrace[RTraceIndex].RWIterNo.push_back(  ProfilingLoopIterID);

    }
  }

  return RTraceIndex;
}

// Define operator <() for set<LIDep>.
bool operator< ( const Dep &LI1, const  Dep &LI2)
{
  //return ((LI1.SrcAddrPos < LI2.SrcAddrPos) || (LI1.SrcVarName < LI2.SrcVarName) || (LI1.SinkAddrPos < LI2.SinkAddrPos) || (LI1.SinkVarName < LI2.SinkVarName) );
  return ((LI1.SrcAddrPos < LI2.SrcAddrPos) || (LI1.SrcAddrPos == LI2.SrcAddrPos && LI1.SrcVarName < LI2.SrcVarName) || ( LI1.SrcAddrPos == LI2.SrcAddrPos &&
 LI1.SrcVarName == LI2.SrcVarName && LI1.SinkAddrPos < LI2.SinkAddrPos) || ( LI1.SrcAddrPos == LI2.SrcAddrPos && LI1.SrcVarName == LI2.SrcVarName && LI1.SinkAddrPos == LI2.SinkAddrPos && LI1.SinkVarName < LI2.SinkVarName) ); 
}

// Define operator <() for map<Dep, set<int> >.
#if 0
bool operator< ( LCDDep LC1, LCDDep LC2)
{

}
#endif

/* Compute deps imediately after a read operation.
**
**
*/

void analyzeReadAddrDepSet(RWTraceInfo *CurRWAddrNode,std::string AddrPos,std::string VarName)
{
  std::vector<RWAddrID>::iterator B, E;
  std::vector<int>::iterator IterNoB,IterNoE;
  //std::vector<int>::reverse_iterator IterNoB,IterNoE;

  // Compute true dependence of loop carried or independent.
  for(B = CurRWAddrNode->WTrace.begin(), E = CurRWAddrNode->WTrace.end(); B != E; ++B){
    Dep Deps;
    Deps.SrcAddrPos =  B->AddrPos;
    Deps.SinkAddrPos = AddrPos; 
    Deps.SrcVarName = B->VarName;
    Deps.SinkVarName =  VarName;

    for( IterNoB = B->RWIterNo.begin(),  IterNoE = B->RWIterNo.end(); IterNoB != IterNoE; ++IterNoB){

      
      // Find loop independent true-dependence. 
      if( *IterNoB ==  ProfilingLoopIterID ){
        LDDepResult[DDAProfilingFlag].LIDDep.TrueDep.insert(Deps);
      } 

      // Loop carried true-dependence.
      else{
        int Dist = ProfilingLoopIterID - *IterNoB;
        //std::cout<< "Loop Carried True: "<< CurRWAddrNode->Addr<< ","<< B->AddrPos << ", " <<AddrPos << "," << B->AddrPos<< "," << VarName <<" " << Dist<< std::endl;
        //std::cout<< "LoopCarriedTrue: " <<  DDAProfilingFlag << "addr " << CurRWAddrNode->Addr << std::endl;
        LDDepResult[DDAProfilingFlag].LCDDep.TrueDep[Deps].insert(Dist);

        std::map<Dep, std::set<int> >::iterator LCDB, LCDE; 
#if 0
        for( LCDB = LDDepResult[DDAProfilingFlag].LCDDep.TrueDep.begin(), LCDE = LDDepResult[DDAProfilingFlag].LCDDep.TrueDep.end(); LCDB != LCDE; LCDB++)
        std::cout<< "name: " <<  LCDB->first.SrcVarName << std::endl ;
#endif
        // Just compute the latest loop carried dependence.
        break;  // Not need now.
      }
    }

  }  
  return ;
}

void analyzeWriteAddrDepSet(RWTraceInfo *CurRWAddrNode,std::string AddrPos,std::string VarName)
{
  std::vector<RWAddrID>::iterator B, E;
  std::vector<int>::iterator IterNoB,IterNoE;
  //std::vector<int>::reverse_iterator IterNoB,IterNoE;

  // Compute output-dependence of loop carried / independent.
  for(B = CurRWAddrNode->WTrace.begin(), E = CurRWAddrNode->WTrace.end(); B != E; ++B){
    Dep Deps;
    Deps.SrcAddrPos = B->AddrPos;
    Deps.SinkAddrPos = AddrPos; 
    Deps.SrcVarName = B->VarName;
    Deps.SinkVarName =  VarName;

    //if( VarName == "pdSwaptionPrice.addr" )
    //  std::cout << "write to pdSwaptionPrice.addr: " << ProfilingLoopIterID << std::endl;

    for( IterNoB = B->RWIterNo.begin(),  IterNoE = B->RWIterNo.end(); IterNoB != IterNoE; ++IterNoB){
     // Find loop independent output-dependence. 
      if( *IterNoB ==  ProfilingLoopIterID && (AddrPos != B->AddrPos || VarName != B->VarName)  ){
        LDDepResult[DDAProfilingFlag].LIDDep.OutDep.insert(Deps);
      } 
      // Loop carried output-dependence.
      else if( *IterNoB !=  ProfilingLoopIterID) {
        int Dist = ProfilingLoopIterID - *IterNoB;
       // if( VarName == "pdSwaptionPrice.addr" )
       //   std::cout << "output: " << Deps.SrcAddrPos << " " << Deps.SrcVarName << " "<< Deps.SinkAddrPos << " " << Deps.SinkVarName << std::endl;
        LDDepResult[DDAProfilingFlag].LCDDep.OutDep[Deps].insert(Dist);
        break; // Not need now.
      }
    }
  }  

  // Compute anti-dependence of loop carried / independent.
  for(B = CurRWAddrNode->RTrace.begin(), E = CurRWAddrNode->RTrace.end(); B != E; ++B){
    for( IterNoB = B->RWIterNo.begin(),  IterNoE = B->RWIterNo.end(); IterNoB != IterNoE; ++IterNoB){
      //std::cout<< "AnalyzeWriteDep:" << CurRWAddrNode->Addr<< "," << B->AddrLen<< "," <<  B->AddrPos <<", "<< B->VarName << std::endl;;     
      Dep Deps;
      Deps.SrcAddrPos = B->AddrPos;
      Deps.SinkAddrPos = AddrPos; 
      Deps.SrcVarName = B->VarName;
      Deps.SinkVarName =  VarName;
    
      // Find loop independent anti-dependence. 
      if( *IterNoB ==  ProfilingLoopIterID ){
        LDDepResult[DDAProfilingFlag].LIDDep.AntiDep.insert(Deps);
      } 
      // Loop carried anti-dependence.
      else{
        int Dist = ProfilingLoopIterID - *IterNoB;
        LDDepResult[DDAProfilingFlag].LCDDep.AntiDep[Deps].insert(Dist);
        break; // Only keep the minimal dependence distance. Not need now.
      }
    }
  }

  return ;
}
/*  __loadcheck: collect and analysis at the same time.
** Input: Addr, AddrLen, AddrPos=FileName_LineNo, VarName, LoopID, IterID. 
** Parameter: Addr, AddrLen, AddrPos, VarName.
** 
*/
void __loadcheck(int *ptr, long AddrLen, char *Pos, char* Name)
  //void __loadcheck(int *ptr, long AddrLen, std::string AddrPos, std::string VarName)
{
  
  if( !DDAProfilingFlag )
    return;
  int key;
  RWTraceInfo *CurRWAddrNode;
  unsigned RTraceIndex;

  // For test.
#if 0
  static int LineNo = 0;
  LineNo++;
  std::ostringstream Convert;
  Convert << LineNo;
  //std::string AddrPos = "simple.c_" + Convert.str();
  //std::string VarName = "dst"; 
#endif
  
  //DEBUG(dbgs() <<"read "<< ptr <<" " << AddrLen<< " "<< Pos << " "<< Name <<" " << ProfilingLoopIterID<< " \n");

  std::string AddrPos(Pos); 
  std::string VarName(Name); //  

  key =  ((long)ptr >> (AddrLen/2)) % N;
  //DEBUG(dbgs() << "LoopID = " <<  DDAProfilingFlag << "\n");
  //printf("LoopIterID = %d \n", ProfilingLoopIterID);

  // 1) Lookup the HashRWTrace node.
  CurRWAddrNode = lookupRWTraceInfo( key, ptr);
  //std::cout  << "read:" << key<< ", " << ptr <<"," << AddrLen << ", " << AddrPos << ", " << VarName <<  "CurAddrnode->addr " << CurRWAddrNode->Addr <<"\n";

  // 2) And the Read info to the node.
  RTraceIndex = addReadtoHashTraceInfo(ptr, AddrLen, CurRWAddrNode , AddrPos, VarName);

  // 3) Analysis the dependence introduced by ReadAddr.
  //
#if 0
  if( ptr != CurRWAddrNode->Addr) 
    std::cout<<"lookup error: " << ptr << " " << CurRWAddrNode->Addr << std::endl;
  else
    std::cout<< "lookup right; " << std::endl;
#endif
  analyzeReadAddrDepSet(CurRWAddrNode, AddrPos, VarName);


}

//  Update iterNo, addr of  memRW .
//  Add output-, anti-  dependencies. 
//
void __storecheck(int *ptr, long AddrLen, char* Pos, char* Name)
  //void __storecheck(int *ptr, long AddrLen, std::string AddrPos, std::string VarName)
{

  if(!DDAProfilingFlag)
    return;

  //DEBUG(dbgs() <<"write "<< ptr <<" " << AddrLen<< " "<< Pos << " "<< Name<<" "<<ProfilingLoopIterID << "\n");

  int key;

  key =  ((long)ptr >> (AddrLen/2))  %  N;

  RWTraceInfo *CurRWAddrNode;
  unsigned RTraceIndex;

  // For test.
  static int LineNo = 0;
  LineNo++;
  std::ostringstream Convert;
  Convert << LineNo;
  //std::string AddrPos = "simple.c_" + Convert.str();
  //std::string VarName = "dst"; 
  std::string AddrPos(Pos);  
  std::string VarName(Name); 
  // End for test.

  // 1) Lookup the HashRWTrace node.
  CurRWAddrNode = lookupRWTraceInfo( key, ptr);

  // 2) And the Write info to the node.
  //std::cout<< "write:" <<key << ", " << ptr <<"," << AddrLen << ", " << AddrPos << "," << VarName<< std::endl;
  RTraceIndex = addWritetoHashTraceInfo(ptr, AddrLen, CurRWAddrNode , AddrPos, VarName);

  // 3) Analysis the dependence introduced by wirte operation.
  analyzeWriteAddrDepSet(CurRWAddrNode, AddrPos, VarName);

  return ;
}

// Write the dependence set to the stdout in default. 
void __outputdependence()
{

  std::fstream fs;
  fs.open("dda-result", std::fstream::out);
  if( !fs.is_open())
    std::cout<<"cannot open file dda-result"<< "\n";

  std::map<int, DepInfo>::iterator LDDB, LDDE;
  std::set<Dep>::iterator LIDB, LIDE;
  std::map<Dep, std::set<int> >::iterator LCDB, LCDE;
  std::set<int>::iterator DistB, DistE;

  for( LDDB = LDDepResult.begin(), LDDE = LDDepResult.end(); LDDB != LDDE; ++LDDB){
    fs<< "New Loop "<< LDDB->second.FuncName << " " << LDDB->second.LoopPos << std::endl;

 #if 0 
    // 1) Output loop independent deps.
    for( LIDB = LDDB->second.LIDDep.TrueDep.begin(), LIDE = LDDB->second.LIDDep.TrueDep.end(); LIDB != LIDE; ++LIDB){
      fs << LIDB->SrcAddrPos<<","<<LIDB->SrcVarName<< ";" << LIDB->SinkAddrPos<<","<< LIDB->SinkVarName<< "; True" << std::endl;
    }

    for( LIDB = LDDB->second.LIDDep.AntiDep.begin(), LIDE = LDDB->second.LIDDep.AntiDep.end(); LIDB != LIDE; ++LIDB){
      fs <<  LIDB->SrcAddrPos<<","<<LIDB->SrcVarName<< ";" << LIDB->SinkAddrPos<<","<< LIDB->SinkVarName<< "; Anti"<<std::endl;
    }

    for( LIDB = LDDB->second.LIDDep.OutDep.begin(), LIDE = LDDB->second.LIDDep.OutDep.end(); LIDB != LIDE; ++LIDB){

      fs << LIDB->SrcAddrPos<<","<<LIDB->SrcVarName<< ";" << LIDB->SinkAddrPos<<","<< LIDB->SinkVarName<< "; Out"<< std::endl;
    }
#endif

    // 2) Output loop carried deps.
#if 1 
    for( LCDB = LDDB->second.LCDDep.TrueDep.begin(), LCDE = LDDB->second.LCDDep.TrueDep.end(); LCDB != LCDE; ++LCDB){
      if( LCDB->first.SrcVarName == "s.addr")
        break;
      fs << LCDB->first.SrcAddrPos<<" "<<LCDB->first.SrcVarName<< " " << LCDB->first.SinkAddrPos<<" "<< LCDB->first.SinkVarName<< " True";
      // Dependence distance.
      for(DistB = LCDB->second.begin(), DistE = LCDB->second.end(); DistB != DistE; ++DistB)
        fs << " " << *DistB;
      fs << std::endl;
    }

    for( LCDB = LDDB->second.LCDDep.AntiDep.begin(), LCDE = LDDB->second.LCDDep.AntiDep.end(); LCDB != LCDE; ++LCDB){
      if( LCDB->first.SrcVarName == "s.addr")
        break;
      fs << LCDB->first.SrcAddrPos<<" "<<LCDB->first.SrcVarName<< " " << LCDB->first.SinkAddrPos<<" "<< LCDB->first.SinkVarName<< " Anti";
      // Dependence distance.
      for(DistB = LCDB->second.begin(), DistE = LCDB->second.end(); DistB != DistE; ++DistB)
        fs << " " << *DistB;
      fs << std::endl;
    }
    for( LCDB = LDDB->second.LCDDep.OutDep.begin(), LCDE = LDDB->second.LCDDep.OutDep.end(); LCDB != LCDE; ++LCDB){
      if( LCDB->first.SrcVarName == "s.addr")
        break;
      fs << LCDB->first.SrcAddrPos<<" "<<LCDB->first.SrcVarName<< " " << LCDB->first.SinkAddrPos<<" "<< LCDB->first.SinkVarName<< " Out";
      // Dependence distance.
      for(DistB = LCDB->second.begin(), DistE = LCDB->second.end(); DistB != DistE; ++DistB)
        fs << " " << *DistB;
      fs << std::endl;
    }
#endif

  }

  fs.close();

}


void __loadcheckstackvar( int *ptr, long AddrLen, char *Pos, char* Name,  int TargetLoopFunc){
  if(TargetLoopFunc)
    __loadcheck(ptr, AddrLen, Pos, Name);

  return;
}
void __storecheckstackvar( int *ptr, long AddrLen, char *Pos, char* Name, int TargetLoopFunc){
  if(TargetLoopFunc)
    __storecheck(ptr, AddrLen, Pos, Name);

  return;

}
