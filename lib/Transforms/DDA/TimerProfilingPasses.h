//===- CommonMemorySafetyPasses.h - Declare common memory safety passes ---===//
// 
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// This file declares the interfaces required to use the LDD profiling 
// passes while they are not part of LLVM.
//
//===----------------------------------------------------------------------===//

#ifndef LDD_PROFILING_PASSES_H 
#define LDD_PROFILING_PASSES_H

namespace llvm{
  class FunctionPass;
  class PassRegistry;

  // Insert Timer Profiling
  FunctionPass *createTimerProfilingPass();
  void initializeTimerProfilingPass(llvm::PassRegistry&);
}

#endif
