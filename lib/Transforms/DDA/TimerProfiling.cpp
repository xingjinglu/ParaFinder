//===- TimerProfiling.cpp - Instrument for timing profiling ------------===//
////
////                      The LLVM Compiler Infrastructure
////
//// This file is distributed under the University of Illinois Open Source
//// License. See LICENSE.TXT for details.
////
////===----------------------------------------------------------------------===//
//
// The current state:
// 1) Profiling the outer-most loop of each function.
//
// Todo:
//  1) Instrument inline function calls.
//  2) Based CFG, just profiling the outermost loop.
//
//===------------------------------------------------------------------------===//
//

#define DEBUG_TYPE "insert-timer-profiling"
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

// Add global
#include "llvm/ADT/Twine.h"
#include "llvm/GlobalVariable.h"
#include"llvm/DerivedTypes.h"
#include"llvm/ADT/APInt.h"

#include "TimerProfilingPasses.h"
#include <vector>
#include<map>
#include<string>
#include<iostream>
#include<sstream>

using namespace llvm;
using namespace std;

STATISTIC(LoadsInstrumented, "Loads instrumented");
STATISTIC(StoresInstrumented, "Stores instrumented");
STATISTIC(AtomicsInstrumented, "Atomic memory intrinsics instrumented");
STATISTIC(IntrinsicsInstrumented, "Block memory intrinsics instrumented");

namespace {

  class TimerProfiling: public ModulePass,
  public InstVisitor<TimerProfiling>{
    private:
      // Current context for multi threading support.
      LLVMContext *Context;
      Module *CurModule;
  
      // Opt for visitLoad/Store..., not create GVarName every time...
      map<string, GlobalVariable*> Pos;
      map<string, GlobalVariable*> Name;

      // which function is currently instrumented
      unsigned currentFunctionNumber;

      // Function prototype in the runtime
      const DataLayout *TD;
      IRBuilder<> *Builder;

      PointerType *VoidPtrTy;
      IntegerType *SizeTy;
      IntegerType *SizeTy32;
      PointerType *StringPtrTy;

      Function *OutputID2NameFunction;
      Function *OutputResultsFunction;
      Function *RecordID2NameFunction;
      Function *RecordResultsFunction;
      Function *RDTSCFunction;
      Function *InitProfilingFunction;

      void instrument(Value *Pointer, Value *Accesssize, Value* AddrPos, Value*VarName,  Function *Check, Instruction &I);
      //void instrumentInit( Function *Check, BasicBlock &I);
      void instrumentInit( Function *Check, Instruction &I);

      // Debug info
      unsigned dbgKind;

      // LoopInfo
      LoopInfo *LI;
      unsigned NumLoops; // ID of Profiled loops.

    // Timer for main function. 
    AllocaInst * FBegin, *FEnd;
    public:
      static char ID;
      TimerProfiling(): ModulePass(ID){ NumLoops = 0; }

      virtual bool doInitialization(Module &M);
      bool runOnModule(Module &M);

      // Instrument each function for Timer 
      virtual bool runOnFunction(Function &F);

      // To instrument 'main' for initilization
      virtual bool runOnFunction(Function &F, int flag);

      // LoopInfo Pass
      virtual void getAnalysisUsage(AnalysisUsage &AU) const{
        AU.addRequired<DataLayout>();
        AU.addRequired<LoopInfo>();
        AU.setPreservesCFG();
      }

      virtual const char *getPassName() const {
        return "LDDProfiling ";
      }

      // Visitor methods
      void visitReturnInst(ReturnInst &LI);

      // Get LineNumber, Filename, Funcname of instruction
      std::string getInstFilename(Instruction*I);
      unsigned int getInstLineNumber(Instruction*I);
      std::string getInstFuncname(Instruction*I);
  };

} // end anon namespace

char TimerProfiling::ID = 0;

static RegisterPass<TimerProfiling> X("insert-timer-profiling", "Hotspot Profiling Pass", false, false);

#if 0
INITIALIZE_PASS(LDDProfiling, "insert-ldd-profiling", "Insert LDD Profling", false, false)
FunctionPass *llvm::createLDDProfilingPass(){
  return new LDDProfiling();
}
#endif 

bool TimerProfiling::doInitialization(Module &M){
  Type *VoidTy = Type::getVoidTy(M.getContext());
  VoidPtrTy = Type::getInt8PtrTy(M.getContext());
  SizeTy = IntegerType::getInt64Ty(M.getContext());
  SizeTy32 = IntegerType::getInt32Ty(M.getContext());
  StringPtrTy = Type::getInt8PtrTy(M.getContext());

  // Create function prototypes
  M.getOrInsertFunction("__initprofiling", VoidTy, NULL);
  M.getOrInsertFunction("__outputLoopID2Name", VoidTy, NULL);
  M.getOrInsertFunction("__outputTimerResults", VoidTy, NULL);

  M.getOrInsertFunction("__recordLoopID2Name", VoidTy, VoidPtrTy, SizeTy, NULL);
  M.getOrInsertFunction("__recordTimerResults", VoidTy, SizeTy, SizeTy, NULL);
  M.getOrInsertFunction("__rdtsc", SizeTy, NULL);

  return true;
}

// main function.
// Instrument functions: __initprofiling and output.
bool TimerProfiling::runOnFunction(Function &F, int flag){

  InitProfilingFunction = F.getParent()->getFunction("__initprofiling");
  assert(InitProfilingFunction && "__initprofiling function has disappeared! \n");
  OutputID2NameFunction = F.getParent()->getFunction("__outputLoopID2Name");
  assert(OutputID2NameFunction && "__outputLoopID2Name function has disappeared! \n");
  OutputResultsFunction = F.getParent()->getFunction("__outputTimerResults");
  assert(OutputResultsFunction && "__outputTimerResults function has disappeared! \n");

  RecordResultsFunction = F.getParent()->getFunction("__recordTimerResults");
  assert(RecordResultsFunction &&"__recordTimerResults function has disappeared! \n");

  RecordID2NameFunction = F.getParent()->getFunction("__recordLoopID2Name");
  assert(RecordID2NameFunction &&"__recordLoopID2Name function has disappeared! \n");
  RDTSCFunction = F.getParent()->getFunction("__rdtsc");
  assert(RDTSCFunction &&"__rdtsc function has disappeared! \n");


  TD = &getAnalysis<DataLayout>(); // 20130325
  IRBuilder<> FuncBuilder(F.getContext());
  Builder = &FuncBuilder;

  // Insert callers in main function.
  inst_iterator FirstInst = inst_begin(F);
  instrumentInit( InitProfilingFunction, *FirstInst);

  Type *VarType = Type::getInt64Ty(F.getContext());
   FBegin = new AllocaInst(VarType, Twine("rdtscFBegin"), &(*FirstInst) ) ; 
  FBegin->setAlignment(8);
  FEnd = new AllocaInst(VarType, Twine("rdtscFEnd"), &(*FirstInst) ) ; 
  FEnd->setAlignment(8);
  
  Builder->SetInsertPoint( &(*FirstInst) );  
  Value *RDTSCBegin = Builder->CreateCall(RDTSCFunction);
  Builder->CreateStore( RDTSCBegin, cast<Value>(FBegin), true); 

  // Insert callsits for outputing.
  visit(F);
  
  // Todo: insert output function.
  return true;
}

// runOnFunction( anyFuncofModule )
bool TimerProfiling::runOnFunction(Function &F){

  // Check that the load and store check functions are declared.
  OutputID2NameFunction = F.getParent()->getFunction("__outputLoopID2Name");
  assert(OutputID2NameFunction && "__outputLoopID2Name function has disappeared!\n");

  OutputResultsFunction = F.getParent()->getFunction("__outputTimerResults");
  assert(OutputResultsFunction &&"__outputTimerResults function has disappeared! \n");

  InitProfilingFunction = F.getParent()->getFunction("__initprofiling");
  assert(InitProfilingFunction &&"__initprofiling function has disappeared! \n");

  RecordResultsFunction = F.getParent()->getFunction("__recordTimerResults");
  assert(RecordResultsFunction &&"__recordTimerResults function has disappeared! \n");

  RecordID2NameFunction = F.getParent()->getFunction("__recordLoopID2Name");
  assert(RecordID2NameFunction &&"__recordLoopID2Name function has disappeared! \n");
  RDTSCFunction = F.getParent()->getFunction("__rdtsc");
  assert(RDTSCFunction &&"__rdtsc function has disappeared! \n");

  TD = &getAnalysis<DataLayout>(); // 20130325
  IRBuilder<> TheBuilder(F.getContext());
  Builder = &TheBuilder;


#ifdef _TIMER_OUTERMOST_PROFILING
  // Create local variables to record timer value rdtscBegin, rdtscEnd.
  inst_iterator FirstInst = inst_begin(F);
  Type *VarType = Type::getInt64Ty(F.getContext());
  AllocaInst * Begin = new AllocaInst(VarType, Twine("rdtscBegin"), &(*FirstInst) ) ; 
  AllocaInst * End = new AllocaInst(VarType, Twine("rdtscEnd"), &(*FirstInst) ) ; 
  Begin->setAlignment(4);
  End->setAlignment(4);

  //
  Constant *ConsInt = ConstantInt::get(VarType, 0);

  //StoreInst *SI1 = new StoreInst( cast<Value>(ConsInt), cast<Value>(StackVar), &(*FirstInst));
 
    //LoadInst *newLoadInst = new LoadInst((Value*)newStackVar, "temp1", &(*firstInst));
  //StoreInst *storeInst = new StoreInst( (Value*)consint, (Value*)stackVar, &(*firstInst));
  StringRef GVarStr("DDAProfilingFlag");
  GlobalVariable *GVar = F.getParent()->getGlobalVariable(GVarStr);
  StringRef GVarStr1("ProfilingLoopIterID");
  GlobalVariable *GVarIterID = F.getParent()->getGlobalVariable(GVarStr1);
  //StoreInst *SI2 = new StoreInst( cast<Value>(ConsInt), cast<Value>(GVar), &(*FirstInst));

  // Diff from Module.getGlobalContext(). 
  llvm::LLVMContext &Context = llvm::getGlobalContext();
#if 0
     IRBuilder<> builder(context);
     LoadInst *newLoadInst = builder.CreateLoad((Value*)newStackVar, "temp"); 
     newLoadInst->insertAfter(&(*firstInst));
     //LoadInst *newLoadInst = new LoadInst((Value*)newStackVar, s1, &(*firstInst));
#endif

     //
     LI = &getAnalysis<LoopInfo>(F);
     Loop *CurLoop = NULL;
     BasicBlock *LoopHeader, *LoopPredecessor;
     BasicBlock *If, *IfTrue, *IfFalse;
     for(LoopInfo::reverse_iterator LIB = LI->rbegin(), LIE = LI->rend(); LIB != LIE; ++LIB ){
       CurLoop = *LIB; 
       NumLoops++;
       cout<< "profiling loop: "<< NumLoops << endl;

       // Get loop header and predecessor.  
       if( (LoopHeader = CurLoop->getHeader()) == NULL){
         DEBUG(errs() << "No loop header in Function \n"<< F.getName().str(); ); 
         return 0;
       }
       if( (LoopPredecessor = CurLoop->getLoopPredecessor()) == NULL){
         DEBUG(errs() << "No loop predecessorr in Function \n"<< F.getName().str(); ); 
         return 0;
       }

       // Create local static flag variable for record LoopID2Name for each loop.
       Type *type = Type::getInt32Ty(getGlobalContext());
       Constant *consint = ConstantInt::get(type, 0);
       GlobalVariable *TimerInitFlag = new GlobalVariable(*CurModule, type, false, GlobalVariable::InternalLinkage, ConstantInt::getIntegerValue(type, APInt(32, 0)), Twine("TimerInit") );


       // Create and inserte profiling controller block. 
       type = Type::getInt32Ty(getGlobalContext());
       If = BasicBlock::Create(F.getContext(), "ProfileIf", &F, LoopHeader);
       IfTrue = BasicBlock::Create(F.getContext(), "ProfileIfTrue", &F, LoopHeader);
       IfFalse = BasicBlock::Create(F.getContext(), "ProfileIfFalse", &F, LoopHeader);
       LoadInst *GVarLoad = new LoadInst(TimerInitFlag, Twine(""), If);
       Value* IfCond = new ICmpInst(*If, ICmpInst::ICMP_EQ, cast<Value>(GVarLoad),  ConstantInt::getIntegerValue(type, APInt(32, 0)), "ProfileFlag" );
       BranchInst *ControlFlag = BranchInst::Create(IfTrue, IfFalse, IfCond, If); 
      
       // Insert code into IfFalse block.
       BranchInst *TermIfFalse = BranchInst::Create(LoopHeader, IfFalse);
       Builder->SetInsertPoint( &(IfFalse->front()) );
       CallInst *CallI = Builder->CreateCall(RDTSCFunction);
       Builder->CreateStore( cast<Value>(CallI), cast<Value>(Begin), true); 

       // Insert code into IfTrue block.
       // To add function call: __recordLoopID2Name, rdtsc.
       BranchInst *TermIfTrue = BranchInst::Create(LoopHeader, IfTrue);
       ConsInt = ConstantInt::get(type, 1);
       Builder->SetInsertPoint( &(IfTrue->front()) );  
       Builder->SetInsertPoint( &(IfTrue->front()) );  // For recordLoopID2Name().
       Builder->SetInsertPoint( &(IfTrue->front()) );  // For TimerInit.
       Builder->SetInsertPoint( &(IfTrue->front()) );  // Call rdtsc.
       Builder->SetInsertPoint( &(IfTrue->front()) );
      
       // LoopName: FileName_LineNo
       Value *ID = ConstantInt::get(SizeTy, NumLoops);
       BasicBlock::iterator first = LoopHeader->begin();
       string File = getInstFilename(first);
       unsigned int Line = getInstLineNumber(first);
       ostringstream Convert;
       Convert << Line;
       string LoopName = File + Convert.str();
       std::cout<< "LoopName " << LoopName << std::endl;
       Constant *LName = ConstantDataArray::getString(F.getParent()->getContext(), StringRef(LoopName) );
       GlobalVariable *Name = new GlobalVariable(*CurModule, LName->getType(), true, GlobalValue::InternalLinkage, LName, "LN");
       Value *PtrCharName = Builder->CreatePointerCast(Name, VoidPtrTy);
       Value *CallID2Nam3 = Builder->CreateCall2(RecordID2NameFunction, (Value*) (PtrCharName), cast<Value> (ID) );


       Builder->CreateStore( cast<Value>(ConsInt), cast<Value>(TimerInitFlag), true); 
       Value *TimerReturnVal = Builder->CreateCall(RDTSCFunction);
       Builder->CreateStore( TimerReturnVal, cast<Value>(Begin), true); 


       // Todo: multi successors in predecessor.
       // Let the predecessor point to the profiling controller block.
       TerminatorInst *TermInst = LoopPredecessor->getTerminator();
       int num = TermInst->getNumSuccessors();
       for( int i = 0; i < num; i++ ){ 
         BasicBlock *SuccessorBlock = TermInst->getSuccessor(i); // Multi Successor
         if( SuccessorBlock == LoopHeader )
           TermInst->setSuccessor(i, If);
       }


       // Get LoopExitingBlocks and insert profiling controller block for exit.
       SmallVector<BasicBlock*, 8> LoopExitingBlocks; 
       CurLoop->getExitingBlocks(LoopExitingBlocks);      
       cout << "Exiting Blocks: "<< LoopExitingBlocks.size() << endl;


       // Create and instert the profiling controller block if we let DDABeginCurFunc = 1 before.
       BasicBlock *ExitingBlock, *ExitIf, *ExitIfTrue, *ExitIfFalse;
       for(SmallVector<BasicBlock*, 8>::iterator BB = LoopExitingBlocks.begin(), BE = LoopExitingBlocks.end(); BB != BE; BB++){
         ExitingBlock = *BB;
         //cout << "ExitingBlock dump"<< endl;
         //ExitingBlock->dump();
         TerminatorInst *BBTermInst =ExitingBlock->getTerminator();
         int num =  BBTermInst->getNumSuccessors();
         for( int i = 0; i < num; i++ )
         { 

           BasicBlock *SuccessorBlock = BBTermInst->getSuccessor(i); // Multi Successor
           if(!CurLoop->contains(SuccessorBlock)){
             ExitIf = BasicBlock::Create(F.getContext(), "ProfileExitIf", &F, ExitingBlock);
             ExitIfTrue = BasicBlock::Create(F.getContext(), "ProfileExitIfTrue", &F, ExitingBlock);
             ExitIfFalse = BasicBlock::Create(F.getContext(), "ProfileExitIfFalse", &F, ExitingBlock);

             // Create ExitIf condition.
             LoadInst *TimerInitFlagLoad = new LoadInst(TimerInitFlag, Twine(""), ExitIf);
             Value* ExitIfCond = new ICmpInst(*ExitIf, ICmpInst::ICMP_EQ, TimerInitFlagLoad,  ConstantInt::getIntegerValue(type, APInt(32, 1)), "ProfileExitFlag" );
             BranchInst *ControlFlag = BranchInst::Create(ExitIfTrue, ExitIfFalse, ExitIfCond, ExitIf);

              // Insert code into IfExitTrue
             BranchInst *TermIfTrue = BranchInst::Create(SuccessorBlock, ExitIfTrue);
            Builder->SetInsertPoint( &(ExitIfTrue->front()) );  
            Value *RDTSCEnd = Builder->CreateCall(RDTSCFunction);
            Builder->CreateStore( RDTSCEnd, cast<Value>(End), true); 
            std::cout << "Begin type: " <<  Begin->getValueID() << std::endl;
            Value *IntBegin = Builder->CreateLoad(cast<Value>(Begin));
            Value *IntEnd = Builder->CreateLoad(cast<Value>(End));
            Value * CurTime = Builder->CreateSub(cast<Value>(IntEnd), cast<Value>(IntBegin));
            Value *ID = ConstantInt::get(SizeTy, NumLoops);
            Value *TotalTime = Builder->CreateCall2(RecordResultsFunction, ID, CurTime);



             BBTermInst->setSuccessor(i, ExitIf);
             //BranchInst *TermIfTrue = BranchInst::Create(SuccessorBlock, ExitIfTrue);
             BranchInst *TermIfFalse = BranchInst::Create(SuccessorBlock, ExitIfFalse);
           }
         }

       } 

     }
#endif  


     return true;
}



#define PRINT_MODULE dbgs() << \
  "\n\n=============== Module Begin ==============\n" << M << \
"\n================= Module End   ==============\n"
bool TimerProfiling::runOnModule(Module &M){
  Context = &M.getContext();

  DEBUG(dbgs() 
      << "************************************\n"
      << "************************************\n"
      << "**                                **\n"
      << "** LDDA PROFILING INSTRUMENTATION  **\n"
      << "**                                **\n"
      << "************************************\n"
      << "************************************\n");

  // Get debug info
  CurModule = &M;
  dbgKind = M.getContext().getMDKindID("dbg");

  // Create function prototypes
  Type *VoidTy = Type::getVoidTy(M.getContext());
  VoidPtrTy = Type::getInt8PtrTy(M.getContext());
  SizeTy = IntegerType::getInt64Ty(M.getContext());
  SizeTy32 = IntegerType::getInt32Ty(M.getContext());

  M.getOrInsertFunction("__initprofiling", VoidTy, NULL);
  M.getOrInsertFunction("__outputLoopID2Name", VoidTy, NULL);
  M.getOrInsertFunction("__outputTimerResults", VoidTy, NULL);

  M.getOrInsertFunction("__recordLoopID2Name", VoidTy, VoidPtrTy, SizeTy, NULL);
  M.getOrInsertFunction("__recordTimerResults", VoidTy, SizeTy, SizeTy, NULL);
  M.getOrInsertFunction("__rdtsc", SizeTy, NULL);


  // Find the Module with main function.
  Function *Main = M.getFunction("main");

  if( Main){

    //  Insert declaration in main-Module, extern global variable in non-main modoule.
    Type *type = Type::getInt32Ty(getGlobalContext());
    Constant *consint = ConstantInt::get(type, 0);
    GlobalVariable *intval = new GlobalVariable(M, type, false, GlobalVariable::AvailableExternallyLinkage , ConstantInt::getIntegerValue(type, APInt(32, 0)), Twine("DDAProfilingFlag") );
    GlobalVariable *GIterID = new GlobalVariable(M, type, false, GlobalVariable::AvailableExternallyLinkage , ConstantInt::getIntegerValue(type, APInt(32, 0)), Twine("ProfilingLoopIterID") );
    cout<< "global type: "<< intval->getType()->getTypeID() << endl;;
    //GlobalVariable *intval = new GlobalVariable(M, type, false, GlobalValue::ExternalLinkage, consint, s2);


    // Instrument 'main' for init at the front of main.
  runOnFunction(*Main, 0);

  }

  std::vector<Constant*> ftInit;
  unsigned functionNumber = 0;
  for(Module::iterator F = M.begin(), E = M.end(); F != E; F++){
    if( F->isDeclaration())
      continue;

    DEBUG(dbgs() << "Function: " << F->getName() << "\n");
    functionNumber++;

    currentFunctionNumber = functionNumber;
    runOnFunction(*F);
  }
  return true;
} // end runOnModule


void 
TimerProfiling::instrumentInit(  Function *Check, Instruction &I)
{
  Builder->SetInsertPoint(&I);
  CallInst *CallI = Builder->CreateCall(Check);

  // Copy debug information if it is present.
  if (MDNode *MD = I.getMetadata("dbg"))
  CallI->setMetadata("dbg", MD);
}


void 
TimerProfiling::instrument(Value *Pointer, Value *AccessSize, Value* AddrPos, Value* VarName, Function *Check, Instruction &I){
  Builder->SetInsertPoint(&I); 
  Value *VoidPointer = Builder->CreatePointerCast(Pointer, VoidPtrTy);
  Value *PtrVarName = Builder->CreatePointerCast(VarName, VoidPtrTy);
  Value *PtrAddrPos = Builder->CreatePointerCast(AddrPos, VoidPtrTy);
  //CallInst *CallI = Builder->CreateCall2(Check, VoidPointer, AccessSize);
  CallInst *CallI = Builder->CreateCall4(Check, VoidPointer, AccessSize, PtrAddrPos, PtrVarName);

  // Copy debug information if it is present.
  if (MDNode *MD = I.getMetadata("dbg"))
    CallI->setMetadata("dbg", MD);
}

void TimerProfiling::visitReturnInst(ReturnInst &LI){
  
   //instrumentInit( OutputID2NameFunction, *firstInst);
   //instrumentInit( OutputResultsFunction, *firstInst);
   Builder->SetInsertPoint(&LI);
  // Builder->CreateCall(OutputID2NameFunction); 
  
  Value *RDTSCFEnd = Builder->CreateCall(RDTSCFunction);
  Builder->CreateStore( RDTSCFEnd, cast<Value>(FEnd), true); 
  Value *IntBegin = Builder->CreateLoad(cast<Value>(FBegin));
  Value *IntEnd = Builder->CreateLoad(cast<Value>(FEnd));
  Value * CurTime = Builder->CreateSub(cast<Value>(IntEnd), cast<Value>(IntBegin));
  Value *ID = ConstantInt::get(SizeTy, 0);
  Value *TotalTime = Builder->CreateCall2(RecordResultsFunction, ID, CurTime);

  Builder->CreateCall(OutputResultsFunction); 
  ++LoadsInstrumented;
}



  std::string 
TimerProfiling::getInstFilename(Instruction*I)
{
  if( MDNode *Dbg = I->getMetadata(dbgKind) ){
    DILocation Loc(Dbg);
    std::cout<< "FileName" << Loc.getFilename().str() << std::endl;
    return( Loc.getFilename().str() );
    //return(Loc.getDirectory().str() + "/" + Loc.getFilename().str());
  }
  else{
    cout<<"getInstFilename can not get dbg information "<< endl;
    I->dump();
    return "Cannot get File name \n";    
  }
}

unsigned int TimerProfiling::getInstLineNumber(Instruction*I)
{
  if( MDNode *Dbg = I->getMetadata(dbgKind) ){
    DILocation Loc(Dbg);
    return( Loc.getLineNumber() );
  }
  else{
    cout<<"getInstFilename can not get dbg information "<< endl;
    return 0;    
  }
}
  std::string 
TimerProfiling::getInstFuncname(Instruction*I)
{
#if 0
  if( MDNode *Dbg = I->getMetadata(dbgKind) ){

    if( Dbg->getFunction() )
      return ( Dbg->getFunction()->getName().str() );
    else
      return "no function" ;

  }
  else{
    cout<<"getInstFuncname can not get dbg information "<< endl;
    return 0;    
  }
#endif
  BasicBlock *bb = I->getParent();
  Function* f = bb->getParent();
  return( f->getName().str() );
}
