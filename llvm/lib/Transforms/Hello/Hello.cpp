//===- Hello.cpp - Example code from "Writing an LLVM Pass" ---------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements two versions of the LLVM "Hello World" pass described
// in docs/WritingAnLLVMPass.html
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Function.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/Transforms/Utils/Local.h"
using namespace llvm;

#define DEBUG_TYPE "hello"

STATISTIC(HelloCounter, "Counts number of functions greeted");

namespace {
  const Function* findEnclosingFunc(const Value* V) {
    if (const Argument* Arg = dyn_cast<Argument>(V)) {
      return Arg->getParent();
    }
    if (const Instruction* I = dyn_cast<Instruction>(V)) {
      return I->getParent()->getParent();
    }
    return NULL;
  }

  const MDNode* findVar(const Value* V, const Function* F) {
    for (const_inst_iterator Iter = inst_begin(F), End = inst_end(F); Iter != End; ++Iter) {
      const Instruction* I = &*Iter;
      if (const DbgDeclareInst* DbgDeclare = dyn_cast<DbgDeclareInst>(I)) {
        if (DbgDeclare->getAddress() == V) return DbgDeclare->getVariable();
      } else if (const DbgValueInst* DbgValue = dyn_cast<DbgValueInst>(I)) {
        if (DbgValue->getValue() == V) {
          errs() << "found dbg value: \n";
          DbgValue->dump();
          DbgValue->getValue()->dump();
          return DbgValue->getVariable();
        }
      }
    }
    return NULL;
  }
  std::string getFragmentTypeName(DIType *T, int64_t Offset, DIType **FinalType, std::string Sep = ".") {
    if (auto Comp = dyn_cast<DICompositeType>(T)) { // FIXME maybe while instead?
      DIDerivedType *tmpT = nullptr;
      for (auto E : Comp->getElements()) {
        E->dump();
        if(auto E2 = dyn_cast<DIDerivedType>(E)) {
          int64_t O2 = E2->getOffsetInBits();
          if (O2 > Offset) {
            break;
          }
          tmpT = E2;
        } else {
          errs() << "Non derived type as struct member? Should not happen\n";
        }
      }
      if (!tmpT)
        return "no-elements-in-struct?";
      return Sep + tmpT->getName().str() + getFragmentTypeName(tmpT, Offset - tmpT->getOffsetInBits(), FinalType);
    } else if (auto Derived = dyn_cast<DIDerivedType>(T)) {
      if (FinalType) *FinalType = Derived;
      if (!Derived->getBaseType()) return "[" + std::to_string(Offset) + "]";
      if (Derived->getTag() == dwarf::DW_TAG_pointer_type) {
        if (Offset == 0)
          return ""; // traversing pointer is not meaningful in this context
        return "[" + std::to_string(Offset) + "]"; // FIXME print index in terms of elements adjusted for size
      }
      return getFragmentTypeName(Derived->getBaseType(), Offset, FinalType);
    }
    return ""; // This is the end of the chain, the type name is no longer part of the variable name
  }

  std::string getNameFromDbgVariableIntrinsic(DbgVariableIntrinsic *VI) {
    DILocalVariable *Val = VI->getVariable();
    DIExpression *Expr = VI->getExpression();
    if(!Expr->isFragment()) {
      return Val->getName();
    }
    int64_t Offset = -1;
    if (Expr->extractIfOffset(Offset)) { // FIXME extractIfOffset seems broken. Workaround below for now.
      return Val->getName();
    }

    Optional<DIExpression::FragmentInfo> FIO = Expr->getFragmentInfo();
    if(FIO) {
      Offset = FIO->OffsetInBits;
    }
    errs() << "original offset " << Offset << " num " << Expr->getNumElements() << " " << FIO->SizeInBits << " " << FIO->OffsetInBits << "\n";
    DIType *Type = Val->getType();
    if (auto Derived = dyn_cast<DIDerivedType>(Type)) { // FIXME this needs more thorough testing
      errs() << "derived: " << Derived->getOffsetInBits() << "\n";
      Derived->dump();
      Type = Derived->getBaseType();
      errs() << "base:\n";
      if(!Type) return "typo lypo";
      Type->dump();
    }

    if (auto Comp = dyn_cast<DICompositeType>(Type)) {
      errs() << "composite:\n";
      Comp->dump();
      return getFragmentTypeName(Comp, Offset, nullptr);
    }
    return "lol-hej";
  }

  DbgVariableIntrinsic* getSingleDbgUser(Value *V) { // TODO look into when a value can have multiple dbg intrinsic users
    SmallVector<DbgVariableIntrinsic *, 4> DbgValues;
    findDbgUsers(DbgValues, V);
    DbgVariableIntrinsic * DVI = nullptr;
    DbgValueInst *crash = nullptr;
    bool single = true;
    for (auto &VI : DbgValues) {
      VI->dump();
      DVI = VI;
      assert(single && "more than one dbg intrinsic for value");
      if(!single) { return (DbgVariableIntrinsic*)crash->getVariable(); }
      single = false;
    }
    return DVI;
  }
  std::string getOriginalName(Value* V, DIType **FinalType = nullptr);

  std::string getOriginalPointerName(GetElementPtrInst *GEP, DIType **FinalType = nullptr) {
    errs() << "GEP!\n";
    GEP->dump();
    Function *F = GEP->getParent()->getParent();
    Module *M = F->getParent();
    const DataLayout &DL = M->getDataLayout();
    APInt ByteOffset(DL.getIndexSizeInBits(GEP->getPointerAddressSpace()), 0);
    APInt EIGHT(ByteOffset);
    EIGHT = 8;
    auto OP = GEP->getPointerOperand();
    errs() << "operand: " << *OP << "\n";
    if (GEP->accumulateConstantOffset(DL, ByteOffset)) {
      APInt BitOffset = ByteOffset * EIGHT;
      errs() << "offset: " << BitOffset  << "\n";
      if (auto LI = dyn_cast<LoadInst>(OP)) {
        // This means our pointer originated in the dereference of another pointer
        // Use that pointer to name this pointer, and make it return metadata for accessing debug symbols for our ptr
        DIType *T = nullptr;
        std::string ptrName = getOriginalName(LI->getPointerOperand(), &T);
        if (!T) return ptrName + "->{unknownField}"; // FIXME handle constant indices
        errs() << "final type: " << *T << "\n";
        if (auto PT = dyn_cast<DIDerivedType>(T)) {
          assert(PT->getTag() == dwarf::DW_TAG_pointer_type);
          return ptrName + getFragmentTypeName(PT->getBaseType(), BitOffset.getSExtValue(), FinalType, "->");
        }
        return ptrName + "->{invalid_pointer_type}";
      }
      DbgVariableIntrinsic * DVI = getSingleDbgUser(OP);
      errs() << DVI << "\n";
      if (!DVI) {
        // The code was compiled without debug info, or was optimised to the point where it's no longer accessible
        return "insert-some-fallback-here"; // FIXME handle gracefully (or return null potentially)
      }
      if(auto Val = dyn_cast<DbgValueInst>(DVI)) {
        assert("this should not happen");
      } else if(auto Decl = dyn_cast<DbgDeclareInst>(DVI)) {
        auto Var = Decl->getVariable();
        auto Type = Var->getType();
        Type->dump();
        Var->dump();
        return Var->getName().str() + getFragmentTypeName(Type, BitOffset.getSExtValue(), FinalType);
      } else {
        return "unknown-dbg-variable-intrinsic";
      }
    } else {
      std::string arrayName = getOriginalName(OP);
      auto indexOP = GEP->getOperand(1); // TODO investigate if non-const indexing ever has multiple index operands
                                         // multidimensional arrays are handled already since that is covered by
                                         // pointer from pointer-deref
      errs() << "index operand: " << *indexOP << "\n";
      std::string indexName = getOriginalName(indexOP);
      return arrayName + "["+indexName+"]";
    }
  }

  std::string getOriginalName(Value* V, DIType **FinalType) {
    // TODO handle globals as well
    if (auto GEP = dyn_cast<GetElementPtrInst>(V)) {
      return getOriginalPointerName(GEP, FinalType);
    }
    DbgVariableIntrinsic * DVI = getSingleDbgUser(V);
    if (!DVI) {
      if (auto UI = dyn_cast<UnaryInstruction>(V)) { // most unary instructions don't really alter the value that much
        return getOriginalName(UI->getOperand(0)); // TODO Figure out if this could ever cause an infinite loop in welformed programs. My guess is no.
      }
      if (auto C = dyn_cast<ConstantInt>(V)) {
        return std::to_string(C->getZExtValue());
      }
      return "tmp-null";
    }
    return getNameFromDbgVariableIntrinsic(DVI);
  }

  void printIntrinsic(IntrinsicInst *I) {
    if(DbgDeclareInst *DDI = dyn_cast<DbgDeclareInst>(I)) {
      errs() << "address: " << *DDI->getAddress() << "\n";
      errs() << "location: " << DDI->getVariableLocation() << "\n";
      DDI->getVariable()->print(errs(), nullptr, true);
      errs() << "\n";
      DDI->getRawVariable()->print(errs(), nullptr, true);
      errs() << "\n";
      DDI->getExpression()->print(errs(), nullptr, true);
      errs() << "\n";
      DDI->getRawExpression()->print(errs(), nullptr, true);
      errs() << "\n";
    } else if(DbgValueInst *DDI = dyn_cast<DbgValueInst >(I)) {
      errs() << "DBGVALUE\n";
      //errs() << "address: " << *DDI->getAddress() << "\n";
      errs() << "location: " << DDI->getVariableLocation() << "\n";
      DDI->getVariable()->print(errs(), nullptr, true);
      errs() << "\n";
      DDI->getRawVariable()->print(errs(), nullptr, true);
      errs() << "\n";
      DDI->getExpression()->print(errs(), nullptr, true);
      errs() << "\n";
      DDI->getRawExpression()->print(errs(), nullptr, true);
      errs() << "\n";
    } else {
      errs() << "UNKNOWN INTRINSIC\n";
      I->dump();
    }
    for (auto &U : I->operands()) {
      errs() << "uselmao: ";
      U->print(errs());
      errs() << "\n";
      if(IntrinsicInst *II = dyn_cast<IntrinsicInst>(U)) {
        printIntrinsic(II);
      }
    }

  }
  void printVariable(Value *I) {
    errs() << "print variable " << getOriginalName(I) << "\n";
  }

  void printAllMetadata(Instruction &I) {
    if(I.hasMetadata()) {
      SmallVector< std::pair< unsigned, MDNode *>, 8> MDs;
      I.getAllMetadata(MDs);
      for(auto &pair : MDs) {
        const auto MD = pair.second;
        MD->print(errs(), nullptr, true);
        errs() << "\n";
        MD->printAsOperand(errs());
        errs() << "\n";
      }
    }
    if(IntrinsicInst *II = dyn_cast<IntrinsicInst>(&I)) {
      printIntrinsic(II);
    }
    if(AllocaInst *AI = dyn_cast<AllocaInst>(&I)) {
      printVariable(AI);
    }

  }
  // Hello - The first implementation, without getAnalysisUsage.
  struct Hello : public FunctionPass {
    static char ID; // Pass identification, replacement for typeid
    Hello() : FunctionPass(ID) {}

    bool runOnFunction(Function &F) override {
      ++HelloCounter;
      errs() << "Hello: ";
      errs().write_escaped(F.getName()) << '\n';
      for(auto &BB : F.getBasicBlockList()) {
        for(auto &I : BB.getInstList()) {
          errs() << I << "\n";
          if (auto GEP = dyn_cast<GetElementPtrInst>(&I)) {
            errs() << getOriginalPointerName(GEP) << " plopp!\n";
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
}

char Hello::ID = 0;
static RegisterPass<Hello> X("hello", "Hello World Pass");

namespace {
  // Hello2 - The second implementation with getAnalysisUsage implemented.
  struct Hello2 : public FunctionPass {
    static char ID; // Pass identification, replacement for typeid
    Hello2() : FunctionPass(ID) {}

    bool runOnFunction(Function &F) override {
      ++HelloCounter;
      errs() << "Hello: ";
      errs().write_escaped(F.getName()) << '\n';
      return false;
    }

    // We don't modify the program, so we preserve all analyses.
    void getAnalysisUsage(AnalysisUsage &AU) const override {
      AU.setPreservesAll();
    }
  };
}

char Hello2::ID = 0;
static RegisterPass<Hello2>
Y("hello2", "Hello World Pass (with getAnalysisUsage implemented)");
