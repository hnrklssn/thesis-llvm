//===- OptimizationRemarkEmitter.cpp - Optimization Diagnostic --*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Optimization diagnostic interfaces.  It's packaged as an analysis pass so
// that by using this service passes become dependent on BFI as well.  BFI is
// used to compute the "hotness" of the diagnostic message.
//===----------------------------------------------------------------------===//

#include "llvm/Analysis/OptimizationRemarkEmitter.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/Analysis/BranchProbabilityInfo.h"
#include "llvm/Analysis/LazyBlockFrequencyInfo.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/DiagnosticInfo.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Metadata.h"
#include "llvm/InitializePasses.h"
#include <memory>

using namespace llvm;

OptimizationRemarkEmitter::OptimizationRemarkEmitter(const Function *F)
  : F(F), LI(nullptr), BFI(nullptr) {

  if (!F->getContext().getDiagnosticsHotnessRequested())
    return;

  // First create a dominator tree.
  DominatorTree DT;
  DT.recalculate(*const_cast<Function *>(F));

  // Generate LoopInfo from it.
  OwnedLI = std::make_unique<LoopInfo>(DT);
  LI = OwnedLI.get();
  LI->analyze(DT);

  // Then compute BranchProbabilityInfo.
  BranchProbabilityInfo BPI;
  BPI.calculate(*F, *LI);

  // Finally compute BFI.
  OwnedBFI = std::make_unique<BlockFrequencyInfo>(*F, BPI, *LI);
  BFI = OwnedBFI.get();
}

bool OptimizationRemarkEmitter::invalidate(
    Function &F, const PreservedAnalyses &PA,
    FunctionAnalysisManager::Invalidator &Inv) {
  bool LIValid = LI && Inv.invalidate<LoopAnalysis>(F, PA);
  // This analysis has no state and so can be trivially preserved but it needs
  // a fresh view of BFI if it was constructed with one.
  if (BFI && Inv.invalidate<BlockFrequencyAnalysis>(F, PA) && LIValid)
    return true;

  // Otherwise this analysis result remains valid.
  return false;
}

Optional<uint64_t> OptimizationRemarkEmitter::computeHotness(const Value *V) {
  if (!BFI)
    return None;

  return BFI->getBlockProfileCount(cast<BasicBlock>(V));
}

void OptimizationRemarkEmitter::computeHotness(
    DiagnosticInfoIROptimization &OptDiag) {
  const Value *V = OptDiag.getCodeRegion();
  if (V)
    OptDiag.setHotness(computeHotness(V));
}

MDNode *OptimizationRemarkEmitter::computeLoopID(const Value *V) const {
  if (!LI)
    errs() << __FUNCTION__ << " LI null\n";
  return nullptr;
  if (!V) {
    errs() << __FUNCTION__ << " BB null\n";
    return nullptr;
  }
  if (LI->empty()) {
    errs() << __FUNCTION__ << " LI empty\n";
    return nullptr;
  }

  Loop *L = LI->getLoopFor(cast<BasicBlock>(V));
  if (!L)
    return nullptr;
  return L->getLoopID();
}

void OptimizationRemarkEmitter::computeLoopID(
    DiagnosticInfoIROptimization &OptDiag) {
  const Value *V = OptDiag.getCodeRegion();
  if (V)
    OptDiag.setLoopID(computeLoopID(V));
}

static void addRemarkMetadata(SmallVectorImpl<MDNode*> &MDs, MDNode *N) {
  for (auto &MDOp : N->operands()) {
    if (auto MDN = dyn_cast<MDNode>(MDOp.get())) {
      if (MDN->getNumOperands() == 0) continue;
      if (auto MDS = dyn_cast<MDString>(MDN->getOperand(0).get())) {
        bool IsRemark = StringSwitch<bool>(MDS->getString())
          .Cases("remark", "remark_missed", "remark_analysis", true)
          .Default(false);
        if (!IsRemark) continue;
        MDs.push_back(MDN);
      }
    }
  }
}

void OptimizationRemarkEmitter::getAllRemarkMetadata(SmallVectorImpl<MDNode*> &MDs) const {
  MDNode *N;
  N = F->getMetadata("llvm.remarks");
  if (N) {
    addRemarkMetadata(MDs, N);
  }
  for (auto &BB : *F) {
    N = computeLoopID(&BB);
    if (!N) continue;
    addRemarkMetadata(MDs, N);
  }
  // const Module *M = F->getParent(); TODO: file level metadata
}

bool OptimizationRemarkEmitter::isAnyRemarkEnabledByMetadata() const {
  SmallVector<MDNode *, 1> MDs;
  getAllRemarkMetadata(MDs);
  return !MDs.empty();
}

bool OptimizationRemarkEmitter::isAnyRemarkEnabledByMetadata(StringRef PassName) const {
  SmallVector<MDNode*, 1> MDs;
  getAllRemarkMetadata(MDs);
  for (auto MD : MDs) {
    for (auto &MDOp : MD->operands()) {
      if (auto MDS = dyn_cast<MDString>(MDOp.get())) {
        if (MDS->getString().equals(PassName)) return true;
      }
    }
  }
  return false;
}

void OptimizationRemarkEmitter::emit(
    DiagnosticInfoOptimizationBase &OptDiagBase) {
  auto &OptDiag = cast<DiagnosticInfoIROptimization>(OptDiagBase);
  computeHotness(OptDiag);
  computeLoopID(OptDiag);

  // Only emit it if its hotness meets the threshold.
  if (OptDiag.getHotness().getValueOr(0) <
      F->getContext().getDiagnosticsHotnessThreshold()) {
    return;
  }

  F->getContext().diagnose(OptDiag);
}

OptimizationRemarkEmitterWrapperPass::OptimizationRemarkEmitterWrapperPass()
    : FunctionPass(ID) {
  initializeOptimizationRemarkEmitterWrapperPassPass(
      *PassRegistry::getPassRegistry());
}

bool OptimizationRemarkEmitterWrapperPass::runOnFunction(Function &Fn) {
  BlockFrequencyInfo *BFI;
  LoopInfo &LI = getAnalysis<LoopInfoWrapperPass>().getLoopInfo();

  if (Fn.getContext().getDiagnosticsHotnessRequested())
    BFI = &getAnalysis<LazyBlockFrequencyInfoPass>().getBFI();
  else
    BFI = nullptr;

  ORE = std::make_unique<OptimizationRemarkEmitter>(&Fn, &LI, BFI);
  return false;
}

void OptimizationRemarkEmitterWrapperPass::getAnalysisUsage(
    AnalysisUsage &AU) const {
  LazyBlockFrequencyInfoPass::getLazyBFIAnalysisUsage(AU);
  AU.setPreservesAll();
}

AnalysisKey OptimizationRemarkEmitterAnalysis::Key;

OptimizationRemarkEmitter
OptimizationRemarkEmitterAnalysis::run(Function &F,
                                       FunctionAnalysisManager &AM) {
  LoopInfo *LI = &AM.getResult<LoopAnalysis>(F);
  BlockFrequencyInfo *BFI;

  if (F.getContext().getDiagnosticsHotnessRequested())
    BFI = &AM.getResult<BlockFrequencyAnalysis>(F);
  else
    BFI = nullptr;

  return OptimizationRemarkEmitter(&F, LI, BFI);
}

char OptimizationRemarkEmitterWrapperPass::ID = 0;
static const char ore_name[] = "Optimization Remark Emitter";
#define ORE_NAME "opt-remark-emitter"

INITIALIZE_PASS_BEGIN(OptimizationRemarkEmitterWrapperPass, ORE_NAME, ore_name,
                      false, true)
INITIALIZE_PASS_DEPENDENCY(LazyBFIPass)
INITIALIZE_PASS_END(OptimizationRemarkEmitterWrapperPass, ORE_NAME, ore_name,
                    false, true)
