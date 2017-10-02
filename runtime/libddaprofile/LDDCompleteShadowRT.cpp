#include "LDDCompleteShadowRT.h"
#include "LDDShadowCommonRT.h"



#include<stdio.h>
#include<stdlib.h>
#include<fstream>
#include<unistd.h>

//#include"llvm/Support/raw_ostream.h"
//#include "llvm/Support/Debug.h"
//
//
//

#define ADDRMASK 0xFFFFFFF8
#define AddrInnerOffsetMASK 0x0000000000000007



long access_offsetMASK = 0 ;
void **shadow;

// Commnunicate with instrumentation.
int LDDCSProfLoopID = 0; // Labeled entering the profiling scope and current ProfilingLoopID
// Not used now.
//std::map<int, int> MaxProfilingLoopID; // Record current max Profiled Loop ID.
int LDDCSProfLoopIter = 0;//=0
long shadow_num = 0;

//int ProfilingLoopID = 0; // not add now.

std::set<void**> shadow_addr_Array;

std::map<int , LIDepInfo>  LIDepResult; //当前表
LDDepInfo LDDCSResult;

#define N 10000
#define TRUEN  1000 
#define ANTIN  1000 
#define OUTPUTN  1000 
//std::set<unsigned> DepDist;  // Dependence distance.

//using namespace llvm;
//Define operator <() for set<shadow_addr_Array>.
#if 0
bool operator< (void** shadow1, void** shadow2)
{
	return *shadow1<*shadow2;
}
#endif

void __initCompleteShadowMemory()
{

  std::cout<< std::setbase(16)  << "ShadowBeg = "<< LinuxCShadowBeg <<" , ShadowEnd = " 
          << LinuxCShadowEnd << "\n";
  uptr Shadow = (uptr) __mmapFixedReserveVMA( LinuxCShadowBeg, LinuxCShadowEnd -
                LinuxCShadowBeg );
  if( Shadow != LinuxCShadowBeg ){
    std::cout << "Error: LDDDALShadow cannot mmap the shadow memory \n";
    std::cout << "Error: Make sure to compile with -fPIE and  "
              << "to link with -pie \n";

    _exit(1); // bug with gcc4.7.0
  }
  
}


void __finiCompleteShadowMemory()
{
  __internal_munmap( (void*) LinuxCShadowBeg, 
                    LinuxCShadowEnd - LinuxCShadowBeg);
 
  return ;
}


// Reserve virtual memory space for shadow.
//
//
void __initLDDCompleteShadow()
{
  static bool is_initialized = false; 

  if( is_initialized )
    return;

  is_initialized = true;

  // Not used now.
  //__initPlatform();

  __initCompleteShadowMemory();

 printf("size = %d \n", LDDCSResult.size() ); 

  return ;
}

void __finiLDDCompleteShadow()
{
  __finiCompleteShadowMemory();

  return;

}



// Define operator <() for set<LIDep>.
bool operator< ( Dep LI1, Dep LI2)
{
  //return ((LI1.SrcAddrPos < LI2.SrcAddrPos) || (LI1.SrcVarName < LI2.SrcVarName) || (LI1.SinkAddrPos < LI2.SinkAddrPos) || (LI1.SinkVarName < LI2.SinkVarName) );
  return ((LI1.SrcAddrPos < LI2.SrcAddrPos) || (LI1.SrcAddrPos == LI2.SrcAddrPos && LI1.SrcVarName < LI2.SrcVarName) || ( LI1.SrcAddrPos == LI2.SrcAddrPos &&
 LI1.SrcVarName == LI2.SrcVarName && LI1.SinkAddrPos < LI2.SinkAddrPos) || ( LI1.SrcAddrPos == LI2.SrcAddrPos && LI1.SrcVarName == LI2.SrcVarName && LI1.SinkAddrPos == LI2.SinkAddrPos && LI1.SinkVarName < LI2.SinkVarName) ); 
}  
// Define operator <() for vector<PrecedRWAddrID>.
bool operator< ( PrecedRWAddrID PreRW1, PrecedRWAddrID PreRW2)
{
	return (PreRW1.AddrPos < PreRW2.AddrPos) || (PreRW1.VarName < PreRW2.VarName) || (PreRW1.WAddrLen < PreRW2.WAddrLen);
}

// Define operator <() for map<Dep, set<int> >.
#if 0
bool operator< (LCDepInfo LC1, LCDepInfo LC2)
{
	return ((LI1.SrcAddrPos <= LI2.SrcAddrPos) || (LI1.SrcVarName <= LI2.SrcVarName) || (LI1.SinkAddrPos <= LI2.SinkAddrPos) || (LI1.SinkVarName <= LI2.SinkVarName) );
}
#endif

/* Compute deps imediately after a read operation.
**
**
*/
void analyzeReadAddrDepSet(void** CurRWAddrNode,std::string AddrPos,std::string VarName,int Keyword)
{
   std::cout<<(((RWTraceInfo*)*CurRWAddrNode)->AddrWflag & Keyword )<<std::endl;
   if( (((RWTraceInfo*)*CurRWAddrNode)->AddrWflag & Keyword !=0) && ((RWTraceInfo*)*CurRWAddrNode)->Present[LDDCSProfLoopIter].RWIterNo==LDDCSProfLoopIter) //如果当前迭代 这个位置被写过 则产生无关TrueDep
	{ //  
		  std::cout<<"analyzeReadAddrDepSet"<<std::endl;
		  std::map<int,PresentRWAddrID>::iterator RB,RE;
		  RB = ((RWTraceInfo*)(*CurRWAddrNode))->Present[LDDCSProfLoopIter].WTrace.begin();
	    RE = ((RWTraceInfo*)(*CurRWAddrNode))->Present[LDDCSProfLoopIter].WTrace.end();
	    if(((RWTraceInfo*)*CurRWAddrNode)->Present[LDDCSProfLoopIter].WTrace.size()==0) return;
	    
		for(; RB!=RE ;RB++)
		{
			if(((RB->first) & Keyword) !=0){
				//std::cout<<"analyzeWriteAddrDepSet ID = " <<  LDDCSProfLoopID <<std::endl;
				Dep Deps;
				Deps.SrcAddrPos =   RB->second.AddrPos;
				Deps.SinkAddrPos = AddrPos; 
				Deps.SrcVarName =   RB->second.VarName; 
				Deps.SinkVarName =  VarName;
				LIDepResult[LDDCSProfLoopID].TrueDep.insert(Deps);
				std::cout<<"analyzeReadAddrDepSet ID = " <<  LDDCSProfLoopID << "----------size = " << LIDepResult[LDDCSProfLoopID].TrueDep.size() <<std::endl;
				// LDDCSProfLoopIter =1  !
			}
		}
	}
   return ;
}
void analyzeWriteAddrDepSet(void** CurRWAddrNode,std::string AddrPos,std::string VarName, int Keyword)
{
	  std::cout<<(((RWTraceInfo*)*CurRWAddrNode)->AddrWflag & Keyword )<<std::endl;
   if( (((RWTraceInfo*)*CurRWAddrNode)->AddrWflag & Keyword !=0) && ((RWTraceInfo*)*CurRWAddrNode)->Present[LDDCSProfLoopIter].RWIterNo==LDDCSProfLoopIter) //如果当前迭代 这个位置被写过 则产生无关OutDep
   {  
   	  
	    std::map<int,PresentRWAddrID>::iterator RB,RE;
		  RB = ((RWTraceInfo*)*CurRWAddrNode)->Present[LDDCSProfLoopIter].WTrace.begin();
	    RE = ((RWTraceInfo*)*CurRWAddrNode)->Present[LDDCSProfLoopIter].WTrace.end();
		  for(; RB!=RE ;RB++)
		  {
		    
			  if((RB->first & Keyword) !=0){
				Dep Deps;
				Deps.SrcAddrPos =   RB->second.AddrPos;
				Deps.SinkAddrPos = AddrPos; 
				Deps.SrcVarName =   RB->second.VarName; 
				Deps.SinkVarName =  VarName;
				LIDepResult[LDDCSProfLoopID].OutDep.insert(Deps);
				// LDDCSProfLoopIter = 1  
			}
		}
    }
  
    if(((RWTraceInfo*)*CurRWAddrNode)->Present[LDDCSProfLoopIter].RTrace.size()==0) return;
     std::cout<<(((RWTraceInfo*)*CurRWAddrNode)->AddrRflag & Keyword )<<std::endl;
    if( (((RWTraceInfo*)*CurRWAddrNode)->AddrRflag & Keyword !=0)&& ((RWTraceInfo*)*CurRWAddrNode)->Present[LDDCSProfLoopIter].RWIterNo==LDDCSProfLoopIter) //如果当前迭代 这个位置被读过 则产生无关AntiDep
    { 
		  std::map<int,PresentRWAddrID>::iterator RB,RE;
		  RB = ((RWTraceInfo*)*CurRWAddrNode)->Present[LDDCSProfLoopIter].RTrace.begin();
	    RE = ((RWTraceInfo*)*CurRWAddrNode)->Present[LDDCSProfLoopIter].RTrace.end();
		for(; RB!=RE ;RB++)
		{
			
			if((RB->first & Keyword) !=0){
				std::cout<<"analyzeWriteAddrDepSet ID = ***" <<  LDDCSProfLoopID <<std::endl;
				Dep Deps;
				Deps.SrcAddrPos =   RB->second.AddrPos;
				Deps.SinkAddrPos = AddrPos;
				Deps.SrcVarName =   RB->second.VarName; 
				Deps.SinkVarName =  VarName;
				LIDepResult[LDDCSProfLoopID].AntiDep.insert(Deps);
				std::cout<<"analyzeWriteAddrDepSet ID = " <<  LDDCSProfLoopID << "*********size = " << LIDepResult[LDDCSProfLoopID].AntiDep.size() <<std::endl;
				// LDDCSProfLoopIter =1  !
			}
		}
	}
    return ;
}

void __shadowR(long *ptr, long AddrLen, char *Pos, char* Name,int AddrInnerOffset,void **shadow_addr,int Keyword)
{
	  int AddrInnerOffsetTwo = AddrInnerOffset;
	  if(shadow_addr == NULL)
	  {
		   (*shadow_addr) = (void*)new RWTraceInfo;
		   PresentRWTrace PresentRW;
		   PresentRW.RTrace[Keyword].AddrPos=Pos;
		   PresentRW.RTrace[Keyword].VarName=Name;
		   PresentRW.RTrace[Keyword].RAddrLen=AddrLen;
		   PresentRW.RWIterNo = LDDCSProfLoopIter;
		  ((RWTraceInfo*)*shadow_addr)->Present[LDDCSProfLoopIter] = PresentRW;
		  ((RWTraceInfo*)*shadow_addr)->AddrRflag = Keyword;
			shadow_addr_Array.insert(shadow_addr) ;
		}//对于当前迭代 多个读之后才有写 并且第一个读没有二次shadow 
	 else { 
		  analyzeReadAddrDepSet(shadow_addr,Pos,Name,Keyword);
		  std::map<int,PresentRWAddrID>::iterator RB,RE;
		  RB = ((RWTraceInfo*)*shadow_addr)->Present[LDDCSProfLoopIter].RTrace.begin();
	         RE = ((RWTraceInfo*)*shadow_addr)->Present[LDDCSProfLoopIter].RTrace.end();
		  int Read_offset_addr = 0 ;
		  for( ; RB !=RE ; RB++)
		  {
     			Read_offset_addr = Keyword ^ RB->first;
				  Keyword = Read_offset_addr & Keyword;
		  }
		  if(Keyword != 0) 
	     {
		     ((RWTraceInfo*)*shadow_addr)->Present[LDDCSProfLoopIter].RTrace[Keyword].AddrPos=Pos;
			   ((RWTraceInfo*)*shadow_addr)->Present[LDDCSProfLoopIter].RTrace[Keyword].VarName=Name;
     		 ((RWTraceInfo*)*shadow_addr)->Present[LDDCSProfLoopIter].RTrace[Keyword].RAddrLen=AddrInnerOffsetTwo;
			   ((RWTraceInfo*)*shadow_addr)->Present[LDDCSProfLoopIter].RWIterNo=LDDCSProfLoopIter;
			 
			   ((RWTraceInfo*)*shadow_addr)->AddrRflag = ((RWTraceInfo*)*shadow_addr)->AddrRflag | Keyword;
	      }		  
		}
	  return ;
}

/*  __loadcheck: collect and analysis at the same time.
** Input: Addr, AddrLen, AddrPos=FileName_LineNo, VarName, LoopID, IterID. 
** Parameter: Addr, AddrLen, AddrPos, VarName.
** 
*/
void __checkLoadCShadow(long *ptr, long AddrLen, char *Pos, char* Name)
{
  if( LDDCSProfLoopID == 0 )
    return;
   std::string AddrPos(Pos);
   std::string VarName(Name);
   RWTraceInfo *CurRWAddrNode;
    std::cout<< "999999999999 bLOad ID = " << LDDCSProfLoopID << "\n";
 
   void **shadow_addr; 
   shadow_addr = (void**)AppAddrToShadow( long(ptr) );
 //  std::cout<<ptr<<" "<<shadow_addr<<" "<<std::endl;
   

   unsigned long AddrInnerOffset = long(ptr) & AddrInnerOffsetMASK;
   unsigned long Keyword; 
   long AddrLen_shadow1;

   if(AddrInnerOffset + AddrLen > 8)
   {
     Keyword = int(1<<(8) - 1<<AddrInnerOffset);
     AddrLen_shadow1 = 8-AddrInnerOffset;
     //std::cout<<"__checkLoadCShadow ptr = "<< ptr << "InnferOffset =  "<< 
      //    AddrInnerOffset << "keyword" << Keyword <<std::endl;
   }
   else
     {
		 Keyword = (unsigned int)( ( 1<<(AddrInnerOffset + (unsigned long)AddrLen) ) - (1<<AddrInnerOffset) );
		 AddrLen_shadow1 = AddrLen;
    std::cout<<"__checkLoadCShadow ptr = "<< ptr << "lenght = " << AddrLen << "  InnferOffset =  "<<
          AddrInnerOffset << " keyword = " << Keyword <<std::endl;       
     }
     
     
   if( (*shadow_addr) == NULL)  //The first visit addr
   {    // 进去了！！！！！！！！！！！！这里应该是总共新地址的个数！！ 包括读和写总共7个？？？？？
	//  std::cout<<"W_checkLoad_NULL"<<std::endl;
	  (*shadow_addr) = (void*)new RWTraceInfo;                 //  This difinitions is ture  Why
	
	  PresentRWTrace PresentRW;
	  PresentRW.RTrace[Keyword].AddrPos =Pos;
	  PresentRW.RTrace[Keyword].VarName =Name;
	  PresentRW.RTrace[Keyword].RAddrLen =AddrLen_shadow1;
	  PresentRW.RWIterNo = LDDCSProfLoopIter;
	  ((RWTraceInfo*)*shadow_addr)->Present[LDDCSProfLoopIter] = PresentRW;
         //((RWTraceInfo*)*shadow_addr)->Present.insert(std::make_pair(LDDCSProfLoopIter, PresentRW) );

	  ((RWTraceInfo*)*shadow_addr)->AddrRflag = Keyword;
	  shadow_addr_Array.insert(shadow_addr);  //把地址放入 shadow_addr_Array
	  
	  if(AddrInnerOffset + AddrLen > 8)
	  {    // 没有进去！！！！！！！！！！！！！！所以说没有二次shadow的地址？？那么地址只有7个？？
	 // 	std::cout<<"hell word!"<<std::endl;
		   int AddrInnerOffsetTwo = (AddrInnerOffset + AddrLen)%8;
		   Keyword = int(1<<(0 + int(AddrInnerOffsetTwo)) - 1<<0);
		   __shadowR(ptr, AddrInnerOffset + AddrLen - 8 , Pos, Name, 0, shadow_addr+1, Keyword);
	  }
  }
  else
  {      // 进去了！！！！！！！出现了很多输出！！！！！！！
	 // std::cout<<"W_checkLoad_NULL_else"<<std::endl;
	  if(((RWTraceInfo*)*shadow_addr)->Present[LDDCSProfLoopIter].RWIterNo == LDDCSProfLoopIter)
	  {
	  	std::cout<<"W_checkLoad_NULL_else"<<std::endl;
		  {
			 __shadowR(ptr, AddrLen_shadow1, Pos, Name, AddrInnerOffset, shadow_addr,Keyword);
			  if(AddrInnerOffset + AddrLen > 8){
				  int AddrInnerOffsetTwo = (AddrInnerOffset + AddrLen)%8;
				  Keyword = int(1<<(0 + AddrInnerOffsetTwo) - 1<<0);
		          __shadowR(ptr, AddrInnerOffset + AddrLen - 8, Pos, Name, 0, shadow_addr+1,Keyword);
	          }
		  }
	  }
	  else
      {
		  ((RWTraceInfo*)*shadow_addr)->Present[LDDCSProfLoopIter].RTrace.clear();
		  ((RWTraceInfo*)*shadow_addr)->Present[LDDCSProfLoopIter].WTrace.clear();
		  
		  ((RWTraceInfo*)*shadow_addr)->Present[LDDCSProfLoopIter].RTrace[Keyword].AddrPos=Pos;
		  ((RWTraceInfo*)*shadow_addr)->Present[LDDCSProfLoopIter].RTrace[Keyword].VarName=Name;
     	((RWTraceInfo*)*shadow_addr)->Present[LDDCSProfLoopIter].RTrace[Keyword].RAddrLen=AddrInnerOffset;
		  ((RWTraceInfo*)*shadow_addr)->Present[LDDCSProfLoopIter].RWIterNo=LDDCSProfLoopIter;
		  ((RWTraceInfo*)*shadow_addr)->AddrRflag = Keyword;
		  if(AddrInnerOffset + AddrLen > 8){
				int AddrInnerOffsetTwo = (AddrInnerOffset + AddrLen)%8;
				Keyword = int(1<<(0 + AddrInnerOffsetTwo) - 1<<0);
		        __shadowR(ptr, AddrInnerOffset + AddrLen - 8, Pos, Name, 0 , shadow_addr+1,Keyword);
	      }
		  //这种情况下没有 无关依赖 。 只有在迭代结束时 加入历史表 并且建立相关依赖
	  }
  }
  return ;
}


void __shadowW( long AddrLen, char *Pos, char* Name,void **shadow_addr,int Keyword)
{
	
	  if(*shadow_addr == NULL)
	  {
		  std::cout<<"W_shadowW_two_NULL"<<std::endl;        // come in

		  (*shadow_addr) = (void*)new RWTraceInfo;
		   PresentRWTrace PresentRW;
	          PresentRW.WTrace[Keyword].AddrPos=Pos;
	          PresentRW.WTrace[Keyword].VarName=Name;
		   PresentRW.WTrace[Keyword].RAddrLen=AddrLen;
		   PresentRW.RWIterNo=LDDCSProfLoopIter;
		   ((RWTraceInfo*)*shadow_addr)->Present[LDDCSProfLoopIter] = PresentRW;
		   ((RWTraceInfo*)*shadow_addr)->AddrWflag = Keyword;
		   shadow_addr_Array.insert(shadow_addr); 
          //         std::cout<<"W_地址"<<std::endl;
	 }
     else {
      		  std::cout<<"W_shadowW_two_NULL"<<std::endl;    //     只是进入这里了》》》》》》》》
		  analyzeWriteAddrDepSet(shadow_addr,Pos,Name,Keyword);
		  
		  std::cout<<"W_shadowW_two_NULL"<<std::endl;
		  std::map<int,PresentRWAddrID>::iterator RB,RE;
		  RB = ((RWTraceInfo*)*shadow_addr)->Present[LDDCSProfLoopIter].WTrace.begin();
	          RE = ((RWTraceInfo*)*shadow_addr)->Present[LDDCSProfLoopIter].WTrace.end();
		  int Read_offset_addr ;
		  for( ; RB !=RE ; RB++) //最后一次写！！！！！！！！！！！
		  {  // 进去了！！！！！！
			      Read_offset_addr = Keyword & RB->first;
				if((~Read_offset_addr) & RB->first == 0)
				{ // 进去了！！！！！！！！！
					((RWTraceInfo*)*shadow_addr)->Present[LDDCSProfLoopIter].WTrace.erase( RB );
				}
				else{//  进去了！！！！！！！！！！！！！
					PresentRWAddrID PrecedRW = RB->second;
					((RWTraceInfo*)*shadow_addr)->Present[LDDCSProfLoopIter].WTrace.erase( RB );
					((RWTraceInfo*)*shadow_addr)->Present[LDDCSProfLoopIter].WTrace[(~Read_offset_addr) & RB->first] = PrecedRW;
				}
		  }
		  if(Keyword != 0)  // 要写的 在当前迭代中有没有写的 字节 
	      { // 进去了！！！！！！！！！！！！！！！！！！
		     ((RWTraceInfo*)*shadow_addr)->Present[LDDCSProfLoopIter].WTrace[Keyword].AddrPos=Pos;
			   ((RWTraceInfo*)*shadow_addr)->Present[LDDCSProfLoopIter].WTrace[Keyword].VarName=Name;
     		 ((RWTraceInfo*)*shadow_addr)->Present[LDDCSProfLoopIter].WTrace[Keyword].RAddrLen=AddrLen;
			   ((RWTraceInfo*)*shadow_addr)->Present[LDDCSProfLoopIter].RWIterNo=LDDCSProfLoopIter;
			   ((RWTraceInfo*)*shadow_addr)->AddrWflag |= Keyword; 
	      }		  
	  }
	  return ;
}
//  Update iterNo, addr of  memRW .
//  Add output-, anti-  dependencies. 
void __checkStoreCShadow(long *ptr, long AddrLen, char* Pos, char* Name)
{
	  
    std::cout<< "99999999999  bStore ID = " << LDDCSProfLoopID << "\n";
    if(!LDDCSProfLoopID)
    return;
    std::string AddrPos(Pos);
    std::string VarName(Name); 

    RWTraceInfo *CurRWAddrNode;
    
    void **shadow_addr;
   
    shadow_addr = (void**)AppAddrToShadow( long(ptr) ); 

   unsigned long AddrInnerOffset = long(ptr) & AddrInnerOffsetMASK;

   unsigned long Keyword;  
   int AddrLen_shadow1;
   if(AddrInnerOffset + AddrLen > 8)
   {
	   Keyword = int(1<<(8) - 1<<AddrInnerOffset);//找到对应的关键字
	   AddrLen_shadow1 = 8-AddrInnerOffset;
	   std::cout<<"__checkStoreCShadow "<<(Keyword) <<" if"<<std::endl;
   }
   else
   {
	   Keyword = (unsigned long)((1<<(AddrInnerOffset + (unsigned long)AddrLen)) - (1<<AddrInnerOffset));
	   AddrLen_shadow1 = AddrLen;
	   std::cout<<"__checkStoreCShadow "<<Keyword <<"else "<<std::endl;
	   	 std::cout<<"__checkLoadCShadow ptr = "<< ptr << "lenght = " << AddrLen << "  InnferOffset =  "<<
          AddrInnerOffset << " keyword = " << Keyword <<std::endl;    
   }
   
   
   
   if(*shadow_addr==NULL)  //The first visit addr
   {
	        std::cout<<"W_checkStoreCShadow"<<std::endl; //  这里没有进入 ？？？？？？？？
	      __shadowW( AddrLen_shadow1, Pos,  Name, shadow_addr, Keyword);
          if(AddrInnerOffset + AddrLen > 8)
	      {
		   int AddrInnerOffsetTwo  = (AddrInnerOffset + AddrLen)%8;
		   Keyword = int(1<<(0 + AddrInnerOffsetTwo) - 1<<0);
		   __shadowW( AddrInnerOffset + AddrLen - 8, Pos, Name , shadow_addr+1,Keyword);
          }
   }
   else
   {
   	 
	  if(((RWTraceInfo*)*shadow_addr)->Present[LDDCSProfLoopIter].RWIterNo == LDDCSProfLoopIter)
	  {
          std::cout<<"W_地址_二次"<<std::endl;
			 __shadowW( AddrLen_shadow1, Pos, Name,  shadow_addr,Keyword);
			  if(AddrInnerOffset + AddrLen > 8){
			  	std::cout<<"W_地址_二次"<<std::endl;
				  int AddrInnerOffsetTwo = (AddrInnerOffset + AddrLen)%8;
				  Keyword = int(1<<(0 + AddrInnerOffsetTwo) - 1<<0);
		          __shadowW( AddrInnerOffset + AddrLen - 8, Pos, Name,  shadow_addr+1,Keyword);
	          }
	  }
	  else
      {//不是当前迭代
		//  std::cout<<"W_地址_二次"<<std::endl;    这里也没有进入？？？？？？？？
		  ((RWTraceInfo*)*shadow_addr)->Present[LDDCSProfLoopIter].RTrace.clear();
		  ((RWTraceInfo*)*shadow_addr)->Present[LDDCSProfLoopIter].WTrace.clear();
		  
		  ((RWTraceInfo*)*shadow_addr)->Present[LDDCSProfLoopIter].WTrace[Keyword].AddrPos=Pos;
		  ((RWTraceInfo*)*shadow_addr)->Present[LDDCSProfLoopIter].WTrace[Keyword].VarName=Name;
     	         ((RWTraceInfo*)*shadow_addr)->Present[LDDCSProfLoopIter].WTrace[Keyword].RAddrLen=AddrInnerOffset;
		  ((RWTraceInfo*)*shadow_addr)->Present[LDDCSProfLoopIter].RWIterNo=LDDCSProfLoopIter;
		  ((RWTraceInfo*)*shadow_addr)->AddrWflag = Keyword;
		  if(AddrInnerOffset + AddrLen > 8){
				int AddrInnerOffsetTwo = (AddrInnerOffset + AddrLen)%8;
				Keyword = int(1<<(0 + AddrInnerOffsetTwo) - 1<<0);
		        __shadowW( AddrInnerOffset + AddrLen - 8, Pos, Name, shadow_addr+1,Keyword);
	      }
		  //  LDDCSProfLoopIter  为常数 1
		  //当前迭代 对这个地址的第一次 访问！
		  //这种情况下没有 无关依赖 。 只有在迭代结束时 加入历史表 并且建立相关依赖
	  }
   }
  return ;
}


// Write the dependence set to the stdout in default. 
void __outputLDDCSDependence()
{

   std::fstream fs;
   fs.open("dda-result.txt", std::fstream::out);
   if(! fs.is_open())
    std::cout<<"cannot open file dda-result"<< std::endl;

   //  std::cout<<"W_addpendingtohistory"<<std::endl;
   std::map<int, DepInfo>::iterator LDDB, LDDE;
   std::set<Dep>::iterator LIDB, LIDE;
   std::map<Dep, std::set<int> >::iterator LCDB, LCDE;
   std::set<int>::iterator DistB, DistE;
   std::map<int , LIDepInfo>::iterator  LIDepiterB,LIDepiterE;
   // 这里把当前表 输出 ， 输出当前pending   LIDepResult
   if(LIDepResult.size()!=0 )
   {
	   LIDepiterB = LIDepResult.begin();
	   LIDepiterE = LIDepResult.end();
	   for(; LIDepiterB != LIDepiterE; LIDepiterB++){
		   int LDDCSProfLoopID_now = LIDepiterB->first;
		   std::cout<<"LIDepResult_LDDCSProfLoopID_now"<<LDDCSProfLoopID_now<<std::endl;
	   if( LIDepResult[LDDCSProfLoopID_now].TrueDep.size()!=0)
		{
			for(LIDB = LIDepResult[LDDCSProfLoopID_now].TrueDep.begin(),LIDE = LIDepResult[LDDCSProfLoopID_now].TrueDep.end();LIDB!=LIDE;LIDB++){ 
			std::cout<<LIDB->SrcAddrPos<<" "<<LIDB->SrcVarName<<" "<<LIDB->SinkAddrPos<<" "<<LIDB->SinkVarName<< "True "<<std::endl;
			}
		}
	    std::cout<<"TrueDep.size()"<<LIDepResult[LDDCSProfLoopID_now].TrueDep.size()<<std::endl;
		if( LIDepResult[LDDCSProfLoopID_now].OutDep.size()!=0)
		{
   			std::cout<<"W___outputLDDCSDependence"<<std::endl;
			for(LIDB = LIDepResult[LDDCSProfLoopID_now].OutDep.begin(),LIDE = LIDepResult[LDDCSProfLoopID_now].OutDep.end();LIDB!=LIDE;LIDB++){ 
			std::cout<<LIDB->SrcAddrPos<<" "<<LIDB->SrcVarName<<" "<<LIDB->SinkAddrPos<<" "<<LIDB->SinkVarName<< "Out "<<std::endl;
			}
		}
		if(LIDepResult[LDDCSProfLoopID_now].AntiDep.size()!=0)
		{
   			std::cout<<"W___outputLDDCSDependence"<<std::endl;
			for(LIDB = LIDepResult[LDDCSProfLoopID_now].AntiDep.begin(),LIDE = LIDepResult[LDDCSProfLoopID_now].AntiDep.end();LIDB!=LIDE;LIDB++){ 
			std::cout<<LIDB->SrcAddrPos<<" "<<LIDB->SrcVarName<<" "<<LIDB->SinkAddrPos<<" "<<LIDB->SinkVarName<< "Anti "<<std::endl;
			}
		}
	   }
   }
   // 这里把相关依赖 输出
  
  if(LDDCSResult.size()!=0)
  {
  	  
	  for( LDDB = LDDCSResult.begin(), LDDE = LDDCSResult.end(); LDDB != LDDE; ++LDDB){
		fs<< LDDB->second.FuncName << " " << LDDB->second.LoopPos << std::endl;
		std::cout<< LDDB->second.FuncName << " " << LDDB->second.LoopPos << std::endl;
			std::cout<<" LDDB->second.FuncName "<<std::endl;
       int LDDCSProfLoopID_now = LDDB->first; 
       std::cout<<"LDDCSResult_LDDCSProfLoopID_now"<<LDDCSProfLoopID_now<<std::endl;
		 // 2) Output loop carried deps.
		   std::cout<<"LDDCSResult[LDDCSProfLoopID_now].LDDep.TrueDep.size() "<<LDDCSResult[LDDCSProfLoopID_now].LDDep.TrueDep.size()<<std::endl;
	 //  if(LDDCSResult[LDDCSProfLoopID_now].LDDep.TrueDep.size()!=0)
		   for(LCDB = LDDCSResult[LDDCSProfLoopID_now].LDDep.TrueDep.begin(),LCDE = LDDCSResult[LDDCSProfLoopID_now].LDDep.TrueDep.end();LCDB!=LCDE;LCDB++)
		   {
				if( LCDB->first.SrcVarName == "s.addr")
				break;
				fs << LCDB->first.SrcAddrPos<<" "<<LCDB->first.SrcVarName<< " " << LCDB->first.SinkAddrPos<<" "<< LCDB->first.SinkVarName<< " True" <<std::endl;
				std::cout<< LCDB->first.SrcAddrPos<<" "<<LCDB->first.SrcVarName<< " " << LCDB->first.SinkAddrPos<<" "<< LCDB->first.SinkVarName<< " True" <<std::endl;
				// Dependence distance.
		         for(DistB = LCDB->second.begin(), DistE = LCDB->second.end(); DistB != DistE; ++DistB)
			     fs << " " << *DistB;
			     fs << std::endl;
		    }
   //     if(LDDCSResult[LDDCSProfLoopID_now].LDDep.AntiDep.size()!=0)
		   for(LCDB = LDDCSResult[LDDCSProfLoopID_now].LDDep.AntiDep.begin(),LCDE = LDDCSResult[LDDCSProfLoopID_now].LDDep.AntiDep.end();LCDB!=LCDE;LCDB++)
		   {
				if( LCDB->first.SrcVarName == "s.addr")
				break;
				fs << LCDB->first.SrcAddrPos<<" "<<LCDB->first.SrcVarName<< " " << LCDB->first.SinkAddrPos<<" "<< LCDB->first.SinkVarName<< " AntiDep" <<std::endl;
				std::cout<< LCDB->first.SrcAddrPos<<" "<<LCDB->first.SrcVarName<< " " << LCDB->first.SinkAddrPos<<" "<< LCDB->first.SinkVarName<< " AntiDep" <<std::endl;
				// Dependence distance.
		         for(DistB = LCDB->second.begin(), DistE = LCDB->second.end(); DistB != DistE; ++DistB)
			     fs << " " << *DistB;
			     fs << std::endl;
		    }
	 //   	if(LDDCSResult[LDDCSProfLoopID_now].LDDep.OutDep.size()!=0)
		   for(LCDB = LDDCSResult[LDDCSProfLoopID_now].LDDep.OutDep.begin(),LCDE = LDDCSResult[LDDCSProfLoopID_now].LDDep.OutDep.end();LCDB!=LCDE;LCDB++)
		   {
				if( LCDB->first.SrcVarName == "s.addr")
				break;
				fs << LCDB->first.SrcAddrPos<<" "<<LCDB->first.SrcVarName<< " " << LCDB->first.SinkAddrPos<<" "<< LCDB->first.SinkVarName<< " OutDep" <<std::endl;
				std::cout<< LCDB->first.SrcAddrPos<<" "<<LCDB->first.SrcVarName<< " " << LCDB->first.SinkAddrPos<<" "<< LCDB->first.SinkVarName<< " OutDep" <<std::endl;
				// Dependence distance.
		         for(DistB = LCDB->second.begin(), DistE = LCDB->second.end(); DistB != DistE; ++DistB)
			     fs << " " << *DistB;
			     fs << std::endl;
		    }
	}
  }
  fs.close();
}


void addpendingtohistory()
{     
	 if( LDDCSProfLoopIter ==1)
	 return ;
   std::cout<<"LDDCSProfLoopIter = " << LDDCSProfLoopIter <<"loop id = " <<  LDDCSProfLoopID <<std::endl;
	 
	 std::map<int,PresentRWAddrID>::iterator B, E;
	 std::map<int,PrecedRWAddrID>::iterator Btow,Etow;
	 
	 
	 RWTraceInfo *CurRWAddrNode;
   //   void **shadow_addr = NULL;
    
	 int RWIterNo;
	 std::set<void**>::iterator shadow_addr_Array_B, shadow_addr_Array_E;
	 shadow_addr_Array_B = shadow_addr_Array.begin();
	 shadow_addr_Array_E = shadow_addr_Array.end();
	 if(shadow_addr_Array.size()==0) return ;
	 for( ; shadow_addr_Array_B != shadow_addr_Array_E; shadow_addr_Array_B++)
	 {
		if(((RWTraceInfo*)(**shadow_addr_Array_B) )->Precede.RTrace.size()!=0)
		{
			Btow=((RWTraceInfo*)(**shadow_addr_Array_B))->Precede.RTrace.begin();
			Etow=((RWTraceInfo*)(**shadow_addr_Array_B))->Precede.RTrace.end();
		//	B=((RWTraceInfo*)(**shadow_addr_Array_B) )->Present[LDDCSProfLoopIter].WTrace.begin();
			E=((RWTraceInfo*)(**shadow_addr_Array_B))->Present[LDDCSProfLoopIter].WTrace.end();
			RWIterNo=((RWTraceInfo*)(**shadow_addr_Array_B))->Present[LDDCSProfLoopIter].RWIterNo;
		 // 这里是否也要判断 fize() ?????????
			for(Btow=((RWTraceInfo*)(**shadow_addr_Array_B))->Precede.RTrace.begin();Btow!=Etow;Btow++)
			{
				
				if( ((Btow->first) & ((RWTraceInfo*)(**shadow_addr_Array_B))->AddrWflag) == 0)
				continue;
				for(B=((RWTraceInfo*)(**shadow_addr_Array_B))->Present[LDDCSProfLoopIter].WTrace.begin(); B!=E; B++)
				if((B->first & Btow->first) !=0)
				{
					Dep Deps;
					Deps.SrcAddrPos =  B->second.AddrPos;
					Deps.SinkAddrPos = Btow->second.AddrPos; 
					Deps.SrcVarName = B->second.VarName;
					Deps.SinkVarName =  Btow->second.VarName;
					int Dist = RWIterNo - Btow->second.RWIterNo;
                 std::cout<<Deps.SrcVarName<<" "<<Deps.SinkVarName<<" 222"<<std::endl;
					LDDCSResult[LDDCSProfLoopID].LDDep.AntiDep[Deps].insert(Dist);
				}
			}
		 }

			if(((RWTraceInfo*)(**shadow_addr_Array_B))->Precede.WTrace.size()!=0)
			{
				RWIterNo = ((RWTraceInfo*)(**shadow_addr_Array_B))->Present[LDDCSProfLoopIter].RWIterNo;
				Btow = ((RWTraceInfo*)(**shadow_addr_Array_B))->Precede.WTrace.begin();
				Etow = ((RWTraceInfo*)(**shadow_addr_Array_B))->Precede.WTrace.end();
				B=((RWTraceInfo*)(**shadow_addr_Array_B))->Present[LDDCSProfLoopIter].WTrace.begin();
				E=((RWTraceInfo*)(**shadow_addr_Array_B))->Present[LDDCSProfLoopIter].WTrace.end();
				for(Btow=((RWTraceInfo*)(**shadow_addr_Array_B))->Precede.WTrace.begin();Btow!=Etow;Btow++)
				{
				if( ((Btow->first) & ((RWTraceInfo*)(**shadow_addr_Array_B))->AddrWflag) ==0 )
				continue;
				for(B=((RWTraceInfo*)(**shadow_addr_Array_B))->Present[LDDCSProfLoopIter].WTrace.begin(); B!=E; B++)
				if((B->first & Btow->first) !=0)
				{
					Dep Deps;
					Deps.SrcAddrPos =  B->second.AddrPos;
					Deps.SinkAddrPos = Btow->second.AddrPos;
					Deps.SrcVarName = B->second.VarName;
					Deps.SinkVarName =  Btow->second.VarName;
					int Dist = RWIterNo - Btow->second.RWIterNo;
                                        std::cout<<Deps.SrcVarName<<" "<<Deps.SinkVarName<<" 2"<<std::endl;
					LDDCSResult[LDDCSProfLoopID].LDDep.OutDep[Deps].insert(Dist);
				
				}
				}

				Btow=((RWTraceInfo*)(**shadow_addr_Array_B))->Precede.WTrace.begin();
				Etow=((RWTraceInfo*)(**shadow_addr_Array_B))->Precede.WTrace.end();
				B=((RWTraceInfo*)(**shadow_addr_Array_B))->Present[LDDCSProfLoopIter].RTrace.begin();
				E=((RWTraceInfo*)(**shadow_addr_Array_B))->Present[LDDCSProfLoopIter].RTrace.end();
				for(Btow=((RWTraceInfo*)(**shadow_addr_Array_B))->Precede.WTrace.begin();Btow!=Etow;Btow++)
				{
					if( ((Btow->first) & ((RWTraceInfo*)(**shadow_addr_Array_B))->AddrRflag) ==0)
						continue;
					for(B=((RWTraceInfo*)(**shadow_addr_Array_B))->Present[LDDCSProfLoopIter].RTrace.begin(); B!=E; B++)
					if((B->first & Btow->first) !=0)
					{
						 Dep Deps;
						 Deps.SrcAddrPos =  B->second.AddrPos;
						 Deps.SinkAddrPos = Btow->second.AddrPos;
						 Deps.SrcVarName = B->second.VarName;
						 Deps.SinkVarName =  Btow->second.VarName;
						 int Dist = RWIterNo - Btow->second.RWIterNo;
                                                 std::cout<<Deps.SrcVarName<<" "<<Deps.SinkVarName<<" 22"<<std::endl;
						 LDDCSResult[LDDCSProfLoopID].LDDep.TrueDep[Deps].insert(Dist);
					}
				}
			}

				//把pending表 加入历史表中 
				E=((RWTraceInfo*)(**shadow_addr_Array_B))->Present[LDDCSProfLoopIter].RTrace.end();
				if(((RWTraceInfo*)(**shadow_addr_Array_B))->Present[LDDCSProfLoopIter].RTrace.size()!=0)
				{
					for( B=((RWTraceInfo*)(**shadow_addr_Array_B))->Present[LDDCSProfLoopIter].RTrace.begin(); B!=E; B++)
					{
						PrecedRWAddrID PrecedRW;
						PrecedRW.AddrPos = B->second.AddrPos;
						PrecedRW.RAddrLen = B->second.RAddrLen;
						PrecedRW.VarName = B->second.VarName;
						PrecedRW.WAddrLen = B->second.WAddrLen;
						PrecedRW.RWIterNo = RWIterNo;
                                                std::cout<<PrecedRW.AddrPos<<" "<<PrecedRW.VarName<<" 1"<<std::endl;
						((RWTraceInfo*)(**shadow_addr_Array_B))->Precede.RTrace[B->first]=PrecedRW;
					}
				}
				E=((RWTraceInfo*)(**shadow_addr_Array_B))->Present[LDDCSProfLoopIter].WTrace.end();
				if(((RWTraceInfo*)(**shadow_addr_Array_B))->Present[LDDCSProfLoopIter].WTrace.size()!=0)
				{
				for(B=((RWTraceInfo*)(**shadow_addr_Array_B))->Present[LDDCSProfLoopIter].WTrace.begin(); B!=E; B++)
				{
					PrecedRWAddrID PrecedRW;
					PrecedRW.AddrPos = B->second.AddrPos;
					PrecedRW.RAddrLen = B->second.RAddrLen;
					PrecedRW.VarName = B->second.VarName;
					PrecedRW.WAddrLen = B->second.WAddrLen;
					PrecedRW.RWIterNo = RWIterNo;
                                      std::cout<<PrecedRW.AddrPos<<" "<<PrecedRW.VarName<<" 11"<<std::endl;
					((RWTraceInfo*)(**shadow_addr_Array_B))->Precede.WTrace[B->first]=PrecedRW;
				}
				}
	 }
	 return ;
}
long *nowptr; long nowAddrLen; char *nowPos; char *nowName;
void __checkLoadCShadowStackVar( long *ptr, long AddrLen, char *Pos, char* Name,  int TargetLoopFunc){
   if( !LDDCSProfLoopID )
      return;
    if(TargetLoopFunc)
    __checkLoadCShadow( ptr, AddrLen, Pos, Name);
    return;
}
void __checkStoreCShadowStackVar( long *ptr, long AddrLen, char *Pos, char* Name, int TargetLoopFunc){
   if( !LDDCSProfLoopID )
      return;
   if(TargetLoopFunc)
    __checkStoreCShadow(ptr, AddrLen, Pos, Name);
    return;
}
void __addnewDepInfo(int *DDAProfilingFlagnum , char* FuncName, char* LoopPos)
{
	//LDDCSProfLoopID = *DDAProfilingFlagnum;
	LDDCSResult[LDDCSProfLoopID].FuncName = FuncName;
	LDDCSResult[LDDCSProfLoopID].LoopPos = LoopPos;

	return ;
}
