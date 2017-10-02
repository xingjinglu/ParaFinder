//==- LDDLightShadow.cpp - Instrumentation for data dependence profiling -==//
////
////                      The LLVM Compiler Infrastructure
////
//// This file is distributed under the University of Illinois Open Source
//// License. See LICENSE.TXT for details.
////
////===--------------------------------------------------------------------===//
//
//  LDD: Loop Data Dependence
//===----------------------------------------------------------------------===//
//

#define DEBUG_TYPE "insert-dd-lshadow"
#include "llvm/DerivedTypes.h"

//#include "ProfilingUtils.h"
#include "llvm/Module.h"
#include "llvm/Function.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Instructions.h"
#include "llvm/LLVMContext.h"

#include "llvm/Pass.h"
#include "llvm/InstrTypes.h"
#include "llvm/TypeBuilder.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/CFG.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/InstIterator.h"

#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/SmallVector.h"


#include "llvm/Constants.h"
#include "llvm/IntrinsicInst.h"
#include "llvm/IRBuilder.h"
#include "llvm/Support/InstVisitor.h"
#include "llvm/DataLayout.h"
#include "llvm/Transforms/Instrumentation.h"


#include "llvm/DebugInfo.h"
#include "llvm/Metadata.h"
#include "llvm/Support/Debug.h"


#include "llvm/Analysis/LoopPass.h"
#include "llvm/Analysis/LoopInfoImpl.h"

// Two version codes.
#include "llvm/Transforms/Utils/BasicBlockUtils.h" // not used
#include "llvm/Transforms/Utils/CodeExtractor.h" // not used
#include "llvm/ADT/SetVector.h" // not used
#include "llvm/ADT/StringExtras.h" // not used
#include "llvm/Analysis/Dominators.h" // not used
#include "llvm/ADT/ilist.h"
#include "llvm/Transforms/Utils/Cloning.h" 
#include "llvm/Transforms/Utils/ValueMapper.h"



// Add global
#include "llvm/ADT/Twine.h"
#include "llvm/GlobalVariable.h"
#include"llvm/DerivedTypes.h"
#include"llvm/ADT/APInt.h"

#include<llvm/Attributes.h>

//#include "LDDDightShadowPasses.h"

#include <cxxabi.h>
extern "C" char *__cxa_demangle(const char *mangled_name, char *output_buffer,
    size_t *length, int *status);

#include <vector>
#include<map>
#include<string>
#include<iostream>
#include<sstream>
#include<fstream>

using namespace llvm;
using namespace std;

STATISTIC(LoadsInstrumented, "Loads instrumented");
STATISTIC(StoresInstrumented, "Stores instrumented");
STATISTIC(AtomicsInstrumented, "Atomic memory intrinsics instrumented");
STATISTIC(IntrinsicsInstrumented, "Block memory intrinsics instrumented");

namespace {


  class LDDLightShadow: public ModulePass,
  public InstVisitor<LDDLightShadow>{
    private:
      // Current context for multi threading support.
      LLVMContext *Context;
      Module *CurModule;
      std::string ModuleName, ModuleNameTail, NewModuleName;

      // Function prototype in the runtime
      const DataLayout *TD;
      IRBuilder<> *Builder;

      PointerType *VoidPtrTy;
      IntegerType *SizeTy;
      IntegerType *SizeInt32Ty;
      PointerType *StringPtrTy;
      PointerType *VoidPointerTy;

      Function *CheckStoreFunction;
      Function *CheckLoadFunction;
      Function *CheckStoreFunctionDebug;
      Function *CheckLoadFunctionDebug;
      Function *CheckStoreStackVarFunction;
      Function *CheckLoadStackVarFunction;
      Function *InitLDDLightShadowFunction;
      Function *FiniLDDLightShadowFunction;
      Function *AddPresentToPrecedeTraceFunction;
      Function *OutputLDDDependenceFunction;

      #ifdef _DDA_PP
      Function *FTP_BEGIN_SCOPE_CREATE;
      Function *__TO_PROF;
      Function *FTP_END_SCOPE_JOIN;
      Function *__INIT_PROC;
      Function *REINITPIPELINESTATE;
      #endif

      void instrument(Value *Pointer, Value *Accesssize, Value* AddrPos, 
          Value*VarName,  Function *Check, Instruction &I);
      void instrumentstackvar(Value *Pointer, Value *Accesssize, Value* AddrPos,
          Value*VarName, Value* DDABeginCurFunc, Function *Check, Instruction &I);
      //void instrumentInit( Function *Check, BasicBlock &I);
      CallInst* instrumentInit( Function *Check, Instruction &I);

      void instrumentaddnewbuffer(Function*addnewbuffer, Value *FName, 
          Value* LPos, Instruction &I);

      // Debug info
      unsigned dbgKind;

      // LoopInfo
      LoopInfo *LI;
      unsigned NumLoops; // ID of Profiled loops.
      std::set<std::string> LDDToProfLoops;
      bool isInOrigFunc;

      // Opt for visitLoad/Store..., not create GVarName every time...
      map<string, GlobalVariable*> Pos;
      map<string, GlobalVariable*> Name;

      // which function is currently instrumented
      unsigned currentFunctionNumber;


      // Control whether to profile stack variables.
      // init = 0. if NoTargetLoops = 1, not profiling all stack variables.
      int NoTargetLoops; 
      std::map<int, int> LoopScope;  //[FirstLine, LastLine)
      std::vector<std::string> LocalSharedVars;



    public:
      static char ID;
      LDDLightShadow(): ModulePass(ID){ 

        // Read the current loop ID.
        std::fstream fs;
        fs.open("ldd-prof-loopID", std::fstream::in); 
        // User not specified the loopid, then setup one started with 0.
        if( !fs || (fs.peek() == std::ifstream::traits_type::eof())  ){
          std::cout<< "**Warning, Not specify starting loopID in ldd-prof-loopID. *********************\n";
          fs.close();
          fs.open("ldd-prof-loopID", std::fstream::out); 
          fs << 0;
          fs.close();
          fs.open("ldd-prof-loopID", std::fstream::in); 
        }
        fs >> NumLoops; 
        fs.close();

        // Read the loops to be profiled.
        fs.open("ldd-prof-loops", std::fstream::in);
        std::string LoopPos;
        if( !fs ){
          std::cout<< "**Warning, Not specify loops in ldd-prof-loops to be profiled***\n";
          //return;
        }
        else{
          while( !fs.eof() ){
            fs >> LoopPos;
            LDDToProfLoops.insert(LoopPos); 
            std::cout<< " Profile Loop: " << LoopPos << "\n";
          }

        }
        fs.close();



      }

      ~LDDLightShadow(){
        std::fstream fs;
        fs.open("ldd-prof-loopID", std::fstream::out); 
        fs << NumLoops; 
        fs.close();

        // Output remain targets loops.
        fs.open("ldd-prof-loops", std::fstream::out); 
        std::set<std::string>::iterator STLB, STLE;
        STLB = LDDToProfLoops.begin();
        STLE = LDDToProfLoops.end();
        for( ; STLB != STLE; STLB++  ){
          fs << *STLB;
          LDDToProfLoops.erase(STLB);
        }
        fs.close();
      }


      //LDDLightShadow(): FunctionPass(ID){}

      virtual bool doInitialization(Module &M);
      bool runOnModule(Module &M);

      // Instrument each function for LDD
      virtual bool runOnCloneFunction(Function &F);
      virtual bool runOnCloneLoop(Function &F, Loop *CurLoop);

      virtual bool insertProfControlCodeOrigLoop(Function &F, Loop *CurLoop);
      virtual bool runOnOrigLoop(Function &F, Loop *CurLoop);
      virtual bool runOnOrigFunction(Function &F);

      // To instrument 'main' for initilization
      virtual bool runOnFunction(Function &F, int flag);

      Function * cloneFunction(Function *F );

      // LoopInfo Pass
      virtual void getAnalysisUsage(AnalysisUsage &AU) const{
        AU.addRequired<DataLayout>();
        AU.addRequired<LoopInfo>();
        AU.setPreservesCFG();
      }

      virtual const char *getPassName() const {
        return "LDDLightShadow ";
      }

      // Visitor methods
      void visitLoadInst(LoadInst &LI);
      void visitStoreInst(StoreInst &SI);
      void visitReturnInst(ReturnInst &SI);

      void visitAtomicCmpXchgInst(AtomicCmpXchgInst &I);
      void visitAtomicRMWInst(AtomicRMWInst &I);
      void visitMemIntrinsic(MemIntrinsic &MI);
      void visitCallInst(CallInst &CI);

      // Get LineNumber, Filename, Funcname of instruction
      std::string getInstFileName(Instruction*I);
      unsigned int getInstLineNumber(Instruction*I);
      std::string getInstFuncName(Instruction*I);

  };


} // end anon namespace

char LDDLightShadow::ID = 0;

static RegisterPass<LDDLightShadow> X("insert-ldd-lshadow", 
    "Loop Dada Dependence Profiling Pass based Light Shadow", false, false);

#if 0
INITIALIZE_PASS(LDDLightShadow, "insert-ldd-profiling", 
    "Insert LDD Profling", false, false)
FunctionPass *llvm::createLDDLightShadowPass(){
  return new LDDLightShadow();
}
#endif 


// Not called now. 20130922
bool LDDLightShadow::doInitialization(Module &M){
  Type *VoidTy = Type::getVoidTy(M.getContext());
  VoidPtrTy = Type::getInt8PtrTy(M.getContext());
  SizeTy = IntegerType::getInt64Ty(M.getContext());
  StringPtrTy = Type::getInt8PtrTy(M.getContext());

  // Create function prototypes
  //M.getOrInsertFunction("__loadcheck", VoidTy, VoidPtrTy, SizeTy, NULL);
  M.getOrInsertFunction("__loadcheck", VoidTy, VoidPtrTy, SizeTy, VoidPtrTy,
      VoidPtrTy,  NULL);
  M.getOrInsertFunction("__storecheck", VoidTy, VoidPtrTy, SizeTy, 
      VoidPtrTy,VoidPtrTy, NULL);
  M.getOrInsertFunction("__initprofiling", VoidTy, NULL);
  M.getOrInsertFunction("__outputdependence", VoidTy, NULL);

  //M.getOrInsertFunction("__initprofiling", VoidTy, VoidPtrTy, SizeTy, NULL);
  return true;
}

/* 1) Generate a copy of the original F, which is called FName+Clone.
 * 2) Insert CallSite of FClone in the original function.
 */
Function * LDDLightShadow::cloneFunction(Function *F )
{
  unsigned int i = 0;

  // Get the function type.
  FunctionType *FTy = F->getFunctionType();

  // Vector to store all arguments' types.
  std::vector<Type*> ArgType;

  for( Function::arg_iterator I = F->arg_begin(), E = F->arg_end(); I != E; 
      ++I, ++i){
    ArgType.push_back( FTy->getParamType(i) );   
  }

  std::cout<< "param_num = " << i << std::endl;
  if( F->isVarArg() )
    std::cout<< "F isVarArg() \n" << std::endl;
  
  FunctionType *NewFTy;
  if( F->isVarArg() )
    NewFTy = FunctionType::get( FTy->getReturnType(), ArgType, true);
  else 
    NewFTy = FunctionType::get( FTy->getReturnType(), ArgType, false);
  Function *NewF = Function::Create( NewFTy, GlobalValue::InternalLinkage,
      F->getName() + "Clone",
      F->getParent() );
  // Set the param list of NewF.
  // std::vector<unsigned int>::iterator it = LEN.begin();
  for( Function::arg_iterator IO = F->arg_begin(), EO = F->arg_end(), 
      IN = NewF->arg_begin(), EN = NewF->arg_end();
      ( IO != EO ) && ( IN != EN );
      ++IO, ++IN ){

    // Set arg names.
    IN->setName( IO->getName() );

    //
  }



  // Create the arguments mapping between the original and the clonal function
  // to prepare for cloning the whole function.
  ValueToValueMapTy VMap;
  Function::arg_iterator DestI = NewF->arg_begin();
  for( Function::arg_iterator I = F->arg_begin(), E = F->arg_end(); I != E; ++I){
    DestI->setName( I->getName() );
    VMap[I] = DestI++;
  }


  // Do the real cloning.
  SmallVector<ReturnInst *, 8 > Returns;
  CloneFunctionInto( NewF, F, VMap, false, Returns );


  // Insert the function callSite in the original function.
  //BasicBlock *BB = BasicBlock::Create( F->getContext(), "clone", F, F->begin() );
  // Bugs? if the successor block has PHI nodes. 20140225
  StringRef GLoopIDName("LDDProfLoopID");
  GlobalVariable *GLoopID = F->getParent()->getGlobalVariable(GLoopIDName);
  StringRef GLoopIterName("LDDProfLoopIter");
  GlobalVariable *GLoopIter = F->getParent()->getGlobalVariable(GLoopIterName);
  StringRef GProfCurIterFlag("LDDProfCurIter");
  GlobalVariable *GProfIter = F->getParent()->getGlobalVariable(GProfCurIterFlag);

  inst_iterator FirstInst = inst_begin(F);
  BasicBlock *Entry = F->begin();
  BasicBlock * ProfSuccessor = Entry->splitBasicBlock( &(*FirstInst), Twine("ProfSuccessor") );

  BasicBlock *If, *IfTrue, *IfFalse;
  Type *type = Type::getInt32Ty( getGlobalContext() );
  If = BasicBlock::Create( F->getContext(), "ProfIf", F, ProfSuccessor );
  IfTrue = BasicBlock::Create( F->getContext(), "ProfIfTrue", F, ProfSuccessor );
  IfFalse = BasicBlock::Create( F->getContext(), "ProfIfFalse", F, ProfSuccessor );
  //IfFalse = BasicBlock::Create( F->getContext(), "ProfIfFalse", F, F->begin() );
  //F->dump();
  LoadInst *GLIDLoad = new LoadInst(GLoopID, Twine(""), If);

  Value * IfCond = new ICmpInst( *If, ICmpInst::ICMP_NE, cast<Value>(GLIDLoad),
      ConstantInt::getIntegerValue(type, APInt(32,0)), "LDDProfLoopID" );
  BranchInst *ControlFlag = BranchInst::Create(IfTrue, IfFalse, IfCond, If);
  BranchInst *TermIfFalse = BranchInst::Create( ProfSuccessor, IfFalse);
  Constant *ConsInt = ConstantInt::get(type, NumLoops);
  //StoreInst *Stor2GVar = new StoreInst( cast<Value>(ConsInt), cast<Value>(GLoopID), IfFalse);
  TerminatorInst *EntryTI = Entry->getTerminator();
  EntryTI->setSuccessor(0, If);

  // 
  std::vector<Value *> args;  
  for ( Function::arg_iterator I = F->arg_begin(), E = F->arg_end(); 
      I != E; ++I ) {
    args.push_back(I);
  }
  CallInst* call_to_new_func = CallInst::Create(NewF, args, "", IfTrue); 
  if( F->getReturnType() == Type::getVoidTy(F->getContext()) ){
    ReturnInst::Create( F->getContext(), IfTrue);
  }else{
    ReturnInst::Create( F->getContext(), call_to_new_func, IfTrue);
  }


  return NewF;


}

// runOnFunction(main)
// Instrument functions: __initprofiling and output.
bool LDDLightShadow::runOnFunction(Function &F, int flag)
{
  InitLDDLightShadowFunction = F.getParent()->getFunction(
      "__initLDDLightShadow");
  assert(InitLDDLightShadowFunction && 
      "__initLDDLightShadowprofiling function has disappeared!\n");

  FiniLDDLightShadowFunction = F.getParent()->getFunction(
      "__finiLDDLightShadow");
  assert(FiniLDDLightShadowFunction && 
      "__finiLDDLightShadow function has disappeared!\n");

  OutputLDDDependenceFunction = F.getParent()->getFunction(
      "__outputLDDDependenceLShadow");
  assert(OutputLDDDependenceFunction && 
      "__outputLDDDependenceLShadow function has disappeared! \n");

  TD = &getAnalysis<DataLayout>(); // 20130325
  IRBuilder<> FuncBuilder(F.getContext());
  Builder = &FuncBuilder;
  DEBUG(errs() << "insert InitLDDLightShadowFunction \n");
  inst_iterator firstInst = inst_begin(F);
  instrumentInit( InitLDDLightShadowFunction, *firstInst);

  // Insert output function. opt me?
  inst_iterator EndInst = inst_end(F);
  for( ; firstInst != EndInst; firstInst++){
    if (firstInst->getOpcode() == Instruction::Ret)
      instrumentInit( OutputLDDDependenceFunction, *firstInst );
  }
  //visit( F );


  return true;
}

// 1) Insert Profile Control code blocks.
// 2) Insert Callsite for __addPresentToPrecedeTrace().
//
bool LDDLightShadow::insertProfControlCodeOrigLoop( Function &F, Loop *CurLoop)
{

  // Declare and Insert Local Variables here. 
  // Declare local variable  DDABeginCurFunc in the front of Function.
  Type *VarType = Type::getInt32Ty(F.getContext());
#if 0
  inst_iterator FirstInst = inst_begin(F);
  Twine StackVarStr("LDDTLoopFunc");
  AllocaInst * StackVar = new AllocaInst(VarType, Twine("LDDTLoopFunc"),
      &(*FirstInst) ) ; 
  StackVar->setAlignment(4);
#endif

  // Insert DDA Profiling Controller Codes.
#ifdef _DDA_OUTERMOST_PROFILING

#if 0
  // Init Declared Variables: DDABeginCurFunc = 0.
  Constant *ConsInt = ConstantInt::get(VarType, 0);
  StoreInst *SI1 = new StoreInst( cast<Value>(ConsInt), cast<Value>(StackVar),
      &(*FirstInst));
#endif
  inst_iterator FirstInst = inst_begin(F); 
  Value *StackVar = &(*FirstInst);


  //LoadInst *newLoadInst = new LoadInst((Value*)newStackVar, "temp1",
  //&(*firstInst));
  //StoreInst *storeInst = new StoreInst( (Value*)consint, (Value*)stackVar,
  //&(*firstInst));
  StringRef GVarStr("LDDProfLoopID");
  GlobalVariable *GVar = F.getParent()->getGlobalVariable(GVarStr);
  StringRef GVarStr1("LDDProfLoopIter");
  GlobalVariable *GVarIterID = F.getParent()->getGlobalVariable(GVarStr1);
  //StoreInst *SI2 = new StoreInst( cast<Value>(ConsInt), cast<Value>(GVar),
  //&(*FirstInst));

  // Diff from Module.getGlobalContext(). 
  llvm::LLVMContext &Context = llvm::getGlobalContext();

  //
  //LI = &getAnalysis<LoopInfo>(F);
  //Loop *CurLoop = NULL;
  BasicBlock *LoopHeader, *LoopPredecessor, *ForInc;
  BasicBlock *If, *IfTrue, *IfFalse, *NewEntry; 
  //  for(LoopInfo::reverse_iterator LIB = LI->rbegin(), LIE = LI->rend(); LIB !=
  //     LIE; ++LIB )
  {
    //  CurLoop = *LIB; 
    NumLoops++;
    cout<< "profiling loop: "<< NumLoops << endl;

    // Get loop header and predecessor.  
    if( (LoopHeader = CurLoop->getHeader()) == NULL){
      DEBUG(errs() << "No loop header in Function \n"<< F.getName().str(); ); 
      return 0;
    }

    // No LoopPredecessor not means error.  
    // And is there a case withoug Predecessor ?
    if( (LoopPredecessor = CurLoop->getLoopPredecessor()) == NULL){
      DEBUG(errs() << "No loop predecessorr in Function \n"<<
          F.getName().str(); ); 
      return 0;
    }

    // Find the for.inc block, which switch to for.cond.
    std::vector<BasicBlock*> LoopBodyBBs = CurLoop->getBlocks();
    std::vector<BasicBlock*>::iterator beg, end;
    beg = LoopBodyBBs.begin();
    end = LoopBodyBBs.end();
    beg++; // skip the LoopHeader BB.
    ForInc = 0;
    for( ;beg != end; beg++ ){
      for( succ_iterator SB = succ_begin(*beg), SE = succ_end(*beg);
          SB != SE; SB++){
        if( *SB == LoopHeader ){
          TerminatorInst *TInst = SB->getTerminator();
          if( TInst->getNumSuccessors() == 2 && !CurLoop->contains(TInst->getSuccessor(1)) ){
            ForInc = *beg;
            break;
          }
        }
      }
      if( ForInc ) break;
    }
    assert( ( beg != end) && "Not find for.inc block in the Target Loop. \n");


    // Extract Induction Variable. Not used now.
    PHINode *PHN = CurLoop->getCanonicalInductionVariable();
    if( PHN != NULL ){
      PHN->dump();
      PHN->getIncomingBlock(0)->dump();
      PHN->getIncomingValue(0)->dump();
      DEBUG( errs() << PHN->getName() << "\n" );
    }

    // Create and inserte profiling controller block. 
    NewEntry = BasicBlock::Create(F.getContext(), "entry", &F, LoopHeader);
    LoopPredecessor->replaceSuccessorsPhiUsesWith(NewEntry);
    Type* type = Type::getInt32Ty(getGlobalContext());
    If = BasicBlock::Create(F.getContext(), "ProfIf", &F, LoopHeader);
    IfTrue = BasicBlock::Create(F.getContext(), "ProfIfTrue", &F,
        LoopHeader);
    IfFalse = BasicBlock::Create(F.getContext(), "ProfIfFalse", &F,
        LoopHeader);
    LoadInst *GVarLoad = new LoadInst(GVar, Twine(""), If);

#ifdef _DDA_PP
    // Insert ftp_begin_scope_create(NULL) before entering for loop: entry1.
    CallInst *CallI;
    Instruction *I = LoopHeader->begin();
    Value * Null =  (Value*) ConstantPointerNull::get(VoidPtrTy );
    //Value * Null =  (Value*) ConstantPointerNull::get( 0 ); 
    Value *PtrNull = Builder->CreatePointerCast( Null, VoidPtrTy);
    Builder->SetInsertPoint( IfTrue );
    Builder->SetInsertPoint( IfTrue );
    CallI = Builder->CreateCall( FTP_BEGIN_SCOPE_CREATE,  PtrNull );
    CallI = Builder->CreateCall( __INIT_PROC );
  if (MDNode *MD = I->getMetadata("dbg"))
    CallI->setMetadata("dbg", MD);
#endif

    // Create and Insert LDDProfLoopID = N.
    Value* IfCond = new ICmpInst(*If, ICmpInst::ICMP_EQ,
        cast<Value>(GVarLoad),  ConstantInt::getIntegerValue(type, APInt(32, 0)),
        "ProfFlag" );
    BranchInst *ControlFlag = BranchInst::Create(IfTrue, IfFalse, IfCond,
        If); 
    BranchInst *TermIfNewEntry = BranchInst::Create(LoopHeader, NewEntry);
    BranchInst *TermIfFalse = BranchInst::Create(NewEntry, IfFalse);
    //BranchInst *TermIfFalse = BranchInst::Create(LoopHeader, IfFalse);
    Constant *ConsInt = ConstantInt::get(type, NumLoops);
    StoreInst *Stor2GVar = new StoreInst( cast<Value>(ConsInt),
        cast<Value>(GVar), IfTrue);




    //instrumentInit( AddNewProfilingBufferFunction, *(IfTrue->end()) ); 
#ifdef _DDA_DEBUG
    LoopHeader->dump();
    LoopHeader->getParent()->dump();
#endif
    // Create LoopPos, FuncName
    GlobalVariable *LoopPos, *FuncName;  
    std::string FileName;

    BasicBlock::iterator BIFI = LoopHeader->begin(); 
    FileName = getInstFileName(&(*BIFI) );      
    while( BIFI != LoopHeader->end() && FileName == "0"  ){
      BIFI++;
      FileName = getInstFileName(&(*BIFI) );      
    }

    Instruction *FirstInst = &*(BIFI); 

    //Instruction * FirstInst= &( *(LoopHeader->begin()) );
    //FirstInst->dump();
    FileName = getInstFileName(FirstInst);

    // Fix: /usr/lib/gcc/x86_64-linux-gnu/4.6/../../../../include/c++/4.6/bits/list.tcc71  
    std::string File, CurFile;     
    File = FileName;       
    std::size_t CharPos;   
#if 1  
    CharPos = File.rfind(".");
    if( CharPos != std::string::npos)
      CurFile = File.substr(0, CharPos);                                                                                                         
#endif 

    CharPos = File.rfind("/");
    if( CharPos != std::string::npos){
      File = File.substr( CharPos+1, File.length() );                                                                                              
    }    

    CharPos = File.find("-");
    while ( CharPos != std::string::npos ){
      File.erase(CharPos, 1); 
      CharPos = File.find("-");
    }    

    CharPos = File.find(".");
    while ( CharPos != std::string::npos ){
      File.erase(CharPos, 1); 
      CharPos = File.find(".");
    }    
    //std::cout<<"File = " << File.c_str() << std::endl;                                                                                           
    FileName = File;
    FileName += "uvw";

    if( CurFile != NewModuleName ){
      FileName += NewModuleName;
      FileName += ModuleNameTail;
    }



    // Get the Loop Line number.
    int LoopLine = getInstLineNumber(FirstInst);
    ostringstream Convert;
    Convert << LoopLine;
    std::string LoopPosStr = FileName + Convert.str();
    cout << "LoopPosStr: " << LoopPosStr << endl;          

    //std::string FuncNameStr = F.getName().str();
    int status;
    std::string FuncNameStr =
      FirstInst->getParent()->getParent()->getName().str();

    // demangling the name.
    char *DMFuncNameStr = __cxa_demangle(FuncNameStr.c_str(), 0, 0, &status);
    if( status == 0 ){
      FuncNameStr = DMFuncNameStr;
      free( DMFuncNameStr);
    }
    int left = FuncNameStr.find("(");
    FuncNameStr = FuncNameStr.substr(0, left);
    // Fix bug.
    //Func Name : std::_Deque_base<OtnChannelSectionRecord*, std::allocator<OtnChannelSectionRecord*> >::_M_create_nodes
    CharPos = FuncNameStr.rfind(":");
    if( CharPos != std::string::npos){
      FuncNameStr = FuncNameStr.substr( CharPos+1, FuncNameStr.length() );
      FuncNameStr = LoopPosStr+ FuncNameStr  ;
    }
    cout << "Func Name : " << FuncNameStr << endl;

    Constant *LoopPosCst =
      ConstantDataArray::getString(F.getParent()->getContext(), StringRef(LoopPosStr)
          ); 
    LoopPos = new GlobalVariable(*CurModule, LoopPosCst->getType(), true,
        GlobalValue::ExternalLinkage, LoopPosCst, LoopPosStr);                     


    Constant *FuncNameCst =
      ConstantDataArray::getString(F.getParent()->getContext(), StringRef(FuncNameStr)
          ); 
    FuncName = new GlobalVariable(*CurModule, FuncNameCst->getType(), true,
        GlobalValue::ExternalLinkage, FuncNameCst, FuncNameStr);                     

    ConsInt = ConstantInt::get(type, 1);
    StoreInst *Stor2StackVar = new StoreInst( cast<Value>(ConsInt),
        cast<Value>(StackVar), IfTrue);

    // ProfilingLoopIterID = 1; 
    StoreInst *Stor2IterID = new StoreInst( cast<Value>(ConsInt),
        cast<Value>(GVarIterID), IfTrue);


    std::cout<<"insert AddNewProfilingBufferFunction"<< std::endl;

    //
    //instrumentaddnewbuffer( AddNewProfilingBufferFunction, (Value*)
    //FuncName, (Value*) LoopPos, *( ++(IfTrue->begin()) ) ); 
    //instrumentInit( AddNewProfilingBufferFunction, *( ++(IfTrue->begin()) )
    //); 



    // old: Look for the first BB of loop.body. 
    // Insert LDDProfLoopIter += DDABeginCurFunc in the loop.body.
    // New: inserted in for.inc. 20140630
    TerminatorInst *LHTermInst =LoopHeader->getTerminator();
    BasicBlock *LoopBody = LHTermInst->getSuccessor(0);
    IRBuilder<> TheBuilder(LoopBody->getContext());

    //TheBuilder.SetInsertPoint(&(LoopBody->front()));
    //TheBuilder.SetInsertPoint(&(LoopBody->front()));
    
    // Insert addPresentToPrecedeTrace in the front of for.inc. 
    // bug?
    //TheBuilder.SetInsertPoint(ForInc->begin());
    //TheBuilder.SetInsertPoint(ForInc->begin()); // One is ok.
    TheBuilder.SetInsertPoint(ForInc->getFirstInsertionPt());
    TheBuilder.SetInsertPoint(ForInc->getFirstInsertionPt()); // One is ok.

    //LoadInst *IterIDLoad = new LoadInst(GVarIterID, Twine(""),ForInc->begin() );
    //LoadInst *CurFuncLoad = new LoadInst(StackVar, Twine(""), ForInc->begin() );
    LoadInst *IterIDLoad = new LoadInst(GVarIterID, Twine(""), ForInc->getFirstInsertionPt() );
    LoadInst *CurFuncLoad = new LoadInst(StackVar, Twine(""), ForInc->getFirstInsertionPt() );

    //TheBuilder.SetInsertPoint(ForInc->begin()); // Call addPresentToPrecedeTrace
    //TheBuilder.CreateCall( AddPresentToPrecedeTraceFunction );

    Value *IterIDAdd = TheBuilder.CreateAdd(cast<Value>(IterIDLoad),
        cast<Value>(CurFuncLoad), Twine("")); 
    //Value *IterIDAdd = TheBuilder.CreateAdd(cast<Value>(IterIDLoad),
    //cast<Value>(ConsInt), Twine("")); 
    Value *StorAdd2IterID = TheBuilder.CreateStore(cast<Value>(IterIDAdd),
        cast<Value>(GVarIterID));

    TheBuilder.SetInsertPoint(ForInc->begin()); // Call addPresentToPrecedeTrace
    TheBuilder.CreateCall( AddPresentToPrecedeTraceFunction );


    #if 0
    // Insert CallSite to __addPresentToPrecedeTrace().
    // __PP not need this function call.
    DEBUG(errs() <<"insert function call:  AddPresentToPrecedeTraceFunction\n" );
    instrumentInit( AddPresentToPrecedeTraceFunction, LoopBody->front() ); 
  #endif

#ifdef _DDA_DEBUG
    LoopBody->dump();
#endif

    BranchInst *TermIfTrue = BranchInst::Create(NewEntry, IfTrue);
    //BranchInst *TermIfTrue = BranchInst::Create(LoopHeader, IfTrue);
    //F.dump();

    // Todo: multi successors in predecessor.
    // Let the predecessor point to the profiling controller block.
    TerminatorInst *TermInst = LoopPredecessor->getTerminator();
    int num = TermInst->getNumSuccessors();
    for( int i = 0; i < num; i++ ){ 
      BasicBlock *SuccessorBlock = TermInst->getSuccessor(i); 
      // Multi Successor
      if( SuccessorBlock == LoopHeader )
        TermInst->setSuccessor(i, If);
    }


    // Get LoopExitingBlocks and insert profiling controller block for exit.
    SmallVector<BasicBlock*, 8> LoopExitingBlocks; 
    CurLoop->getExitingBlocks(LoopExitingBlocks);           

    // Create and instert the profiling controller block if we let DDABeginCurFunc
    // = 1 before.
    BasicBlock *ExitingBlock, *ExitIf, *ExitIfTrue, *ExitIfFalse;
    for(SmallVector<BasicBlock*, 8>::iterator BB = LoopExitingBlocks.begin(),
        BE = LoopExitingBlocks.end(); BB != BE; BB++){
      ExitingBlock = *BB;
      //cout << "ExitingBlock dump"<< endl;
      //ExitingBlock->dump();
      TerminatorInst *BBTermInst =ExitingBlock->getTerminator();
      // fixed c++ exception, destructor and longjmp .20140826.                                                                                              
      if( InvokeInst *invokeInst = dyn_cast<InvokeInst>(&*BBTermInst)
        ){                                                                                     
        //BBTermInst->dump(); // debug 
        continue;
      }
      int num =  BBTermInst->getNumSuccessors();
      for( int i = 0; i < num; i++ )
      { 
        // Multi Successor
        BasicBlock *SuccessorBlock = BBTermInst->getSuccessor(i); 
        if(!CurLoop->contains(SuccessorBlock)){
          ExitIf = BasicBlock::Create(F.getContext(), "ProfExitIf", &F,
              ExitingBlock);
          ExitIfTrue = BasicBlock::Create(F.getContext(),
              "ProfExitIfTrue", &F, ExitingBlock);
          ExitIfFalse = BasicBlock::Create(F.getContext(),
              "ProfExitIfFalse", &F, ExitingBlock);
          LoadInst *StackVarLoad = new LoadInst(StackVar, Twine(""), ExitIf);
          Value* ExitIfCond = new ICmpInst(*ExitIf, ICmpInst::ICMP_EQ,
              StackVarLoad,  ConstantInt::getIntegerValue(type, APInt(32, 1)),
              "ProfExitFlag" );
          BranchInst *ControlFlag = BranchInst::Create(ExitIfTrue,
              ExitIfFalse, ExitIfCond, ExitIf);



          Constant *ConsInt = ConstantInt::get(VarType, 0);
          StoreInst *Stor2GVar = new StoreInst( cast<Value>(ConsInt),
              cast<Value>(GVar), ExitIfTrue);

          #ifndef _DDA_PP
          // Insert CallSite to __addPresentToPrecedeTrace().
          DEBUG(errs() << "insert AddPresentToPrecedeTraceFunction \n");
          instrumentInit( AddPresentToPrecedeTraceFunction,  ExitIfTrue->front() ); 
          #endif

          StoreInst *Stor2StackVar = new StoreInst( cast<Value>(ConsInt),
              cast<Value>(StackVar), ExitIfTrue);

#ifdef _DDA_PP
          TheBuilder.SetInsertPoint( ExitIfTrue );
          TheBuilder.SetInsertPoint( ExitIfTrue );
          CallInst * CallIProf = TheBuilder.CreateCall(FTP_END_SCOPE_JOIN); 
          CallInst * CallIReInit = TheBuilder.CreateCall(REINITPIPELINESTATE); 
#endif

          BBTermInst->setSuccessor(i, ExitIf);
          BranchInst *TermIfTrue = BranchInst::Create(SuccessorBlock,
              ExitIfTrue);
          BranchInst *TermIfFalse = BranchInst::Create(SuccessorBlock,
              ExitIfFalse);
        }
      }

    } 
    // Insert CallSite to __to_prof()
    // if( __to_prof() == 0 ) continue;
#ifdef _DDA_PP
  BasicBlock *ToProf, *ToProTrue;
  BasicBlock *ToProfTrue, *ToProfFalse;
  //ToProf = LoopBody->splitBasicBlock(LoopBody->begin(), "for.toprof");
 
  StringRef GCurIter("LDDProfCurIter");
  GlobalVariable *GProfIter = F.getParent()->getGlobalVariable(GCurIter);
 
  // Create if( __to_prof() == 0 ) goto for.inc;
  ToProf = BasicBlock::Create(F.getContext(), "for.toprof", &F, LoopHeader); 
  TheBuilder.SetInsertPoint( ToProf );
  CallInst * CallIProf = TheBuilder.CreateCall(__TO_PROF); 
  TheBuilder.SetInsertPoint( ToProf  );
  Value * CmpProf = TheBuilder.CreateICmpEQ( CallIProf, ConstantInt::getIntegerValue(type, APInt(32, 0)), Twine("CmpProf")); 
  TheBuilder.SetInsertPoint( ToProf  );


  // insert LDDProfCurIter = 1;
  ToProfTrue = BasicBlock::Create(F.getContext(), "ToProfTrue", &F, LoopHeader);
  Constant *ConsInt1 = ConstantInt::get(type, 1);
  StoreInst *Stor2GVar1 = new StoreInst( cast<Value>(ConsInt1), cast<Value>(GProfIter), ToProfTrue);
  //BranchInst *TermToProfTrue = BranchInst::Create(LoopBody, ToProfTrue);

  // insert LDDProfCurIter = 0;
  ToProfFalse = BasicBlock::Create(F.getContext(), "ToProfFalse", &F, LoopHeader );
  Constant *ConsInt0 = ConstantInt::get(type, 0);
  StoreInst *Stor2GVar2 = new StoreInst( cast<Value>(ConsInt0), cast<Value>(GProfIter), ToProfFalse);
  //BranchInst *TermToProfFalse = BranchInst::Create(LoopBody, ToProfFalse);

  TheBuilder.CreateCondBr(CmpProf, ToProfFalse, ToProfTrue);

  for( pred_iterator PI = pred_begin(LoopBody), PE = pred_end(LoopBody); PI != PE; ++PI){
    if( *PI == ToProf ) continue;
    TerminatorInst * TermInst = (*PI)->getTerminator();
    int Num = TermInst->getNumSuccessors();
    for( int i = 0; i < Num; i++){
      if( TermInst->getSuccessor(i) == LoopBody ){
          TermInst->setSuccessor(i, ToProf); 
      }
    }
  }

  BranchInst *TermToProfTrue = BranchInst::Create(LoopBody, ToProfTrue);
  BranchInst *TermToProfFalse = BranchInst::Create(LoopBody, ToProfFalse);


  //TheBuilder.SetInsertPoint( &LoopBody->front());
  //TheBuilder.CreateICmpEQ( CallIProf, ConstantInt::getIntegerValue(type, APInt(32, 0)), Twine("CmpProf")); 

#endif





  }


#endif  


  return true;
}

/* 1) Check if CurLoop or its nested child is Target Loop.
 * 2:) If CurLoop is Target Loop, 
 *    a) Insert prof control block in F.
 *    b) Insert prof control block in loop, done within insertProfControlCodeOrigLoop( F, CurLoop);  
 *    c) Instrument load/store in the orig Function where the TLoop located.
 *    d) Remove CallSite of FClone.
 */
bool LDDLightShadow::runOnOrigLoop(Function &F, Loop * CurLoop)
{
  // Determine: is CurLoop the TargetLoop ?
  BasicBlock *LHeader = CurLoop->getHeader(); 

  // If the FirstInst is PHI node, there is no dbg info.
  // We visit the next Inst instead.
  BasicBlock::iterator FirstInst = LHeader->begin();
  std::string FileName = getInstFileName( &(*FirstInst) );
  while( FileName == "0" ){
    FirstInst++;
    FileName = getInstFileName( &(*FirstInst) );
  }
  if( FileName == "0" )
    DEBUG( errs() << "runOnOrigLoop(): cannot get the File Name \n";);
  int LoopLine = getInstLineNumber( FirstInst );
  ostringstream Convert; 
  Convert << LoopLine;
  std::string LoopPosStr = FileName + "*" + Convert.str();
  std::cout << "**************LoopPosStr "<<LoopPosStr  << std::endl;
  std::set<std::string>::iterator SE = LDDToProfLoops.end(), SF; 
  SF =  LDDToProfLoops.find(LoopPosStr);

  // CurLoop is the target loop.
  if( SF != SE){
    isInOrigFunc = true;  // Not need?
    DEBUG( errs() << "******TargetLoop FuncName =  " << FileName << "*********** \n";);

#ifdef _DDA_TWO_VERSION
    // Remove the inserted Callsite of xxxClone();
    // bug if there are PHI nodes?
    if( F.getName() != "main" ){
      Function::iterator  ProfBB = F.begin();
      Function::iterator  Entry, ToDelIf, ToDelIfTrue, ToDelIfFalse, Successor;
      Entry = ProfBB;
      ToDelIf = ++ProfBB;
      ToDelIfTrue = ++ProfBB;
      ToDelIfFalse = ++ProfBB;
      Successor = ++ProfBB;

      TerminatorInst *EntryTI = Entry->getTerminator();
      EntryTI->setSuccessor(0, Successor);

      ToDelIf->eraseFromParent();
      ToDelIfTrue->eraseFromParent();
      ToDelIfFalse->eraseFromParent();
    }
#endif

    inst_iterator FirstInst = inst_begin(F);
    Type *VarType = Type::getInt32Ty(F.getContext());
    Twine StackVarStr("LDDTLoopFunc");
    AllocaInst * StackVar = new AllocaInst(VarType, Twine("LDDTLoopFunc"),
        &(*FirstInst) ) ; 
    StackVar->setAlignment(4);

    // Init Declared Variables: DDABeginCurFunc = 0.
    Constant *ConsInt = ConstantInt::get(VarType, 0);
    StoreInst *SI1 = new StoreInst( cast<Value>(ConsInt), cast<Value>(StackVar),
        &(*FirstInst));

#ifdef _DDA_TWO_VERSION
    visit ( F );  // Instrument the load/store of Original Target Function.
#endif

    insertProfControlCodeOrigLoop( F, CurLoop);  
    // Introduce more Time Complexity.
    //LDDToProfLoops.erase( SF );  // bug? Not delete Found_Profile_Loops.
  }

  // Recursive look for the Target Loops within CurLoop.
  if( LDDToProfLoops.size() ){
    std::vector<Loop*> SLVec =  CurLoop->getSubLoopsVector();
    std::vector<Loop*>::iterator SLVecB, SLVecE;
    SLVecB = SLVec.begin();
    SLVecE = SLVec.end();
    for( ; SLVecB != SLVecE; SLVecB++){
      runOnOrigLoop( F, *SLVecB);
    }

  }


  return true;

}

/*
 * 1) Find Loop Lexical Scope of each loop (StartLine, EndLine), and stored in
 *    LoopScope. 
 * 2) for each Loop in F, call runOnOrigLoop(F, Loop) to check if F has TLoop.
 */
bool LDDLightShadow::runOnOrigFunction(Function &F)
{
  // Check that the load and store check functions are declared.
  CheckLoadFunction = F.getParent()->getFunction("__checkLoadLShadow");
  assert(CheckLoadFunction && "__checkLoadLShadow function has disappeared!\n");

  CheckStoreFunction = F.getParent()->getFunction("__checkStoreLShadow");
  assert(CheckStoreFunction &&"__checkStoreLShadow function disappeared!\n");

CheckLoadFunctionDebug = F.getParent()->getFunction("__checkLoadLShadowDebug");
  assert(CheckLoadFunctionDebug && "__checkLoadLShadowDebug function has disappeared!\n");

  CheckStoreFunctionDebug = F.getParent()->getFunction("__checkStoreLShadowDebug");
  assert(CheckStoreFunctionDebug &&"__checkStoreLShadowDebug function disappeared!\n");


  InitLDDLightShadowFunction = F.getParent()->getFunction(
      "__initLDDLightShadow");
  assert(InitLDDLightShadowFunction 
      && "__initLDDLightShadow function has disappeared! \n");


  CheckLoadStackVarFunction = F.getParent()->getFunction(
      "__checkLoadStackVarLShadow");
  assert(CheckLoadStackVarFunction && 
      "__checkLoadStackVarLShadow function has disappeared!\n");

  CheckStoreStackVarFunction = F.getParent()->getFunction(
      "__checkStoreStackVarLShadow");
  assert(CheckStoreStackVarFunction &&
      "__checkStoreStackVarLShadow function has disappeared! \n");

  AddPresentToPrecedeTraceFunction = F.getParent()->getFunction(
      "__addPresentToPrecedeTrace");
  assert( AddPresentToPrecedeTraceFunction  &&
      "__addPresentToPrecedeTrace function has disappeared! \n");

#ifdef _DDA_PP
  FTP_BEGIN_SCOPE_CREATE = F.getParent()->getFunction(
      "ftp_begin_scope_create");
  assert( FTP_BEGIN_SCOPE_CREATE  &&
      "ftp_begin_scope_create function has disappeared! \n");
  __TO_PROF = F.getParent()->getFunction(
      "__to_prof");
  assert( __TO_PROF  &&
      "__to_prof function has disappeared! \n");
  FTP_END_SCOPE_JOIN = F.getParent()->getFunction(
      "ftp_end_scope_join");
  assert( FTP_END_SCOPE_JOIN  &&
      "ftp_end_scope_join function has disappeared! \n");
  REINITPIPELINESTATE = F.getParent()->getFunction(
      "ReInitPipelineState");
  assert( REINITPIPELINESTATE  &&
      "ReInitPipelineState function has disappeared! \n");
  __INIT_PROC = F.getParent()->getFunction(
      "__init_proc");
  assert( __INIT_PROC  &&
      "__init_proc function has disappeared! \n");
#endif

  TD = &getAnalysis<DataLayout>(); // 20130325
  IRBuilder<> TheBuilder(F.getContext());
  Builder = &TheBuilder;

  isInOrigFunc = true;     
  DEBUG( errs() << "orig function name " << F.getName().str() << "\n"; );

  // Find the outermost loop lexical scope: FirstLineNo, LastLineNo.
#if  1
  LoopScope.clear();
  LocalSharedVars.clear();

  LI = &getAnalysis<LoopInfo>(F);
  Loop *LocalLoop = NULL;
  BasicBlock *Header, *Tail;
  int FLineNo, MinLine = 0, num = 0;
  // The outer-most loop.
  for(LoopInfo::reverse_iterator LIB = LI->rbegin(), LIE = LI->rend(); 
      LIB != LIE; ++LIB ){

    LocalLoop = *LIB;
    // Will this happen?
    if( (Header = LocalLoop->getHeader() ) == NULL){
      DEBUG( errs() << "No loop header in function \n" << F.getName().str(); ); 
      return 0;
    } 
    BasicBlock::iterator FirstInst = Header->begin();
    FLineNo = getInstLineNumber(&(*FirstInst));

    // The last line of loop: the first line of succeed block of current loop. 
    // Not good enough.
    SmallVector<BasicBlock*, 8> LoopExitBlocks;
    LocalLoop->getExitBlocks(LoopExitBlocks);
    BasicBlock *ExitingBlock;  
    MinLine = 0, num = 0;
    for(SmallVector<BasicBlock*, 8>::iterator BB = LoopExitBlocks.begin(), 
        BE = LoopExitBlocks.end(); BB != BE; BB++){
      ExitingBlock = *BB;
      //cout << "ExitingBlock dump"<< endl;
      //ExitingBlock->dump();
      BasicBlock::iterator LastInst = ExitingBlock->begin();
      num =  getInstLineNumber(&(*LastInst) );
      if( MinLine == 0 ){
        MinLine = num;
      }else if( MinLine > num ){
        MinLine = num;
      }
    }

    //LastLineNo.push_back(MinLine);
    LoopScope[FLineNo] = MinLine;
    std::cout << "FirstLineNo: "<< FLineNo << " MaxLine:" << " " << MinLine 
      << endl;


    runOnOrigLoop( F, *LIB);

  }
#endif

  return true;
}


// Not used now. 20131210.
bool LDDLightShadow::runOnCloneLoop(Function &F, Loop *)
{



  return true;

}


/* The version is to duplicate function/loop for avoiding prof redunt load/store. 
 * 1) For each defined function: we duplicate a new version called FuncName+Clone;
 *    and every load/store is instrumented in this cloned function;
 * 2) Find Loop Lexical Scope of each loop (StartLine, EndLine), and stored in
 *    LoopScope. 
*/
bool LDDLightShadow::runOnCloneFunction(Function &F)
{
  // Check that the load and store check functions are declared.
  CheckLoadFunction = F.getParent()->getFunction("__checkLoadLShadow");
  assert(CheckLoadFunction && "__checkLoadLShadow function has disappeared!\n");

  //errs() << "Data Dependence Profiling: ";
  //errs().write_escaped( F.getName() ) << "\n";
  CheckStoreFunction = F.getParent()->getFunction("__checkStoreLShadow");
  assert(CheckStoreFunction &&"__checkStoreLShadow function disappeared!\n");

  CheckLoadFunctionDebug = F.getParent()->getFunction("__checkLoadLShadowDebug");
  assert(CheckLoadFunctionDebug && "__checkLoadLShadowDebug function has disappeared!\n");                                                                   

  CheckStoreFunctionDebug = F.getParent()->getFunction("__checkStoreLShadowDebug");
  assert(CheckStoreFunctionDebug &&"__checkStoreLShadowDebug function disappeared!\n");

  InitLDDLightShadowFunction = F.getParent()->getFunction(
      "__initLDDLightShadow");
  assert(InitLDDLightShadowFunction 
      && "__initLDDLightShadow function has disappeared! \n");


  CheckLoadStackVarFunction = F.getParent()->getFunction(
      "__checkLoadStackVarLShadow");
  assert(CheckLoadStackVarFunction && 
      "__checkLoadStackVarLShadow function has disappeared!\n");

  CheckStoreStackVarFunction = F.getParent()->getFunction(
      "__checkStoreStackVarLShadow");
  assert(CheckStoreStackVarFunction &&
      "__checkStoreStackVarLShadow function has disappeared! \n");

  AddPresentToPrecedeTraceFunction = F.getParent()->getFunction(
      "__addPresentToPrecedeTrace");
  assert( AddPresentToPrecedeTraceFunction  &&
      "__addPresentToPrecedeTrace function has disappeared! \n");

#ifdef _DDA_PP
  FTP_BEGIN_SCOPE_CREATE = F.getParent()->getFunction(
      "ftp_begin_scope_create");
  assert( FTP_BEGIN_SCOPE_CREATE  &&
      "ftp_begin_scope_create function has disappeared! \n");

  __TO_PROF = F.getParent()->getFunction(
      "__to_prof");
  assert( __TO_PROF  &&
      "__to_prof function has disappeared! \n");

  FTP_END_SCOPE_JOIN = F.getParent()->getFunction(
      "ftp_end_scope_join");
  assert( FTP_END_SCOPE_JOIN  &&
      "ftp_end_scope_join function has disappeared! \n");

  REINITPIPELINESTATE = F.getParent()->getFunction( "ReInitPipelineState");
  assert( REINITPIPELINESTATE  && "ReInitPipelineState function has disappeared! \n");
  __INIT_PROC = F.getParent()->getFunction("__init_proc");
  assert(__INIT_PROC && "__init_proc function has disappeared! \n");
#endif


  TD = &getAnalysis<DataLayout>(); // 20130325
  IRBuilder<> TheBuilder(F.getContext());
  Builder = &TheBuilder;

  isInOrigFunc = false;

  // Find the outermost loop lexical scope: FirstLineNo, LastLineNo.
  // 
#if  1
  LoopScope.clear();
  LocalSharedVars.clear();

  LI = &getAnalysis<LoopInfo>(F);
  Loop *LocalLoop = NULL;
  BasicBlock *Header, *Tail;
  int FLineNo, MinLine = 0, num = 0;
  for(LoopInfo::reverse_iterator LIB = LI->rbegin(), LIE = LI->rend(); 
      LIB != LIE; ++LIB ){
    LocalLoop = *LIB;
    // Will this happen?
    if( (Header = LocalLoop->getHeader() ) == NULL){
      DEBUG( errs() << "No loop header in function \n" << F.getName().str(); ); 
      return 0;
    } 
    BasicBlock::iterator FirstInst = Header->begin();
    FLineNo = getInstLineNumber(&(*FirstInst));

    // The last line of loop: the first line of succeed block of current loop. 
    // Not good enough.
    SmallVector<BasicBlock*, 8> LoopExitBlocks;
    LocalLoop->getExitBlocks(LoopExitBlocks);
    BasicBlock *ExitingBlock;  
    MinLine = 0, num = 0;
    for(SmallVector<BasicBlock*, 8>::iterator BB = LoopExitBlocks.begin(), 
        BE = LoopExitBlocks.end(); BB != BE; BB++){
      ExitingBlock = *BB;
      //cout << "ExitingBlock dump"<< endl;
      //ExitingBlock->dump();
      BasicBlock::iterator LastInst = ExitingBlock->begin();
      num =  getInstLineNumber(&(*LastInst) );
      if( MinLine == 0 ){
        MinLine = num;
      }
      else if( MinLine > num ){
        MinLine = num;
      }
    }

    //LastLineNo.push_back(MinLine);
    LoopScope[FLineNo] = MinLine;
    std::cout << "FirstLineNo: "<< FLineNo << " MaxLine:" << " " << MinLine 
      << endl;
  }
#endif


  // Function maybe called from the profiled scope or original scope.
  // Generate two versions for Instrument and  original.
#if 0
  // Duplicate the function body as the original version.
  Function::BasicBlockListType &OrigBlocks = F.getBasicBlockList();
  Function::BasicBlockListType NewBlocks; // = F.getBasicBlockList();
  //Function::BasicBlockListType NewBlocks();

  Function::BasicBlockListType::iterator OBB, OBE;
  //ValueToValueMapTy VMap;
  for( OBB = OrigBlocks.begin(), OBE = OrigBlocks.end(); OBB != OBE; ++OBB){
    ValueToValueMapTy VMap; // ?
    BasicBlock* New = CloneBasicBlock(OBB, VMap);
    NewBlocks.push_back(New);
  }
#endif


  // Visit all of the instructions in the function.
  // And instrument load/store operation in the current function when needed.
  visit(F);


  return true;
}


#define PRINT_MODULE dbgs() << \
  "\n\n=============== Module Begin ==============\n" << M << \
"\n================= Module End   ==============\n"
bool LDDLightShadow::runOnModule(Module &M){
  Context = &M.getContext();

  DEBUG(dbgs() 
      << "*************************************************\n"
      << "*************************************************\n"
      << "**                                             **\n"
      << "** LDDLightShadow PROFILING INSTRUMENTATION  **\n"
      << "**                                             **\n"
      << "*************************************************\n"
      << "*************************************************\n");


  ModuleName = M.getModuleIdentifier();
  std::cout<<"Module Name = " << ModuleName << "\n";

  std::size_t CharPos;
  
  CharPos = ModuleName.rfind(".");
  if( CharPos != std::string::npos){
    NewModuleName = ModuleName.substr( 0, CharPos );
  }
 

  CharPos = ModuleName.rfind("/");
  if( CharPos != std::string::npos){
    ModuleNameTail = ModuleName.substr( CharPos+1, ModuleName.length() );
  }
  CharPos = ModuleNameTail.rfind(".");
  while( CharPos != std::string::npos ){
    ModuleNameTail.erase(CharPos, 1);
    CharPos = ModuleNameTail.rfind(".");
  }    
  CharPos = ModuleNameTail.rfind("-");
  while( CharPos != std::string::npos ){
    ModuleNameTail.erase(CharPos, 1);
    CharPos = ModuleNameTail.rfind("-");
  }    

  // Get debug info
  CurModule = &M;
  dbgKind = M.getContext().getMDKindID("dbg");

  // Find the Module with main function.

  // Initialization in every module.
  Type *type = Type::getInt32Ty(getGlobalContext());
  Constant *consint = ConstantInt::get(type, 0);
  GlobalVariable *GLoopID = new GlobalVariable(M, type, false, 
      GlobalVariable::AvailableExternallyLinkage , 
      ConstantInt::getIntegerValue(type, APInt(32, 0)), 
      Twine("LDDProfLoopID") );
  GlobalVariable *GLoopIter = new GlobalVariable(M, type, false, 
      GlobalVariable::AvailableExternallyLinkage , 
      ConstantInt::getIntegerValue(type, APInt(32, 0)), 
      Twine("LDDProfLoopIter") );
 GlobalVariable *GProfLoopIter = new GlobalVariable(M, type, false, 
      GlobalVariable::AvailableExternallyLinkage , 
      ConstantInt::getIntegerValue(type, APInt(32, 0)), 
      Twine("LDDProfCurIter") );


  cout<< "global type: "<< GLoopID->getType()->getTypeID() << endl;;

  //GlobalVariable *intval = new GlobalVariable(M, type, false, 
  //                          GlobalValue::ExternalLinkage, consint, s2);


  Type * VoidTy = Type::getVoidTy(M.getContext());
  VoidPtrTy = Type::getInt8PtrTy(M.getContext());
  SizeTy = IntegerType::getInt64Ty(M.getContext());
  SizeInt32Ty = IntegerType::getInt32Ty(M.getContext());
  //VoidPointerTy = PointerType::get( Type::getVoidTy(M.getContext()), 0);

  // Create function prototypes declaration. 
  //AttributeSet InlineAttr(M.getContext(), AttributeSet::FunctionIndex, AlwaysInline);
 #ifdef _DDA_INLINE
  AttrListPtr InlineAttr =
  AttrListPtr().addAttr( M.getContext(), AttrListPtr::FunctionIndex, Attributes::get(M.getContext(), Attributes::InlineHint));
  //AttrListPtr InlineAttr =
  //AttrListPtr().addAttr( M.getContext(), AttrListPtr::FunctionIndex, Attributes::get(M.getContext(), Attributes::InlineHint)).addAttr( M.getContext(), AttrListPtr::FunctionIndex, Attributes::get(M.getContext(), Attributes::NoUnwind) ).addAttr( M.getContext(), AttrListPtr::FunctionIndex, Attributes::get(M.getContext(), Attributes::UWTable) );
  //InlineAttr.addAttr( M.getContext(), AttrListPtr::FunctionIndex, Attributes::get(M.getContext(), Attributes::NoUnwind) );
  M.getOrInsertFunction("__checkLoadLShadow", InlineAttr, VoidTy, VoidPtrTy, SizeTy, NULL);
#else
  M.getOrInsertFunction("__checkLoadLShadow", VoidTy, VoidPtrTy, SizeTy, NULL);
#endif
  M.getOrInsertFunction("__checkStoreLShadow", VoidTy, VoidPtrTy, SizeTy, NULL);
  M.getOrInsertFunction("__checkLoadLShadowDebug", VoidTy, VoidPtrTy, SizeTy, VoidPtrTy, VoidPtrTy, NULL);
  M.getOrInsertFunction("__checkStoreLShadowDebug", VoidTy, VoidPtrTy, SizeTy, VoidPtrTy, VoidPtrTy, NULL);
  //M.getOrInsertFunction("__storecheck", VoidTy, VoidPtrTy, SizeTy, VoidPtrTy,
  //                     VoidPtrTy, NULL);
  M.getOrInsertFunction("__checkLoadStackVarLShadow", VoidTy, VoidPtrTy, 
      SizeTy,NULL);
  M.getOrInsertFunction("__checkStoreStackVarLShadow", VoidTy, VoidPtrTy, 
      SizeTy,NULL);
  //M.getOrInsertFunction("__initprofiling", VoidTy, VoidPtrTy, SizeTy, NULL);
  M.getOrInsertFunction("__initLDDLightShadow", VoidTy,  NULL);
  M.getOrInsertFunction("__finiLDDLightShadow", VoidTy,  NULL);
  M.getOrInsertFunction("__addPresentToPrecedeTrace", VoidTy,  NULL);
  M.getOrInsertFunction("__outputLDDDependenceLShadow", VoidTy,  NULL);
  M.getOrInsertFunction("ReInitPipelineState", VoidTy,  NULL);

#ifdef _DDA_PP
  M.getOrInsertFunction("ftp_begin_scope_create", VoidTy, VoidPtrTy, NULL);
  M.getOrInsertFunction("__to_prof", SizeInt32Ty,  NULL);
  M.getOrInsertFunction("ftp_end_scope_join", VoidTy,  NULL);
  M.getOrInsertFunction("__init_proc", VoidTy,  NULL);
#endif


  // Instrument 'main' for init at the front of main.
  Function *Main = M.getFunction("main");
  if( Main){
    //M.getOrInsertFunction("__outputdependence", VoidTy,  NULL);
    runOnFunction(*Main, 0);
  }

#if _DDA_TWO_VERSION
  // Clone each function and name them as OrigFuncNameClone();
  for(Module::iterator F = M.begin(), E = M.end(); F != E; F++){
    if( F->isDeclaration() ) 
      continue;
    std::string FName = F->getName();
    if ( FName == "main" )
      continue;
    else if( FName.find("Clone") == std::string::npos ){
      std::cout<< "Clone Function: " << FName << std::endl;
      cloneFunction( F );
    }
    else{
      std::cout<< "Not Clone Function: " << FName << std::endl;
    }
  }
#endif


  //  Instrument any function include "main" function.
  std::vector<Constant*> ftInit;
  unsigned functionNumber = 0;
  for(Module::iterator F = M.begin(), E = M.end(); F != E; F++){
    if( F->isDeclaration())
      continue;

    DEBUG(dbgs() << "Function: " << F->getName() << "\n");
    functionNumber++;

    std::string FName = F->getName();

#if _DDA_TWO_VERSION
    if( FName.find("Clone") != std::string::npos ){
      //std::cout<< "Clone Function: " << FName << std::endl;
      runOnCloneFunction(*F);
    } else {
      runOnOrigFunction(*F);
    }
#else
    runOnCloneFunction(*F); // Instrument load/store  operations.
    runOnOrigFunction(*F); //
#endif
    currentFunctionNumber = functionNumber;
  }

  return true;
} // end runOnModule


  CallInst * 
LDDLightShadow::instrumentInit(  Function *Check, Instruction &I)
{
#if 0
  BasicBlock *LoopBody = I.getParent();
  BasicBlock::iterator FirstInst = LoopBody->begin();
  PHINode *PHN = dyn_cast<PHINode>(FirstInst);  // 
  if( PHN == NULL )
    Builder->SetInsertPoint(&I);
  else{
    FirstInst++;
    Builder->SetInsertPoint(++FirstInst);
  }
 #endif
  Builder->SetInsertPoint(&I);
  CallInst *CallI = Builder->CreateCall(Check);

  // Copy debug information if it is present.
  if (MDNode *MD = I.getMetadata("dbg"))
    CallI->setMetadata("dbg", MD);

  return CallI;
}


void 
LDDLightShadow::instrument(Value *Pointer, Value *AccessSize, Value* AddrPos, 
    Value* VarName, Function *Check, Instruction &I){
  CallInst *CallI;
  Builder->SetInsertPoint(&I); 
  Value *VoidPointer = Builder->CreatePointerCast(Pointer, VoidPtrTy);
#ifndef _DDA_PA
  Value *PtrVarName = Builder->CreatePointerCast(VarName, VoidPtrTy);
  Value *PtrAddrPos = Builder->CreatePointerCast(AddrPos, VoidPtrTy);
  //CallI = Builder->CreateCall2(Check, VoidPointer, AccessSize);
#endif

#ifdef _DDA_DEBUG
//#if  1
  Builder->SetInsertPoint(&I); 
  CallI = Builder->CreateCall4( CheckLoadFunctionDebug, VoidPointer, AccessSize, 
                    PtrAddrPos, PtrVarName);
#endif

  CallI = Builder->CreateCall2(Check, VoidPointer, AccessSize);
#ifdef _DDA_INLINE
  AttrListPtr InlineCallAttr =
    AttrListPtr().addAttr( *Context, AttrListPtr::FunctionIndex, Attributes::get(*Context, Attributes::InlineHint));
  //AttrListPtr().addAttr( *Context, AttrListPtr::FunctionIndex, Attributes::get(*Context, Attributes::InlineHint)).addAttr( *Context, AttrListPtr::FunctionIndex, Attributes::get(*Context, Attributes::NoUnwind) ).addAttr( *Context, AttrListPtr::FunctionIndex, Attributes::get(*Context, Attributes::UWTable) );
  CallI->setAttributes(InlineCallAttr);
#endif
  // Copy debug information if it is present.
  if (MDNode *MD = I.getMetadata("dbg"))
    CallI->setMetadata("dbg", MD);
}
  void
LDDLightShadow::instrumentaddnewbuffer(Function *AddNewProfilingBuffer, 
    Value *FName, Value* LPos, Instruction &I )
{
  Builder->SetInsertPoint(&I); 
  Value *FuncName = Builder->CreatePointerCast(FName, VoidPtrTy);
  Value *LoopPos = Builder->CreatePointerCast(LPos, VoidPtrTy);
  //CallInst *CallI = Builder->CreateCall2(Check, VoidPointer, AccessSize);
  CallInst *CallI = Builder->CreateCall2(AddNewProfilingBuffer, FuncName, 
      LoopPos);

  // Copy debug information if it is present.
  if (MDNode *MD = I.getMetadata("dbg"))
    CallI->setMetadata("dbg", MD);


}



void 
LDDLightShadow::instrumentstackvar(Value *Pointer, Value *AccessSize, 
    Value* AddrPos, Value* VarName, 
    Value* CurFunc, Function *Check, 
    Instruction &I){
 CallInst *CallI;
  Builder->SetInsertPoint(&I); 
  Value *VoidPointer = Builder->CreatePointerCast(Pointer, VoidPtrTy);
#ifndef _DDA_PA
  Value *PtrVarName = Builder->CreatePointerCast(VarName, VoidPtrTy);
  Value *PtrAddrPos = Builder->CreatePointerCast(AddrPos, VoidPtrTy);
#endif

  //CallI = Builder->CreateCall2(Check, VoidPointer, AccessSize);
  //CallInst *CallI = Builder->CreateCall5(Check, VoidPointer, AccessSize, 
  //                                      PtrAddrPos, PtrVarName, CurFunc);
#ifdef _DDA_DEBUG 
//#if 1
  Builder->SetInsertPoint(&I); 
  CallI = Builder->CreateCall4(CheckLoadFunctionDebug, VoidPointer, AccessSize,
                                       PtrAddrPos, PtrVarName);
#endif
  CallI = Builder->CreateCall2(Check, VoidPointer, AccessSize);

  // Copy debug information if it is present.
  if (MDNode *MD = I.getMetadata("dbg"))
    CallI->setMetadata("dbg", MD);
}


//
//
void LDDLightShadow::visitLoadInst(LoadInst &LI)
{
  // flag: whether  __loadcheckstackvar a stack variable.
  int DoProfile = 0;
  Value *AccessSize = ConstantInt::get(SizeTy,
      TD->getTypeStoreSize(LI.getType()));

  // Extract written/read variables information.
  StringRef Var = LI.getPointerOperand()->getName();


  // Filter un-need profiling vars(FUnPV).
  // FUnPV: 1) return val. 
  if( Var.str() == "retval" ){
    cout << "return value: " << Var.str()<< endl;
    return ;
  }

  std::vector<string>::iterator LSVB, LSVE; 
  Value *LoadValue = LI.getOperand(0);

  // Local Scalar variable, pointers.
  if(isa<AllocaInst>(LoadValue ) ){
    if(!isa<PointerType>(cast<AllocaInst>(LoadValue)->getAllocatedType() )){
      // Is it LocalSharedVars?
      for( LSVB = LocalSharedVars.begin(), LSVE = LocalSharedVars.end(); 
          LSVB != LSVE; ++LSVB){
        if( *LSVB == Var.str()){
          // if is local shared scalar/array, call loadcheckstackvar 
          DoProfile = 1; 
          //cout <<"Load Profilie: " <<  *LSVB << endl;
          break;
        }
      } 

      // Declared within loop scope.
      // FUnPV: 2) Local private vars.
      if( !DoProfile ){
        //cout <<Var.str() << "--------------------- pointer"<< endl;
        return ;
      }
    }

    // int *ptr;  local && Pointer
    else{
      return;  // bug?
    }

  }

  // Both local and global array, global pointers. 
  else{
    if(isa<PointerType>(LI.getOperand(0)->getType())){

      // fixme ? maybe not work some day.
      int FindOrigName = 0;
      BasicBlock::iterator bbit(&LI);
      BasicBlock *bb = LI.getParent();

      // load getlementptr
      if( bbit == bb->begin() ){
        if( ConstantExpr *CE = dyn_cast<ConstantExpr>(bbit->getOperand(0)))
          if( CE->getOpcode() == Instruction::GetElementPtr){
            Value *operand = CE->getOperand(0);            
            //cout << operand->getName().str() << endl; // get the right name 
            Var = operand->getName(); 
            if(isa<AllocaInst>(operand))
              DoProfile = 1; 
          }
      }
      // multi-dims array or pointers.
      else{
        // keep the lates related getelementptr
        BasicBlock::iterator bbitprev = bbit;         
        StringRef VarCurName = Var;
        StringRef VarPrevName; 
        --bbit;
        // Advance to the final getelementptr.
        while( bbit != bb->begin() && bbit->getName() != VarCurName ){
          --bbit;
        }

        //
        VarPrevName = bbit->getName(); 
        //if( bbit->getOpcode() == Instruction::GetElementPtr 
        //     && VarCurName == VarPrevName )
        if( VarCurName == VarPrevName ){
          // cout << " find name \n";
          FindOrigName = 1;
          // Same name rule.
          // To be opt. 
          while(bbit != bb->begin()){ 

            if( VarPrevName == VarCurName ){
              // load or getelementptr
              VarCurName = bbit->getOperand(0)->getName(); 
              bbitprev = bbit;

            }
            --bbit;
            VarPrevName = bbit->getName(); 
            //bbit->dump();
          }

          // Tackle begin
          if( VarPrevName == VarCurName ){
            bbitprev = bbit;
          }
        }


        //  bbitprev is the last related getelementptr.
        //  getElementType() isPrimitiveType() 
        //  PointerType::getArrayElementType getAllocatedType
        //  bbitprev->dump();
        Value *operand = bbitprev->getOperand(0);

        //operand->dump();

        // Get Variable name.
        Var = operand->getName();
        //  If the name == "", take the name of Inst as the operand name.
        if( Var.str() == "" ){
          Var = bbitprev->getName();  
        }

        //  Var = LI.getOperand(0)->getName();

        //  Is it local array?
        if(isa<AllocaInst>(operand)){
          if ( isa<ArrayType>((cast<AllocaInst>(operand)->getAllocatedType()))){
            PointerType *OprPointerType = cast<AllocaInst>(operand)->getType();
            Type* ArrayEleType = OprPointerType->getArrayElementType();

            //ArrayEleType->dump(); 
            // Get the array element type.
            while( isa<ArrayType>(ArrayEleType) ) 
              ArrayEleType = ArrayEleType->getArrayElementType();
            //ArrayEleType->dump(); 
            //cout << endl; 

            if( ArrayEleType->isIntegerTy() || 
                ArrayEleType->isFloatingPointTy() || 
                ArrayEleType->isDoubleTy()  ) 
              cout<< endl  << "local primitive array: "<< Var.str() << endl;
            DoProfile = 1;
          }
          //cout << "local: "<< Var.str() << endl;
        }
      }
    }
  }


  // Extract written/read variables information.
  //
  GlobalVariable *GAddrPos, *GVarName;
  Value *GVarNameValue;
  std::string File = getInstFileName(&LI);
  // If cannot get FileName, don't profiling the instruction.
  if( File == "0"){
   DEBUG(errs() << "No File Name for LoadInst \n"; );    
    return;
  }
  unsigned int  Line = getInstLineNumber(&LI);
  //StringRef Func = LI.getParent()->getParent()->getName();

  // Generate VarName and AddrPos.
  ostringstream Convert;
  Convert << Line;
  string StrPos = File + Convert.str();

  // Get File Name. xxx/xxx.c, xxxx-xxx.c, xx.c~xx1.c
  // xx/xx/xx/xx/xx.c ??
  std::string FileName, CurFile;
  std::size_t  CharPos;
  // Module Name == File - ".bc";
  CurFile = File;
#if 1
  // remove the suffix of a file.
  CharPos = File.rfind(".");
  if( CharPos != std::string::npos){
      CurFile = CurFile.substr(0, CharPos);
  }
#endif
  
  //std::cout<<"CurFile " << CurFile.c_str() << std::endl;

  CharPos = File.rfind("/");
  if( CharPos != std::string::npos )
    File = File.substr( CharPos+1, File.length() );
    //File.erase(CharPos, 1); 

  CharPos = File.rfind("/");
  while(CharPos != std::string::npos ){
    File.erase(CharPos, 1); 
    CharPos = File.rfind("/");
  }
  
//  std::cout<<"File-orig  " << File.c_str()<<"\n";

  CharPos = File.find("-");
  while ( CharPos != std::string::npos ){
    File.erase(CharPos, 1); 
    CharPos = File.find("-");
  }

  CharPos = File.find(".");
  if( CharPos != std::string::npos ){
    File.erase(CharPos, 1); 
  }
  CharPos = File.find(".");
  while( CharPos != std::string::npos ){
    File.erase(CharPos, 1); 
    CharPos = File.find(".");
  }

    FileName = File;
    FileName += "xyz";
  std::cout<<"CurFile = " << CurFile << std::endl;
    if( NewModuleName != CurFile ){
      FileName = ModuleNameTail + FileName;
    }

  // Create global Variable name to store AddrPos.
  std::string PosName = "GAddrPos";
  PosName += FileName;
#ifndef _DDA_PA // turn off get the VarName and VarPos.
  if( Pos[StrPos] ){
    GAddrPos = Pos[StrPos]; 
  }
  else{
    Constant *AddrPos = ConstantDataArray::getString(*Context, StringRef(StrPos)); 
    GAddrPos = new GlobalVariable(*CurModule, AddrPos->getType(), true, 
        GlobalValue::ExternalLinkage, AddrPos, PosName); 
    Pos[StrPos] = GAddrPos; 
  }

  // Create Global Variable Name to store VarName. 
  std::string  GlobalVarName = "GVarName";
  GlobalVarName += FileName; 
  //std::cout<< "new name " << GlobalVarName << std::endl;
  if( Name[Var.str()] ){
    GVarName = Name[Var.str()];
  }
  else{
    Constant *VarName = ConstantDataArray::getString(*Context, Var); 
    GVarName = new GlobalVariable(*CurModule, VarName->getType(), true, 
        GlobalValue::ExternalLinkage, VarName, GlobalVarName); 
    //GVarNameValue = Builder->CreateGlobalString( Var , "GVarName" );
    Name[Var.str()] = GVarName;
 ; 
  }
  cout << StrPos << " "<< Var.str() << " " << File << " " << Line << endl;
#endif

  if( !DoProfile )
    instrument(LI.getPointerOperand(), AccessSize,(Value*)GAddrPos, 
        (Value*)GVarName,  CheckLoadFunction, LI);
  // The function Contains the Target Loop, prof stackvar.
  else if( isInOrigFunc ){
    Function *F = LI.getParent()->getParent();
    inst_iterator FirstInst = inst_begin(F); 
    Value* DDABeginCurFuncDec = &(*FirstInst);
    LoadInst *CurFuncLoad = NULL;
    //LoadInst *CurFuncLoad = new LoadInst(DDABeginCurFuncDec, Twine(""), &LI);
    instrumentstackvar(LI.getPointerOperand(), AccessSize,(Value*)GAddrPos, 
        (Value*)GVarName, (Value*)CurFuncLoad, 
        CheckLoadStackVarFunction, LI);
  }
  else ;
  ++LoadsInstrumented;

}


void LDDLightShadow::visitStoreInst(StoreInst &SI)
{
  // Instrument a store instruction with a store check.
  // Is the size precise and available all the time?
  uint64_t Bytes = TD->getTypeStoreSize(SI.getValueOperand()->getType());
  Value *AccessSize = ConstantInt::get(SizeTy, Bytes);

  StringRef Var = SI.getPointerOperand()->getName();

  //cout << "begin store var name :" << Var.str() << endl;

  // Filter un-need profiling vars.
  //
  if( Var.str() == "retval" ){
    cout << "return value: " << Var.str()<< endl;
    return ;
  }

  int DoProfile = 0;
  std::vector<string>::iterator LSVB, LSVE; 
  Value *StoreValue = SI.getOperand(1);

  // 1) Auto Variables.
  if(isa<AllocaInst>(StoreValue ) ){
    //SI.dump();
    if(!isa<PointerType>(cast<AllocaInst>(StoreValue)->getAllocatedType() )){

      // is LocalSharedVars?
      for( LSVB = LocalSharedVars.begin(), LSVE = LocalSharedVars.end(); 
          LSVB != LSVE; ++LSVB){
        if( *LSVB == Var.str()){
          DoProfile = 1; 
          //cout <<"Store Profile: " <<  *LSVB << endl;
          break;
        }
      } 
      if( !DoProfile ){
        //cout <<Var.str() << "--------------- Not Profile in StoreInst"<< endl;
        return ;
      }

    }
    // Local Pointers itself, not pointed objects.
    else{
      return; 
    }
  }

  // Global array and pointers. 
  // Local array and pointers with getelelemntptr.
  else{
    //SI.dump();
    if(isa<PointerType>(SI.getOperand(1)->getType())){

      // fixme ? maybe not work some day.
      BasicBlock::iterator bbit(&SI);
      BasicBlock *bb = SI.getParent();
      //bbit->dump(); 

      // load getlementptr
      if( bbit == bb->begin() ){
        //bbit->dump();
        if( ConstantExpr *CE = dyn_cast<ConstantExpr>(bbit->getOperand(1)))
          if( CE->getOpcode() == Instruction::GetElementPtr){
            Value *operand = CE->getOperand(0);            
            cout << operand->getName().str() << endl; // get the right name 
            Var = operand->getName(); 
            if(isa<AllocaInst>(operand))
              DoProfile = 1; 
          }
      }
      // multi-dims array or pointers.
      else{
        // keep the lates related getelementptr
        BasicBlock::iterator bbitprev = bbit;         
        int FindOrigName = 0;
        StringRef VarCurName = Var;
        StringRef VarPrevName; 
        //cout <<"VarCurName: " <<  VarCurName.str() << endl;

        --bbit;
        //bbit->dump();

        // Advance to the final getelementptr.
        while( bbit != bb->begin() && bbit->getName() != VarCurName ) {
          --bbit;
          //bbit->dump();
        }

        // 
        //
        // only there is getelementptr instuction need to tackkle...
        VarPrevName = bbit->getName(); 
        //if( bbit->getOpcode() == Instruction::GetElementPtr  
        //   && VarCurName == VarPrevName )
        if( VarCurName == VarPrevName ){
          //cout << " find name \n";
          FindOrigName = 1;

          // To be opt. 
          while(bbit != bb->begin()){ 

            // Same name rule.
            if( VarPrevName == VarCurName ){
              // load or getelementptr
              VarCurName = bbit->getOperand(0)->getName();   
              bbitprev = bbit;

            }
            --bbit;
            VarPrevName = bbit->getName(); 
            //bbit->dump();
          }


          // Tackle begin
          if( VarPrevName == VarCurName ){
            bbitprev = bbit;
          }

        }




        // bbitprev is the last related getelementptr.
        // getElementType()   isPrimitiveType()   
        // PointerType::getArrayElementType 
        // getAllocatedType
        // bbitprev->dump();
        Value * operand;
        if( FindOrigName ){
          operand = bbitprev->getOperand(0);  // load addr / getelementptr
        }
        else{
          operand = bbitprev->getOperand(1);  // it is still SI.
        }

        //operand->dump();



        // Get Variable name. fix me?
        Var = operand->getName();
        if( Var.str() == "" ){
          Var = bbitprev->getName();
        }
        //  Var = SI.getPointerOperand()->getName();

        //cout << "store last name: " << Var.str() << endl;

        //  Is it local array?
        if(isa<AllocaInst>(operand)){
          if (isa<ArrayType>( (cast<AllocaInst>(operand)->getAllocatedType()))){
            PointerType *OprPointerType = cast<AllocaInst>(operand)->getType();
            Type* ArrayEleType = OprPointerType->getArrayElementType();

            //ArrayEleType->dump(); 
            // Get the array element type.
            while( isa<ArrayType>(ArrayEleType) ) 
              ArrayEleType = ArrayEleType->getArrayElementType();
            //ArrayEleType->dump(); 

            if( ArrayEleType->isIntegerTy() || 
                ArrayEleType->isFloatingPointTy() || 
                ArrayEleType->isDoubleTy()  ) 
              cout<< endl  << "local primitive array: "<< Var.str() << endl;
            DoProfile = 1;
          }
          //cout << "local: "<< Var.str() << endl;
        }


      }
    }


  }

  GlobalVariable *GAddrPos, *GVarName;
  std::string File = getInstFileName(&SI);

  // If cannot get FileName, don't profiling the instruction.
  if( File == "0")
    return;
  unsigned int  Line = getInstLineNumber(&SI);

#if 1 
  ostringstream Convert;
  Convert << Line;
  string StrPos = File + Convert.str();
#endif
  //string StrPos = File.str();

  // Create global Variable name to store AddrPos.
  std::string FileName, CurFile;
  std::size_t CharPos;

  CharPos = File.rfind(".");
  if( CharPos != std::string::npos){
    CurFile = CurFile.substr(0, CharPos);
  }
  //std::cout<<"CurFile " << CurFile.c_str() << std::endl;
 
  CharPos = File.rfind("/");
  if( CharPos != std::string::npos )
    File = File.substr( CharPos+1, File.length() );

  CharPos = File.find("-");
  while( CharPos != std::string::npos ){
    File = File.erase( CharPos, 1 ); 
    CharPos = File.find("-");
  }

  CharPos = File.find(".");
  while ( CharPos != std::string::npos ) {
    File.erase( CharPos, 1 );  
    CharPos = File.find(".");
  }
  
  FileName = File;
  FileName += "rst";
  if( NewModuleName != CurFile ){
    FileName = ModuleNameTail + FileName;
  }
  
  std::string PosName = "GAddrPos";
  PosName += FileName;

  // Create Global Variable Name to store VarName. 
  std::string  GlobalVarName = "GVarName";
  GlobalVarName += FileName;
  //std::cout<< "new name " << GlobalVarName << std::endl;


#ifndef _DDA_PA
  if( Pos[StrPos] ){
    GAddrPos = Pos[StrPos]; 
  }
  else{
    Constant *AddrPos = ConstantDataArray::getString(*Context, StringRef(StrPos));
    GAddrPos = new GlobalVariable(*CurModule, AddrPos->getType(), true, 
        GlobalValue::ExternalLinkage, AddrPos, PosName); 
    Pos[StrPos] = GAddrPos; 
  }


  if( Name[Var.str()] ){
    GVarName = Name[Var.str()];
  }
  else{
    Constant *VarName = ConstantDataArray::getString(*Context, Var); 
    GVarName = new GlobalVariable(*CurModule, VarName->getType(), true, 
        GlobalValue::ExternalLinkage, VarName, GlobalVarName);
    Name[Var.str()] = GVarName;
  }
#endif
  //instrument(SI.getPointerOperand(), AccessSize,(Value*)GAddrPos, 
  //           (Value*)GVarName, StoreCheckFunction, SI);
  //instrumentInit(InitProfilingFunction, SI);


  if( !DoProfile )
    instrument(SI.getPointerOperand(), AccessSize,(Value*)GAddrPos, 
        (Value*)GVarName,  CheckStoreFunction, SI);
  else if ( isInOrigFunc ) {
    Function *F = SI.getParent()->getParent();
    inst_iterator FirstInst = inst_begin(F); 
    Value* DDABeginCurFuncDec = &(*FirstInst);
    LoadInst *CurFuncLoad = NULL;
    //LoadInst *CurFuncLoad = new LoadInst(DDABeginCurFuncDec, Twine(""), &SI);
    instrumentstackvar(SI.getPointerOperand(), AccessSize,(Value*)GAddrPos, 
        (Value*)GVarName, (Value*)CurFuncLoad, 
        CheckStoreStackVarFunction, SI);

  }
  else;

  ++StoresInstrumented;
}


void LDDLightShadow::visitReturnInst(ReturnInst &I)
{

  return;
  // is main function ?
  Function * F = I.getParent()->getParent();
  string FuncName = F->getName().str();

  if( FuncName == "main"){
#if  1
    //OutputDependenceFunction = F->getParent()->getFunction(" __outputdependence");
    //assert(OutputDependenceFunction && "__outputdependence function has disappeared!\n");

    //TD = &getAnalysis<DataLayout>(); // 20130325
    //IRBuilder<> FuncBuilder(F->getContext());
    //Builder = &FuncBuilder;
    printf("instrument OutputLDDDependenceFunction \n ");
    instrumentInit( OutputLDDDependenceFunction, I);
#endif

  } 

}

void LDDLightShadow::visitAtomicRMWInst(AtomicRMWInst &I){
  // Instument an AtomicRMW instruction with a store check.
  Value *AccessSize = ConstantInt::get(SizeTy, TD->getTypeStoreSize(I.getType()));
  //instrument(I.getPointerOperand(), AccessSize, StoreCheckFunction, I);
  ++AtomicsInstrumented;
}

void LDDLightShadow::visitAtomicCmpXchgInst(AtomicCmpXchgInst &I){
  // Instrument an AtomicCmpXchg instruction with a store check.
  Value *AccessSize = ConstantInt::get(SizeTy, TD->getTypeStoreSize(I.getType()));

  GlobalVariable *GAddrPos, *GVarName;
  StringRef Var = I.getPointerOperand()->getName();
  std::string File = getInstFileName(&I);
  // If cannot get FileName, don't profiling the instruction.
  if( File == "0")
    return;
  unsigned int  Line = getInstLineNumber(&I);
  //StringRef Func = LI.getParent()->getParent()->getName();

#if 1 
  ostringstream Convert;
  Convert << Line;
  string StrPos = File + Convert.str();
#endif


  // Create global Variable name to store AddrPos.
  std::string PosName = "GAddrPos";
  unsigned int DotPos = File.find(".");
  std::string FileName = File.substr(0, DotPos);
  PosName += FileName;

  // Create Global Variable Name to store VarName. 
  std::string  GlobalVarName = "GVarName";
  GlobalVarName += FileName;
  //std::cout<< "new name " << GlobalVarName << std::endl;

  //string StrPos = File.str();
  if( Pos[StrPos] ){
    GAddrPos = Pos[StrPos]; 
  }
  else{
    Constant *AddrPos = ConstantDataArray::getString(*Context, StringRef(StrPos));
    GAddrPos = new GlobalVariable(*CurModule, AddrPos->getType(), true, 
        GlobalValue::ExternalLinkage, AddrPos, PosName); 
    Pos[StrPos] = GAddrPos; 
  }




  if( Name[Var.str()] ){
    GVarName = Name[Var.str()];
  }
  else{
    Constant *VarName = ConstantDataArray::getString(*Context, Var); 
    GVarName = new GlobalVariable(*CurModule, VarName->getType(), true, 
        GlobalValue::ExternalLinkage, VarName, GlobalVarName); 
    Name[Var.str()] = GVarName;
  }

  instrument(I.getPointerOperand(), AccessSize, (Value*) GAddrPos, 
      (Value*)GVarName, CheckStoreFunction, I);
  ++AtomicsInstrumented;
}

void LDDLightShadow::visitMemIntrinsic(MemIntrinsic &MI){
  // Instrument llvm.mem[set|set|cpy|move].* calls with load/store checks.
  Builder->SetInsertPoint(&MI);
  Value *AccessSize = Builder->CreateIntCast(MI.getLength(), SizeTy, false);

#if 0
  GlobalVariable *GAddrPos, *GVarName;
  StringRef Var = MI.getPointerOperand()->getName();
  StringRef File = getInstFileName(&MI);
  unsigned int  Line = getInstLineNumber(&MI);
  //StringRef Func = LI.getParent()->getParent()->getName();

  ostringstream Convert;
  Convert << Line;
  string StrPos = File.str() + Convert.str();

  //string StrPos = File.str();
  if( Pos[StrPos] ){
    GAddrPos = Pos[StrPos]; 
  }
  else{
    Constant *AddrPos = ConstantDataArray::getString(
        *Context, StringRef(StrPos)); 
    GAddrPos = new GlobalVariable(*CurModule, AddrPos->getType(), true, 
        GlobalValue::ExternalLinkage, AddrPos, "GAddrPos"); 
    Pos[StrPos] = GAddrPos; 
  }
  if( Name[Var.str()] ){
    GVarName = Name[Var.str()];
  }
  else{
    Constant *VarName = ConstantDataArray::getString(*Context, Var); 
    GVarName = new GlobalVariable(*CurModule, VarName->getType(), true, 
        GlobalValue::ExternalLinkage, VarName, "GVarName"); 
    Name[Var.str()] = GVarName;
  }
  // memcpy and memmove have a source memory area but memset doesn't
  if(MemTransferInst *MTI = dyn_cast<MemTransferInst>(&MI))
    instrument(MTI->getSource(), AccessSize, (Value*)GAddrPos, 
        (Value*) GVarName, CheckLoadFunction, MI);
  instrument(MI.getDest(), AccessSize, (Value*)GAddrPos, (Value*)GvarName,  
      CheckStoreFunction, MI);
#endif
  ++IntrinsicsInstrumented;
}

// LDDLightShadow::LoopScope
void LDDLightShadow::visitCallInst(CallInst &CI)
{
  //CI.dump();
  std::string FuncName;
  Function * Func = CI.getCalledFunction();
  if( Func != NULL )  // fixed.
    FuncName = Func->getName().str(); 
  else // Indirect funcall call returns NULL.
    FuncName = "";
  int LineNo;
  if( FuncName == "llvm.dbg.declare" ){
    if( MDNode *Dbg = CI.getMetadata(dbgKind) ){
      DILocation Loc(Dbg);
      //Dbg->dump();
      //cout<<"CalledFuncFileName: " <<  Loc.getFilename().str() << endl;
      //return(Loc.getDirectory().str() + "/" + Loc.getFilename().str());
      LineNo = Loc.getLineNumber();
    }    
    //cout << "xxxxxxxx LineNo: " << LineNo << endl;
    std::map<int, int>::iterator LSB, LSE;
    for( LSB = LoopScope.begin(), LSE = LoopScope.end(); LSB != LSE; ++LSB){
      if( LineNo < LSB->first ){
        //cout<< "LineNo: "<< LineNo <<" FLineNo" << LSB->first<< endl;
        Value* Var1 = CI.getArgOperand(0);
        DILocation Loc((MDNode*)Var1); 
        std::string VarName = ((MDNode*)Var1)->getOperand(0)->getName().str();
        LocalSharedVars.push_back(VarName);
        break;
      }
      else if( LineNo < LSB->second ){
        //StringRef Var = LI.getPointerOperand()->getName();
        //Value* Var1 = CI.getArgOperand(0);
        //DILocation Loc((MDNode*)Var1); 
        //std::string VarName = ((MDNode*)Var1)->getOperand(0)->getName().str();
        //LocalSharedVars.push_back(VarName);
        //cout <<"OperandName " << VarName << endl;
        break;
      } 
    }

  }

  //std::cout<<"CalledFunc " <<  Func->getName().str() << endl;
  return;
}


  std::string 
LDDLightShadow::getInstFileName(Instruction*I)
{
  if( MDNode *Dbg = I->getMetadata(dbgKind) ){
    DILocation Loc(Dbg);
    // Dbg->dump();
    return( Loc.getFilename().str() );
    //return(Loc.getDirectory().str() + "/" + Loc.getFilename().str());
  }
  else{
    cout<<"getInstFileName can not get dbg information "<< endl;
    //I->dump();
    return "0";    
  }
}

unsigned int LDDLightShadow::getInstLineNumber(Instruction*I)
{
  if( MDNode *Dbg = I->getMetadata(dbgKind) ){
    DILocation Loc(Dbg);
    return( Loc.getLineNumber() );
  }
  else{
    cout<<"getInstFileName can not get dbg information "<< endl;
    return 0;    
  }
}
  std::string 
LDDLightShadow::getInstFuncName(Instruction*I)
{
#if 0
  if( MDNode *Dbg = I->getMetadata(dbgKind) ){

    if( Dbg->getFunction() )
      return ( Dbg->getFunction()->getName().str() );
    else
      return "no function" ;

  }
  else{
    cout<<"getInstFuncName can not get dbg information "<< endl;
    return 0;    
  }
#endif
  BasicBlock *bb = I->getParent();
  Function* f = bb->getParent();
  return( f->getName().str() );
}

#if 0
  StringRef
LDDLightShadow::getPointerName(Instruction &I)
{

  return "xx";
}
#endif




