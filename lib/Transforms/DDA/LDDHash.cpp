//===- LDDProfiling.cpp - Inserts counters for data dependence profiling ------------===//
////
////                      The LLVM Compiler Infrastructure
////
//// This file is distributed under the University of Illinois Open Source
//// License. See LICENSE.TXT for details.
////
////===----------------------------------------------------------------------===//
//
//  LDD: Loop Data Dependence
//===------------------------------------------------------------------------===//
//

#define DEBUG_TYPE "insert-ldd-profiling"
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



// Add global
#include "llvm/ADT/Twine.h"
#include "llvm/GlobalVariable.h"
#include"llvm/DerivedTypes.h"
#include"llvm/ADT/APInt.h"

#include "LDDHash.h"

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


  class LDDProfiling: public ModulePass,
  public InstVisitor<LDDProfiling>{
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
      IntegerType *SizeInt32Ty;
      PointerType *StringPtrTy;

      Function *StoreCheckFunction;
      Function *LoadCheckFunction;
      Function *StoreCheckStackVarFunction;
      Function *LoadCheckStackVarFunction;
      Function *InitProfilingFunction;
      Function *OutputDependenceFunction;
      Function *AddNewProfilingBufferFunction;

      void instrument(Value *Pointer, Value *Accesssize, Value* AddrPos, Value*VarName,  Function *Check, Instruction &I);
      void instrumentstackvar(Value *Pointer, Value *Accesssize, Value* AddrPos, Value*VarName, Value* DDABeginCurFunc,  Function *Check, Instruction &I);
      //void instrumentInit( Function *Check, BasicBlock &I);
      void instrumentInit( Function *Check, Instruction &I);

      void instrumentaddnewbuffer(Function*addnewbuffer, Value *FName, Value* LPos, Instruction &I);

      // Debug info
      unsigned dbgKind;

      // LoopInfo
      LoopInfo *LI;
      unsigned NumLoops; // ID of Profiled loops.

      // Control whether to profile stack variables.
      int NoTargetLoops; // init = 0. if NoTargetLoops = 1, not profiling all stack variables.
      std::map<int, int> LoopScope;  //[FirstLine, LastLine)
      std::vector<std::string> LocalSharedVars;



    public:
      static char ID;
      LDDProfiling(): ModulePass(ID){ 
        std::fstream fs;
        fs.open("dda-init-setup", std::fstream::in); 
        fs >> NumLoops; 
        fs.close();
      }
      ~LDDProfiling(){
        std::fstream fs;
        fs.open("dda-init-setup", std::fstream::out); 
        fs << NumLoops; 
        fs.close();
      }


      //LDDProfiling(): FunctionPass(ID){}

      virtual bool doInitialization(Module &M);
      bool runOnModule(Module &M);

      // Instrument each function for LDD
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

char LDDProfiling::ID = 0;

static RegisterPass<LDDProfiling> X("insert-ldd-hash", "Loop Dada Dependence Profiling Pass based Hash", false, false);

#if 0
INITIALIZE_PASS(LDDProfiling, "insert-ldd-profiling", "Insert LDD Profling", false, false)
FunctionPass *llvm::createLDDProfilingPass(){
  return new LDDProfiling();
}
#endif 


// Not called now. 20130922
bool LDDProfiling::doInitialization(Module &M){
  Type *VoidTy = Type::getVoidTy(M.getContext());
  VoidPtrTy = Type::getInt8PtrTy(M.getContext());
  SizeTy = IntegerType::getInt64Ty(M.getContext());
  StringPtrTy = Type::getInt8PtrTy(M.getContext());

  // Create function prototypes
  //M.getOrInsertFunction("__loadcheck", VoidTy, VoidPtrTy, SizeTy, NULL);
  M.getOrInsertFunction("__loadcheck", VoidTy, VoidPtrTy, SizeTy, VoidPtrTy, VoidPtrTy,  NULL);
  M.getOrInsertFunction("__storecheck", VoidTy, VoidPtrTy, SizeTy, VoidPtrTy,VoidPtrTy, NULL);
  M.getOrInsertFunction("__initprofiling", VoidTy, NULL);
  M.getOrInsertFunction("__outputdependence", VoidTy, NULL);

  //M.getOrInsertFunction("__initprofiling", VoidTy, VoidPtrTy, SizeTy, NULL);
  return true;
}

// runOnFunction(main)
// Instrument functions: __initprofiling and output.
bool LDDProfiling::runOnFunction(Function &F, int flag){


  InitProfilingFunction = F.getParent()->getFunction("__initprofiling");
  assert(InitProfilingFunction && "__initprofiling function has disappeared! \n");

  OutputDependenceFunction = F.getParent()->getFunction("__outputdependence");
  assert(OutputDependenceFunction && "__outputdependence function has disappeared! \n");

  TD = &getAnalysis<DataLayout>(); // 20130325
  IRBuilder<> FuncBuilder(F.getContext());
  Builder = &FuncBuilder;

  inst_iterator firstInst = inst_begin(F);
  instrumentInit( InitProfilingFunction, *firstInst);

  // Insert output function.


  return true;
}

// runOnFunction( anyFuncofModule )
bool LDDProfiling::runOnFunction(Function &F){

  // Check that the load and store check functions are declared.
  LoadCheckFunction = F.getParent()->getFunction("__loadcheck");
  assert(LoadCheckFunction && "__loadcheck function has disappeared!\n");

  //errs() << "Data Dependence Profiling: ";
  //errs().write_escaped( F.getName() ) << "\n";
  StoreCheckFunction = F.getParent()->getFunction("__storecheck");
  assert(StoreCheckFunction &&"__storecheck function has disappeared! \n");

  InitProfilingFunction = F.getParent()->getFunction("__initprofiling");
  assert(InitProfilingFunction &&"__initprofiling function has disappeared! \n");

  AddNewProfilingBufferFunction = F.getParent()->getFunction("__addnewprofilingbuffer");
  assert(AddNewProfilingBufferFunction  &&"__addnewprofilingbuffer  function has disappeared! \n");

  LoadCheckStackVarFunction = F.getParent()->getFunction("__loadcheckstackvar");
  assert(LoadCheckStackVarFunction && "__loadcheckstackvar function has disappeared!\n");

  StoreCheckStackVarFunction = F.getParent()->getFunction("__storecheckstackvar");
  assert(StoreCheckStackVarFunction &&"__storecheckstackvar function has disappeared! \n");

  TD = &getAnalysis<DataLayout>(); // 20130325
  IRBuilder<> TheBuilder(F.getContext());
  Builder = &TheBuilder;

  // Find the outermost loop lexical scope: FirstLineNo, LastLineNo.
  // 
#if  1
  LoopScope.clear();
  LocalSharedVars.clear();

  LI = &getAnalysis<LoopInfo>(F);
  Loop *LocalLoop = NULL;
  BasicBlock *Header, *Tail;
  int FLineNo, MinLine = 0, num = 0;
  for(LoopInfo::reverse_iterator LIB = LI->rbegin(), LIE = LI->rend(); LIB != LIE; ++LIB ){
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
    for(SmallVector<BasicBlock*, 8>::iterator BB = LoopExitBlocks.begin(), BE = LoopExitBlocks.end(); BB != BE; BB++){
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
    std::cout << "FirstLineNo: "<< FLineNo << " MaxLine:" << " " << MinLine << endl;
  }
#endif



  // Declare and Insert Local Variables here. 
  // Declare local variable  DDABeginCurFunc in the front of Function.
  inst_iterator FirstInst = inst_begin(F);
  Type *VarType = Type::getInt32Ty(F.getContext());
  Twine StackVarStr("DDABeginCurFunc");
  AllocaInst * StackVar = new AllocaInst(VarType, Twine("DDABeginCurFunc"), &(*FirstInst) ) ; 
  StackVar->setAlignment(4);


  // Visit all of the instructions in the function.
  // And instrument load/store operation in the current function when needed.
  visit(F);


  // Insert DDA Profiling Controller Codes.
#ifdef _DDA_OUTERMOST_PROFILING

  // Init Declared Variables: DDABeginCurFunc = 0.
  Constant *ConsInt = ConstantInt::get(VarType, 0);
  StoreInst *SI1 = new StoreInst( cast<Value>(ConsInt), cast<Value>(StackVar), &(*FirstInst));


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
      std::cout << "No loop header in Function \n"<< F.getName().str(); 
      DEBUG(errs() << "No loop header in Function \n"<< F.getName().str(); ); 
      return 0;
    }

    // Get the loop line.
    //BasicBlock::iterator BIFI = LoopHeader->begin(); 
    //Instruction *FirstInst = &*(++BIFI); 

    // Not need this ???
#if 1 
    if( (LoopPredecessor = CurLoop->getLoopPredecessor()) == NULL){
      std::cout << "No loop predecessorr in Function \n"<< F.getName().str();  
      DEBUG(errs() << "No loop predecessorr in Function \n"<< F.getName().str(); ); 
      return 0;
    }
#endif

#if 0
    //cout<<"GVar type "<< GVar->getType()->getTypeID() << endl;
    //cout<<"Stack type" << StackVar->getType()->getTypeID() << endl;
#endif

    // Create and inserte profiling controller block. 
    Type* type = Type::getInt32Ty(getGlobalContext());
    If = BasicBlock::Create(F.getContext(), "ProfileIf", &F, LoopHeader);
    IfTrue = BasicBlock::Create(F.getContext(), "ProfileIfTrue", &F, LoopHeader);
    IfFalse = BasicBlock::Create(F.getContext(), "ProfileIfFalse", &F, LoopHeader);
    LoadInst *GVarLoad = new LoadInst(GVar, Twine(""), If);

    //if (MDNode *MD = I.getMetadata("dbg"))
    //  CallI->setMetadata("dbg", MD); 

    //PtrToIntInst *GVarInt = new PtrToIntInst::PtrToIntInst( GVar, type,  Twine(""), If);
    //Value* IfCond = new ICmpInst(*If, ICmpInst::ICMP_SLT, cast<Value>( ConstantInt::getIntegerValue(Type::getInt32Ty(getGlobalContext()), APInt(32, 1))),cast<Value>( ConstantInt::getIntegerValue(Type::getInt32Ty(getGlobalContext()), APInt(32, 1))), "ProfileFlag" );


    // Create and Insert DDAProfilingFlang = N;
    Value* IfCond = new ICmpInst(*If, ICmpInst::ICMP_EQ, cast<Value>(GVarLoad),  ConstantInt::getIntegerValue(type, APInt(32, 0)), "ProfileFlag" );
    BranchInst *ControlFlag = BranchInst::Create(IfTrue, IfFalse, IfCond, If); 
    BranchInst *TermIfFalse = BranchInst::Create(LoopHeader, IfFalse);
    Constant *ConsInt = ConstantInt::get(type, NumLoops);
    StoreInst *Stor2GVar = new StoreInst( cast<Value>(ConsInt), cast<Value>(GVar), IfTrue);

    //instrumentInit( AddNewProfilingBufferFunction, *(IfTrue->end()) ); 

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

    // Get the Loop Line number.
    //MDNode * LMDN = CurLoop->getLoopID();
    ///LMDN->dump();
    //
    int LoopLine = getInstLineNumber(FirstInst);
    ostringstream Convert;
    Convert << LoopLine;
    std::string LoopPosStr = FileName + Convert.str();
    cout << "LoopPosStr: " << LoopPosStr << endl;

    //std::string FuncNameStr = F.getName().str();
    int status;
    std::string FuncNameStr = FirstInst->getParent()->getParent()->getName().str();

    // demangling the name.
    char *DMFuncNameStr = __cxa_demangle(FuncNameStr.c_str(), 0, 0, &status);
    if( status == 0 ){
      FuncNameStr = DMFuncNameStr;
      free( DMFuncNameStr);
    }
    int left = FuncNameStr.find("(");
    FuncNameStr = FuncNameStr.substr(0, left);
    cout << "Func Name : " << FuncNameStr << endl;

    Constant *LoopPosCst = ConstantDataArray::getString(F.getParent()->getContext(), StringRef(LoopPosStr) ); 
    LoopPos = new GlobalVariable(*CurModule, LoopPosCst->getType(), true, GlobalValue::ExternalLinkage, LoopPosCst, LoopPosStr);                     


    Constant *FuncNameCst = ConstantDataArray::getString(F.getParent()->getContext(), StringRef(FuncNameStr) ); 
    FuncName = new GlobalVariable(*CurModule, FuncNameCst->getType(), true, GlobalValue::ExternalLinkage, FuncNameCst, FuncNameStr);                     

    ConsInt = ConstantInt::get(type, 1);
    StoreInst *Stor2StackVar = new StoreInst( cast<Value>(ConsInt), cast<Value>(StackVar), IfTrue);
    // ProfilingLoopIterID = 1; 
    StoreInst *Stor2IterID = new StoreInst( cast<Value>(ConsInt), cast<Value>(GVarIterID), IfTrue);

    std::cout<<"insert AddNewProfilingBufferFunction"<< std::endl;
    //

    instrumentaddnewbuffer( AddNewProfilingBufferFunction, (Value*) FuncName, (Value*) LoopPos, *( ++(IfTrue->begin()) ) ); 
    //instrumentInit( AddNewProfilingBufferFunction, *( ++(IfTrue->begin()) ) ); 

    // Insert ProfilingLoopIterID += DDABeginCurFunc;  in the loop.body
    TerminatorInst *LHTermInst =LoopHeader->getTerminator();
    BasicBlock *LoopBody = LHTermInst->getSuccessor(0);
    IRBuilder<> TheBuilder(LoopBody->getContext());
    TheBuilder.SetInsertPoint(&(LoopBody->front()));
    TheBuilder.SetInsertPoint(&(LoopBody->front()));
    LoadInst *IterIDLoad = new LoadInst(GVarIterID, Twine(""), &(LoopBody->front()));
    //if (MDNode *MD = ((LoopBody->front())).getMetadata("dbg"))
    //  IterIDFuncLoad->setMetadata("dbg", MD); 
    LoadInst *CurFuncLoad = new LoadInst(StackVar, Twine(""), &(LoopBody->front()));

    // if (MDNode *MD = ((LoopBody->front())).getMetadata("dbg"))
    //   CurFuncLoad->setMetadata("dbg", MD); 

    Value *IterIDAdd = TheBuilder.CreateAdd(cast<Value>(IterIDLoad), cast<Value>(CurFuncLoad), Twine("")); 
    //Value *IterIDAdd = TheBuilder.CreateAdd(cast<Value>(IterIDLoad), cast<Value>(ConsInt), Twine("")); 
    Value *StorAdd2IterID = TheBuilder.CreateStore(cast<Value>(IterIDAdd), cast<Value>(GVarIterID));


    BranchInst *TermIfTrue = BranchInst::Create(LoopHeader, IfTrue);
    //F.dump();

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
      // fixed c++ exception, destructor and longjmp .20140826.
      if( InvokeInst *invokeInst = dyn_cast<InvokeInst>(&*BBTermInst) ){                                                                                  
        //BBTermInst->dump(); // debug 
        continue;
      }
      int num =  BBTermInst->getNumSuccessors();
      for( int i = 0; i < num; i++ )
      { 

        BasicBlock *SuccessorBlock = BBTermInst->getSuccessor(i); // Multi Successor
        if(!CurLoop->contains(SuccessorBlock)){
          ExitIf = BasicBlock::Create(F.getContext(), "ProfileExitIf", &F, ExitingBlock);
          ExitIfTrue = BasicBlock::Create(F.getContext(), "ProfileExitIfTrue", &F, ExitingBlock);
          ExitIfFalse = BasicBlock::Create(F.getContext(), "ProfileExitIfFalse", &F, ExitingBlock);
          LoadInst *StackVarLoad = new LoadInst(StackVar, Twine(""), ExitIf);
          Value* ExitIfCond = new ICmpInst(*ExitIf, ICmpInst::ICMP_EQ, StackVarLoad,  ConstantInt::getIntegerValue(type, APInt(32, 1)), "ProfileExitFlag" );
          BranchInst *ControlFlag = BranchInst::Create(ExitIfTrue, ExitIfFalse, ExitIfCond, ExitIf);
          Constant *ConsInt = ConstantInt::get(VarType, 0);
          StoreInst *Stor2GVar = new StoreInst( cast<Value>(ConsInt), cast<Value>(GVar), ExitIfTrue);
          StoreInst *Stor2StackVar = new StoreInst( cast<Value>(ConsInt), cast<Value>(StackVar), ExitIfTrue);

          BBTermInst->setSuccessor(i, ExitIf);
          BranchInst *TermIfTrue = BranchInst::Create(SuccessorBlock, ExitIfTrue);
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
bool LDDProfiling::runOnModule(Module &M){
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

  // Find the Module with main function.

  // Initialization in every module.
  Type *type = Type::getInt32Ty(getGlobalContext());
  Constant *consint = ConstantInt::get(type, 0);
  GlobalVariable *intval = new GlobalVariable(M, type, false, GlobalVariable::AvailableExternallyLinkage , ConstantInt::getIntegerValue(type, APInt(32, 0)), Twine("DDAProfilingFlag") );
  GlobalVariable *GIterID = new GlobalVariable(M, type, false, GlobalVariable::AvailableExternallyLinkage , ConstantInt::getIntegerValue(type, APInt(32, 0)), Twine("ProfilingLoopIterID") );
  cout<< "global type: "<< intval->getType()->getTypeID() << endl;;
  //GlobalVariable *intval = new GlobalVariable(M, type, false, GlobalValue::ExternalLinkage, consint, s2);


  Type *VoidTy = Type::getVoidTy(M.getContext());
  VoidPtrTy = Type::getInt8PtrTy(M.getContext());
  SizeTy = IntegerType::getInt64Ty(M.getContext());
  SizeInt32Ty = IntegerType::getInt32Ty(M.getContext());

  // Create function prototypes
  M.getOrInsertFunction("__loadcheck", VoidTy, VoidPtrTy, SizeTy, VoidPtrTy, VoidPtrTy,  NULL);
  M.getOrInsertFunction("__storecheck", VoidTy, VoidPtrTy, SizeTy, VoidPtrTy,VoidPtrTy, NULL);

  M.getOrInsertFunction("__loadcheckstackvar", VoidTy, VoidPtrTy, SizeTy, VoidPtrTy, VoidPtrTy, SizeInt32Ty, NULL);
  M.getOrInsertFunction("__storecheckstackvar", VoidTy, VoidPtrTy, SizeTy, VoidPtrTy,VoidPtrTy, SizeInt32Ty, NULL);
  //M.getOrInsertFunction("__initprofiling", VoidTy, VoidPtrTy, SizeTy, NULL);
  M.getOrInsertFunction("__initprofiling", VoidTy,  NULL);
  M.getOrInsertFunction("__outputdependence", VoidTy,  NULL);
  M.getOrInsertFunction("__addnewprofilingbuffer", VoidTy, VoidPtrTy, VoidPtrTy, NULL);

  // Instrument 'main' for init at the front of main.
  Function *Main = M.getFunction("main");
  if( Main){
    //M.getOrInsertFunction("__outputdependence", VoidTy,  NULL);
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
LDDProfiling::instrumentInit(  Function *Check, Instruction &I)
{
  Builder->SetInsertPoint(&I);
  CallInst *CallI = Builder->CreateCall(Check);

  // Copy debug information if it is present.
  if (MDNode *MD = I.getMetadata("dbg"))
    CallI->setMetadata("dbg", MD);
}


void 
LDDProfiling::instrument(Value *Pointer, Value *AccessSize, Value* AddrPos, Value* VarName, Function *Check, Instruction &I){
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
  void
LDDProfiling::instrumentaddnewbuffer(Function *AddNewProfilingBuffer, Value *FName, Value* LPos, Instruction &I )
{
  Builder->SetInsertPoint(&I); 
  Value *FuncName = Builder->CreatePointerCast(FName, VoidPtrTy);
  Value *LoopPos = Builder->CreatePointerCast(LPos, VoidPtrTy);
  //CallInst *CallI = Builder->CreateCall2(Check, VoidPointer, AccessSize);
  CallInst *CallI = Builder->CreateCall2(AddNewProfilingBuffer, FuncName, LoopPos);

  // Copy debug information if it is present.
  if (MDNode *MD = I.getMetadata("dbg"))
    CallI->setMetadata("dbg", MD);


}



void 
LDDProfiling::instrumentstackvar(Value *Pointer, Value *AccessSize, Value* AddrPos, Value* VarName, Value* CurFunc, Function *Check, Instruction &I){
  Builder->SetInsertPoint(&I); 
  Value *VoidPointer = Builder->CreatePointerCast(Pointer, VoidPtrTy);
  Value *PtrVarName = Builder->CreatePointerCast(VarName, VoidPtrTy);
  Value *PtrAddrPos = Builder->CreatePointerCast(AddrPos, VoidPtrTy);
  //CallInst *CallI = Builder->CreateCall2(Check, VoidPointer, AccessSize);
  CallInst *CallI = Builder->CreateCall5(Check, VoidPointer, AccessSize, PtrAddrPos, PtrVarName, CurFunc);
  //CallInst *CallI = Builder->CreateCall4(Check, VoidPointer, AccessSize, PtrAddrPos, PtrVarName);

  // Copy debug information if it is present.
  if (MDNode *MD = I.getMetadata("dbg"))
    CallI->setMetadata("dbg", MD);
}

void LDDProfiling::visitLoadInst(LoadInst &LI){
  // flag: whether  __loadcheckstackvar a stack variable.
  int DoProfile = 0;

  Value *AccessSize = ConstantInt::get(SizeTy, TD->getTypeStoreSize(LI.getType()));

  // Extract written/read variables information.
  StringRef Var = LI.getPointerOperand()->getName();


  // Filter un-need profiling vars.
  if( Var.str() == "retval" ){
    cout << "return value: " << Var.str()<< endl;
    return ;
  }

  std::vector<string>::iterator LSVB, LSVE; 
  Value *LoadValue = LI.getOperand(0);

  // Local Scalar variable, pointers.
  if(isa<AllocaInst>(LoadValue ) ){
    if(!isa<PointerType>(cast<AllocaInst>(LoadValue)->getAllocatedType() )){

      // is LocalSharedVars?
      for( LSVB = LocalSharedVars.begin(), LSVE = LocalSharedVars.end(); LSVB != LSVE; ++LSVB){
        if( *LSVB == Var.str()){
          // if is local shared scalar/array, call loadcheckstackvar 
          DoProfile = 1; 
          //cout <<"Load Profilie: " <<  *LSVB << endl;
          break;
        }
      } 

      // Declared within loop scope.
      if( !DoProfile ){

        //cout <<Var.str() << "--------------------- pointer"<< endl;
        return ;
      }

    }
    // int *ptr;  local && Pointer
    else{
      return;
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
        BasicBlock::iterator bbitprev = bbit; // keep the lates related getelementptr
        StringRef VarCurName = Var;
        StringRef VarPrevName; 
        //cout <<"init VarCurName: " <<  VarCurName.str() << endl;

        //bbit->dump();
        --bbit;
        //bbit->dump();
        //cout << "next name: " << bbit->getName().str() << endl;

        // Advance to the final getelementptr.
        while( bbit != bb->begin() && bbit->getName() != VarCurName ){
          --bbit;
          //bbit->dump();
          //cout << "here name = \n"<< bbit->getName().str()<< endl ;
        }

        //
        VarPrevName = bbit->getName(); 
        //if( bbit->getOpcode() == Instruction::GetElementPtr  && VarCurName == VarPrevName ){
        if( VarCurName == VarPrevName ){
          // cout << " find name \n";
          FindOrigName = 1;
          // Same name rule.
          // To be opt. 
          while(bbit != bb->begin()){ 
            //while( VarPrevName == VarCurName){

            if( VarPrevName == VarCurName ){
              VarCurName = bbit->getOperand(0)->getName();   // load or getelementptr
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
          //  getElementType()   isPrimitiveType()   PointerType::getArrayElementType getAllocatedType
          //bbitprev->dump();
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
            if ( isa<ArrayType>( (cast<AllocaInst>(operand)->getAllocatedType())) ){
              PointerType *OprPointerType = cast<AllocaInst>(operand)->getType();
              Type* ArrayEleType = OprPointerType->getArrayElementType();

              //ArrayEleType->dump(); 
              // Get the array element type.
              while( isa<ArrayType>(ArrayEleType) ) 
                ArrayEleType = ArrayEleType->getArrayElementType();
              //ArrayEleType->dump(); 
              //cout << endl; 

              if( ArrayEleType->isIntegerTy() || ArrayEleType->isFloatingPointTy() || ArrayEleType->isDoubleTy()  ) 
                cout<< endl  << "local primitive array: "<< Var.str() << endl;
              DoProfile = 1;
            }
            //cout << "local: "<< Var.str() << endl;
          }


        }
      }


      }


      // Extract written/read variables information.
      GlobalVariable *GAddrPos, *GVarName;
      std::string File = getInstFileName(&LI);
      // If cannot get FileName, don't profiling the instruction.
      if( File == "0")
        return;
      unsigned int  Line = getInstLineNumber(&LI);
      //StringRef Func = LI.getParent()->getParent()->getName();

      // Generate VarName and AddrPos.
      ostringstream Convert;
      Convert << Line;
      string StrPos = File + Convert.str();

      // Create global Variable name to store AddrPos.
      std::string PosName = "GAddrPos";
      unsigned int DotPos = File.find(".");
      std::string FileName = File.substr(0, DotPos);
      PosName += FileName;
      if( Pos[StrPos] ){
        GAddrPos = Pos[StrPos]; 
      }
      else{
        Constant *AddrPos = ConstantDataArray::getString(*Context, StringRef(StrPos) ); 
        GAddrPos = new GlobalVariable(*CurModule, AddrPos->getType(), true, GlobalValue::ExternalLinkage, AddrPos, PosName); 
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
        GVarName = new GlobalVariable(*CurModule, VarName->getType(), true, GlobalValue::ExternalLinkage, VarName, GlobalVarName); 
        Name[Var.str()] = GVarName;
      }

      //cout << StrPos << " "<< Var.str() << " " << File << " " << Line << endl;

      if( !DoProfile )
        instrument(LI.getPointerOperand(), AccessSize,(Value*)GAddrPos, (Value*)GVarName,  LoadCheckFunction, LI);
      else{

        Function *F = LI.getParent()->getParent();
        inst_iterator FirstInst = inst_begin(F); 
        Value* DDABeginCurFuncDec = &(*FirstInst);
        LoadInst *CurFuncLoad = new LoadInst(DDABeginCurFuncDec, Twine(""), &LI);
        instrumentstackvar(LI.getPointerOperand(), AccessSize,(Value*)GAddrPos, (Value*)GVarName, (Value*)CurFuncLoad, LoadCheckStackVarFunction, LI);

      }
      ++LoadsInstrumented;

    }


    void LDDProfiling::visitStoreInst(StoreInst &SI){
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
          for( LSVB = LocalSharedVars.begin(), LSVE = LocalSharedVars.end(); LSVB != LSVE; ++LSVB){
            if( *LSVB == Var.str()){
              DoProfile = 1; 
              //cout <<"Store Profile: " <<  *LSVB << endl;
              break;
            }
          } 
          if( !DoProfile ){
            //cout <<Var.str() << "--------------------- Not Profile in StoreInst"<< endl;
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
            BasicBlock::iterator bbitprev = bbit; // keep the lates related getelementptr
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
            //if( bbit->getOpcode() == Instruction::GetElementPtr  && VarCurName == VarPrevName ){
            if( VarCurName == VarPrevName ){
              //cout << " find name \n";
              FindOrigName = 1;

              // To be opt. 
              while(bbit != bb->begin()){ 
                //while( VarPrevName == VarCurName){

                // Same name rule.
                if( VarPrevName == VarCurName ){
                  VarCurName = bbit->getOperand(0)->getName();   // load or getelementptr
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
              //  getElementType()   isPrimitiveType()   PointerType::getArrayElementType getAllocatedType
              //
              //bbitprev->dump();
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
                if ( isa<ArrayType>( (cast<AllocaInst>(operand)->getAllocatedType())) ){
                  PointerType *OprPointerType = cast<AllocaInst>(operand)->getType();
                  Type* ArrayEleType = OprPointerType->getArrayElementType();

                  //ArrayEleType->dump(); 
                  // Get the array element type.
                  while( isa<ArrayType>(ArrayEleType) ) 
                    ArrayEleType = ArrayEleType->getArrayElementType();
                  //ArrayEleType->dump(); 

                  if( ArrayEleType->isIntegerTy() || ArrayEleType->isFloatingPointTy() || ArrayEleType->isDoubleTy()  ) 
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
          std::string PosName = "GAddrPos";
          unsigned int DotPos = File.find(".");
          std::string FileName = File.substr(0, DotPos);
          PosName += FileName;

          // Create Global Variable Name to store VarName. 
          std::string  GlobalVarName = "GVarName";
          GlobalVarName += FileName;
          //std::cout<< "new name " << GlobalVarName << std::endl;



          if( Pos[StrPos] ){
            GAddrPos = Pos[StrPos]; 
          }
          else{
            Constant *AddrPos = ConstantDataArray::getString(*Context, StringRef(StrPos) ); 
            GAddrPos = new GlobalVariable(*CurModule, AddrPos->getType(), true, GlobalValue::ExternalLinkage, AddrPos, PosName); 
            Pos[StrPos] = GAddrPos; 
          }


          if( Name[Var.str()] ){
            GVarName = Name[Var.str()];
          }
          else{
            Constant *VarName = ConstantDataArray::getString(*Context, Var); 
            GVarName = new GlobalVariable(*CurModule, VarName->getType(), true, GlobalValue::ExternalLinkage, VarName, GlobalVarName); 
            Name[Var.str()] = GVarName;
          }

          //instrument(SI.getPointerOperand(), AccessSize,(Value*)GAddrPos, (Value*)GVarName, StoreCheckFunction, SI);
          //instrumentInit(InitProfilingFunction, SI);


          if( !DoProfile )
            instrument(SI.getPointerOperand(), AccessSize,(Value*)GAddrPos, (Value*)GVarName,  StoreCheckFunction, SI);
          else{
            Function *F = SI.getParent()->getParent();
            inst_iterator FirstInst = inst_begin(F); 
            Value* DDABeginCurFuncDec = &(*FirstInst);
            LoadInst *CurFuncLoad = new LoadInst(DDABeginCurFuncDec, Twine(""), &SI);
            instrumentstackvar(SI.getPointerOperand(), AccessSize,(Value*)GAddrPos, (Value*)GVarName, (Value*)CurFuncLoad, StoreCheckStackVarFunction, SI);

          }


          ++StoresInstrumented;
        }


        void LDDProfiling::visitReturnInst(ReturnInst &I){

          // is main function ?
          Function * F = I.getParent()->getParent();
          string FuncName = F->getName().str();

          if( FuncName == "main"){
#if  1
            //OutputDependenceFunction = F->getParent()->getFunction(" __outputdependence");
            //assert(OutputDependenceFunction && "__outputdependence function has disappeared! \n");

            //TD = &getAnalysis<DataLayout>(); // 20130325
            //IRBuilder<> FuncBuilder(F->getContext());
            //Builder = &FuncBuilder;

            instrumentInit( OutputDependenceFunction, I);
#endif

          } 

        }

        void LDDProfiling::visitAtomicRMWInst(AtomicRMWInst &I){
          // Instument an AtomicRMW instruction with a store check.
          Value *AccessSize = ConstantInt::get(SizeTy, TD->getTypeStoreSize(I.getType()));
          //instrument(I.getPointerOperand(), AccessSize, StoreCheckFunction, I);
          ++AtomicsInstrumented;
        }

        void LDDProfiling::visitAtomicCmpXchgInst(AtomicCmpXchgInst &I){
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
            Constant *AddrPos = ConstantDataArray::getString(*Context, StringRef(StrPos) ); 
            GAddrPos = new GlobalVariable(*CurModule, AddrPos->getType(), true, GlobalValue::ExternalLinkage, AddrPos, PosName); 
            Pos[StrPos] = GAddrPos; 
          }


          if( Name[Var.str()] ){
            GVarName = Name[Var.str()];
          }
          else{
            Constant *VarName = ConstantDataArray::getString(*Context, Var); 
            GVarName = new GlobalVariable(*CurModule, VarName->getType(), true, GlobalValue::ExternalLinkage, VarName, GlobalVarName); 
            Name[Var.str()] = GVarName;
          }

          instrument(I.getPointerOperand(), AccessSize, (Value*) GAddrPos, (Value*)GVarName, StoreCheckFunction, I);
          ++AtomicsInstrumented;
        }

        void LDDProfiling::visitMemIntrinsic(MemIntrinsic &MI){
          // Instrument llvm.mem[set|set|cpy|move].* calls with load/store checks.
          Builder->SetInsertPoint(&MI);
          Value *AccessSize = Builder->CreateIntCast(MI.getLength(), SizeTy, /*isSigned=*/false);

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
            Constant *AddrPos = ConstantDataArray::getString(*Context, StringRef(StrPos) ); 
            GAddrPos = new GlobalVariable(*CurModule, AddrPos->getType(), true, GlobalValue::ExternalLinkage, AddrPos, "GAddrPos"); 
            Pos[StrPos] = GAddrPos; 
          }
          if( Name[Var.str()] ){
            GVarName = Name[Var.str()];
          }
          else{
            Constant *VarName = ConstantDataArray::getString(*Context, Var); 
            GVarName = new GlobalVariable(*CurModule, VarName->getType(), true, GlobalValue::ExternalLinkage, VarName, "GVarName"); 
            Name[Var.str()] = GVarName;
          }
          // memcpy and memmove have a source memory area but memset doesn't
          if(MemTransferInst *MTI = dyn_cast<MemTransferInst>(&MI))
            instrument(MTI->getSource(), AccessSize, (Value*)GAddrPos, (Value*) GVarName, LoadCheckFunction, MI);
          instrument(MI.getDest(), AccessSize, (Value*)GAddrPos, (Value*)GvarName,  StoreCheckFunction, MI);
#endif
          ++IntrinsicsInstrumented;
        }


        void LDDProfiling::visitCallInst(CallInst &CI){
          //CI.dump();
          Function * Func = CI.getCalledFunction(); // bug?
          if ( Func == NULL )
            return;
          std::string  FuncName = Func->getName().str(); 
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
          LDDProfiling::getInstFileName(Instruction*I)
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

        unsigned int LDDProfiling::getInstLineNumber(Instruction*I)
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
          LDDProfiling::getInstFuncName(Instruction*I)
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
          LDDProfiling::getPointerName(Instruction &I)
          {

            return "xx";
          }
#endif





















