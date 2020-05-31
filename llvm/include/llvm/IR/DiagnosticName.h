
//===- llvm/IR/DiagnosticName.h - Diagnostic Value Names --------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file declares a utility function for constructing a human friendly name
// for LLVM Values.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_IR_DIAGNOSTICNAME_H
#define LLVM_IR_DIAGNOSTICNAME_H

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/IR/DIBuilder.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/Module.h"
#include <bits/stdint-intn.h>
#include <string>

namespace llvm {

enum TypeCompareResult {
 NoMatch = 0, IncompleteTypeMatch, Match };
class DiagnosticNameGenerator {
public:
  /// Reconstruct the original name of a value from debug symbols.
  /// Output string is in C syntax no matter the source language.
  /// Will fail if input is not compiled with debug symbols (-g with Clang).
  std::string getOriginalName(const Value *V);
  DiagnosticNameGenerator(Module *M, DIBuilder *B);
  static DiagnosticNameGenerator create(Module *M);

private:
  Module *M;
  DIBuilder *Builder;
  DebugInfoFinder DIF;
  /// Used to prevent infinite recursion.
  SmallSet<const PHINode *, 4> CurrentPhis;
  /// Fast lookup of DITypes referencing a given DIType
  DenseMap<DIType*, SmallVector<DIType*, 4>*> DITypeUsers;
  DenseMap<DIType *, SmallVector<DIType *, 4> *>* getDITypeUsers();
  DIDerivedType *createPointerType(DIType *BaseTy);
  TypeCompareResult compareValueTypeAndDebugType(const Type *Ty, const DIType *DITy);
  std::string getFragmentTypeName(DIType *T, int64_t Offset, DIType **FinalType,
                                  std::string Sep = ".");
  std::string getFragmentTypeName(DIType *T, const int64_t *Offsets_begin,
                                  const int64_t *Offsets_end,
                                  const Type *ValueTy, DIType **FinalType,
                                  std::string Sep = ".");
  std::pair<TypeCompareResult, uint32_t>
  isPointerChainToType(const PointerType *Ty, DIType *DITy);
  std::pair<TypeCompareResult, DIType*> calibrateDebugType(const Type *Ty, DIType *DITy);
  std::string getNameFromDbgVariableIntrinsic(const DbgVariableIntrinsic *VI,
                                              DIType **const FinalType);
  std::string tryGetNameFromDbgValue(const Value *V, DIType **FinalType);
  std::string
  getOriginalRelativePointerName(const Value *V, StringRef ArrayIdx,
                                 SmallVectorImpl<int64_t> &StructIndices,
                                 DIType **FinalType);
  std::string getFragmentNameNoDbg(const Type *Ty, const int64_t *idx_begin,
                                   const int64_t *idx_end);
  std::string getFragmentNameNoDbg(const Type *Ty, const Use *idx_begin,
                                   const Use *idx_end);
  std::string getOriginalPointerName(const GetElementPtrInst *const GEP,
                                     DIType **const FinalType);
  std::pair<std::string, int32_t>
  getOriginalInductionVariableName(const User *V, DIType **FinalType);
  std::string getOriginalStoreName(const StoreInst *ST,
                                   DIType **FinalType);
  std::string getOriginalCallName(const CallBase *Call,
                                  DIType **FinalType);
  std::string getOriginalSwitchName(const SwitchInst *Switch,
                                    DIType **FinalType);
  std::string getOriginalCmpName(const CmpInst *Cmp, DIType **FinalType);
  std::string getOriginalSelectName(const SelectInst *Select,
                                    DIType **FinalType);
  std::string getOriginalPhiName(const PHINode *PHI, DIType **FinalType);
  std::string getOriginalAsmName(const InlineAsm *Asm,
                                 DIType **FinalType);
  std::string getOriginalReturnName(const ReturnInst *Return,
                                    DIType **FinalType);
  std::string getOriginalBranchName(const BranchInst *Br,
                                    DIType **FinalType);
  std::string getOriginalBinOpName(const BinaryOperator *BO,
                                   DIType **FinalType);
  std::string getOriginalInstructionName(const Instruction *const I,
                                         DIType **const FinalType);
  std::string getOriginalConstantName(const Constant *C,
                                      DIType **const FinalType);
  std::string getOriginalNameImpl(const Value *V,
                                  DIType **const FinalType);
  std::string getOriginalBitCastName(const BitCastInst *BC, DIType **const FinalType);
  std::string getOriginalCastName(const CastInst *Cast,
                                  DIType **const FinalType);
};
} // namespace llvm
#endif // LLVM_IR_DIAGNOSTICNAME_H
