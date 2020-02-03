//===- DiagnosticNameWrapper.cpp - Wrapper pass printing Value names from debug info ---------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This implements a pass for testing the output of getOriginalName.
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Function.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/DiagnosticName.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/Transforms/Utils/Local.h"
#include "llvm/Analysis/InstructionSimplify.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Passes/PassBuilder.h"

#include "llvm/Support/Debug.h"

using namespace llvm;

#define DEBUG_TYPE "diagnostic-name"

namespace {
  void printVariable(Value *I) {
    errs() << "print variable " << getOriginalName(I) << "\n";
  }

struct DiagnosticNameWrapper : public FunctionPass {
  static char ID; // Pass identification, replacement for typeid
  DiagnosticNameWrapper() : FunctionPass(ID) {}

  bool runOnFunction(Function &F) override {
    LLVM_DEBUG(errs() << "Source Names: ");
    LLVM_DEBUG(errs().write_escaped(F.getName()) << '\n');
    for(auto &BB : F.getBasicBlockList()) {
      for(auto &I : BB.getInstList()) {
        LLVM_DEBUG(errs() << I << "\n");
        if (auto GEP = dyn_cast<GetElementPtrInst>(&I)) {
          errs() << I << " --> " << getOriginalName(GEP) << "\n";
        }
      }
    }
    errs() << "---args section--\n";
    for (auto& A : F.args()) {
      printVariable(&A);
    }
    return false;
  }
};
} // end anonymous namespace

char DiagnosticNameWrapper::ID = 0;
static RegisterPass<DiagnosticNameWrapper> X("diagnostic-names", "Pass printing the C-style names for values, for testing DiagnosticName.cpp");
