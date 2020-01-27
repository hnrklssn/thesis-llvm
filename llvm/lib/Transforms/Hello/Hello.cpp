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
#include "llvm/Analysis/InstructionSimplify.h"
using namespace llvm;

#define DEBUG_TYPE "source-name"

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
          LLVM_DEBUG(errs() << "found dbg value: \n");
          LLVM_DEBUG(DbgValue->dump());
          LLVM_DEBUG(DbgValue->getValue()->dump());
          return DbgValue->getVariable();
        }
      }
    }
    return NULL;
  }

  std::string getFragmentTypeName(DIType *T, int64_t Offset, DIType **FinalType, std::string Sep = ".") {
    if (auto Comp = dyn_cast<DICompositeType>(T)) {
      DIDerivedType *tmpT = nullptr;
      for (auto E : Comp->getElements()) {
        LLVM_DEBUG(E->dump());
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

  int pointerDepth(DIType *T) {
    int i = 0;
    while(T && T->getTag() == dwarf::DW_TAG_pointer_type) {
      i++;
      if(auto PT = dyn_cast<DIDerivedType>(T)) {
        T = PT->getBaseType();
      } else return i;
    }
    return i;
  }

  std::string getFragmentTypeName(DIType *T, int64_t *Offsets_begin, int64_t *Offsets_end, DIType **FinalType, std::string Sep = ".") {
    int64_t Offset = -1;
    LLVM_DEBUG(errs() << "getFragmentTypeName: " << *T << "\n");
    if (auto Derived = dyn_cast<DIDerivedType>(T)) {
      LLVM_DEBUG(errs() << "pointer depth: " << pointerDepth(Derived) << "\n");
      if(Derived->getBaseType()) {
        LLVM_DEBUG(errs() << "basetype: " << *Derived->getBaseType() << " " << Derived->getBaseType()->getName() << "\n");
        LLVM_DEBUG(errs() << "base pointer depth: " << pointerDepth(Derived->getBaseType()) << "\n");
        LLVM_DEBUG(Derived->getBaseType()->dump());
        LLVM_DEBUG(Derived->getBaseType()->printAsOperand(errs()));
      } else {
        LLVM_DEBUG(errs() << "no basetype\n");
      }
    }
    if (Offsets_begin == Offsets_end) {
      LLVM_DEBUG(errs() << "offsets_begin == offsets_end \n");
    } else {
      Offset = *Offsets_begin;
    }
    // derived non pointer types don't count, so we still haven't reached the final type if we encounter one of those
    if (Offsets_begin == Offsets_end  && (!isa<DIDerivedType>(T) || T->getTag() == dwarf::DW_TAG_pointer_type)) {
      if(FinalType) *FinalType = T;
      return "";
    }
    LLVM_DEBUG(errs() << "getFragmentTypeName offset: " << Offset << "\n");
    if (auto Comp = dyn_cast<DICompositeType>(T)) {
      auto elements = Comp->getElements();
      if (Offset >= elements.size()) {
        return "not-enough-elements-in-struct?";
      }
      if(auto NextT = dyn_cast<DIDerivedType>(elements[Offset])) {
        LLVM_DEBUG(errs() << "nextT: " << *NextT << "\n");
        return Sep + NextT->getName().str() + getFragmentTypeName(NextT, Offsets_begin + 1, Offsets_end, FinalType);
      } else {
        return "Non derived type as struct member? Should not happen";
      }
    } else if (auto Derived = dyn_cast<DIDerivedType>(T)) {
      if (FinalType) *FinalType = Derived;
      if (!Derived->getBaseType()) {
        if (Offsets_begin != Offsets_end)
          return "[" + std::to_string(Offset) + "]";
        return "";
      }
      if (Derived->getTag() == dwarf::DW_TAG_pointer_type) {
        if (Offset == 0)
          return "";
        return "[" + std::to_string(Offset) + "]";
      }
      // transparently step through derived type without iterating offset
      return getFragmentTypeName(Derived->getBaseType(), Offsets_begin, Offsets_end, FinalType, Sep);
    }
    return ""; // This is the end of the chain, the type name is no longer part of the variable name
  }

  // store uses as int64_t in a vector in reverse order
  bool getConstOffsets(Use *Offsets_begin, Use *Offsets_end, SmallVectorImpl<int64_t> &vec) {
    while (Offsets_begin < Offsets_end) {
      Value *Offset = Offsets_begin->get();
      if (auto ConstOffset = dyn_cast<Constant>(Offset)) {
        LLVM_DEBUG(errs() << "getConstOffsets: " << ConstOffset->getUniqueInteger().getSExtValue() << "\n");
        vec.push_back(ConstOffset->getUniqueInteger().getSExtValue());
      } else {
        errs() << "non const offsets? should not happen\n";
        vec.clear();
        return false;
      }
      Offsets_begin++;
    }
    return true;
  }

  std::string getNameFromDbgVariableIntrinsic(DbgVariableIntrinsic *VI, DIType **FinalType = nullptr) {
    DILocalVariable *Val = VI->getVariable();
    DIType *Type = Val->getType();
    DIExpression *Expr = VI->getExpression();
    if(!Expr->isFragment()) {
      if (FinalType) *FinalType = Type;
      return Val->getName();
    }
    int64_t Offset = -1;
    if (Expr->extractIfOffset(Offset)) { // FIXME extractIfOffset seems broken. Workaround below for now.
      if (FinalType) *FinalType = Type;
      return Val->getName();
    }

    Optional<DIExpression::FragmentInfo> FIO = Expr->getFragmentInfo();
    if(FIO) {
      Offset = FIO->OffsetInBits;
    }
    LLVM_DEBUG(errs() << "original offset " << Offset << " num " << Expr->getNumElements() << " " << FIO->SizeInBits << " " << FIO->OffsetInBits << "\n");
    if (auto Derived = dyn_cast<DIDerivedType>(Type)) { // FIXME this needs more thorough testing
      LLVM_DEBUG(errs() << "derived: " << Derived->getOffsetInBits() << "\n");
      LLVM_DEBUG(Derived->dump());
      Type = Derived->getBaseType();
      LLVM_DEBUG(errs() << "base:\n");
      if(!Type) return "typo lypo";
      LLVM_DEBUG(Type->dump());
    }

    if (auto Comp = dyn_cast<DICompositeType>(Type)) {
      LLVM_DEBUG(errs() << "composite:\n");
      LLVM_DEBUG(Comp->dump());
      return getFragmentTypeName(Comp, Offset, FinalType);
    }
    return "lol-hej";
  }
  void testFindDbgUsers(SmallVectorImpl<DbgVariableIntrinsic *> &DbgUsers,
                              Value *V) {
    // This function is hot. Check whether the value has any metadata to avoid a
    // DenseMap lookup.
    if (!V->isUsedByMetadata()) {
      return;
    }
    if (auto *L = ValueAsMetadata::getIfExists(V)) { // used to be LocalAsMetadata, but that will crash if passed a constant value
      if (auto *MDV = MetadataAsValue::getIfExists(V->getContext(), L)) {
        for (User *U : MDV->users()) {
          if (DbgVariableIntrinsic *DII = dyn_cast<DbgVariableIntrinsic>(U))
            DbgUsers.push_back(DII);
        }
      }
    }
 }

  DbgVariableIntrinsic* getSingleDbgUser(Value *V) { // TODO look into when a value can have multiple dbg intrinsic users
    SmallVector<DbgVariableIntrinsic *, 4> DbgValues;
    testFindDbgUsers(DbgValues, V); // TODO upstream fix
    //findDbgUsers(DbgValues, V);
    DbgVariableIntrinsic * DVI = nullptr;
    DbgValueInst *crash = nullptr;
    bool single = true;
    for (auto &VI : DbgValues) {
      LLVM_DEBUG(VI->dump());
      if (DVI && VI->isIdenticalTo(DVI)) continue; // intrinsics can be duplicated
      DVI = VI;
      assert(single && "more than one dbg intrinsic for value");
      if(!single) { return (DbgVariableIntrinsic*)crash->getVariable(); }
      single = false;
    }
    return DVI;
  }
  std::string getOriginalName(Value* V, DIType **FinalType = nullptr);

  // TODO check which string type is appropriate here
  std::string getOriginalRelativePointerName(Value *V, std::string &ArrayIdx, SmallVectorImpl<int64_t> &StructIndices, DIType **FinalType = nullptr) {
    // Use the value V to name this pointer prefix, and make it return metadata for accessing debug symbols for our pointer
    DIType *T = nullptr;
    LLVM_DEBUG(errs() << "getting pointer prefix for " << *V << "\n");
    std::string ptrName = getOriginalName(V, &T);
    LLVM_DEBUG(errs() << "prefix: " << ptrName << " arrayidx: " << ArrayIdx << "\n");
    if (!T) return ptrName + "->{unknownField}";
    LLVM_DEBUG(errs() << "final type: " << *T << " for " << *V << "\n");
    LLVM_DEBUG(errs() << "pointer depth: " << pointerDepth(T) << "\n");
    if (auto PT = dyn_cast<DIDerivedType>(T)) {
      std::string Sep = ".";
      if (ArrayIdx.empty() && PT->getTag() == dwarf::DW_TAG_pointer_type) Sep = "->"; // if explicit array indexing, dereference is already done
      return ptrName + ArrayIdx + getFragmentTypeName(PT->getBaseType(), StructIndices.begin(), StructIndices.end(), FinalType, Sep);
    }
    LLVM_DEBUG(errs() << "non derived type: " << *T << "\n");
    return ptrName + ArrayIdx + getFragmentTypeName(T, StructIndices.begin(), StructIndices.end(), FinalType);
  }

  /**
   * This function sets FinalType to the type of the value when dereferenced due to DIDerivedType pointer types
   * not always being available for subfields. If FinalType is needed, be aware that this is inconsistent with other similar functions.
   */
  std::string getOriginalPointerName(GetElementPtrInst *GEP, DIType **FinalType = nullptr) {
    LLVM_DEBUG(errs() << "GEP!\n");
    LLVM_DEBUG(GEP->dump());
    auto OP = GEP->getPointerOperand();
    LLVM_DEBUG(errs() << "operand: " << *OP << "\n");
    std::string ArrayIdx = "";
    auto FirstOffset = GEP->idx_begin()->get();
    LLVM_DEBUG(errs() << "first offset: " << *FirstOffset << "\n");
    if (!isa<Constant>(FirstOffset) || !cast<Constant>(FirstOffset)->isZeroValue()) {
      ArrayIdx = "[" + getOriginalName(FirstOffset) + "]";
    }
    LLVM_DEBUG(errs() << "arrayidx: " << ArrayIdx << "\n");
    if (GEP->getNumIndices() == 1) {
      std::string arrayName = getOriginalName(OP, FinalType);
      if (FinalType) {
        *FinalType = cast<DIDerivedType>(*FinalType)->getBaseType();
      } else {
        LLVM_DEBUG(errs() << "final type pointer missing for " << *GEP << "\n");
      }
      LLVM_DEBUG(errs() << "arrayName: " << arrayName << " arrayidx: " << ArrayIdx << "\n");
      LLVM_DEBUG(errs() << "num indices: " << GEP->getNumIndices() << "\n");
      for(auto &index : GEP->indices()) {
        LLVM_DEBUG(errs() << "index: " << *index << "\n");
      }
      return arrayName + ArrayIdx;
    }
    SmallVector<int64_t, 2> Indices;
    if (!getConstOffsets(GEP->idx_begin() + 1, GEP->idx_end(), Indices)) { // skip first offset, it doesn't matter if it's constant
      return "offset with index > 0 non const"; // TODO investigate if this ever happens
    }
    if (auto LI = dyn_cast<LoadInst>(OP)) {
      // This means our pointer originated in the dereference of another pointer
      return getOriginalRelativePointerName(LI->getPointerOperand(), ArrayIdx, Indices, FinalType);
    } else if (auto GEP2 = dyn_cast<GetElementPtrInst>(OP)) {
      // This means our pointer is some linear offset of another pointer, e.g. a subfield of a struct, relative to the pointer to the base of the struct
      return getOriginalRelativePointerName(GEP2, ArrayIdx, Indices, FinalType);
    }
    DbgVariableIntrinsic * DVI = getSingleDbgUser(OP);
    LLVM_DEBUG(errs() << DVI << "\n");
    if (!DVI) {
      // The code was compiled without debug info, or was optimised to the point where it's no longer accessible
      return "insert-some-fallback-here"; // FIXME handle gracefully (or return null potentially)
    }
    if(auto Val = dyn_cast<DbgValueInst>(DVI)) {
      assert("this should not happen");
    } else if(auto Decl = dyn_cast<DbgDeclareInst>(DVI)) {
      auto Var = Decl->getVariable();
      auto Type = Var->getType();
      LLVM_DEBUG(Type->dump());
      LLVM_DEBUG(Var->dump());
      return Var->getName().str() + ArrayIdx + getFragmentTypeName(Type, Indices.begin(), Indices.end(), FinalType);
    } else {
      return "unknown-dbg-variable-intrinsic";
    }
  }

  /**
   * Reconstruct the original name of a value from debug symbols. Output string is in C syntax no matter the source language.
   * If FinalType is given, it is set to point to the DIType of the value EXCEPT if the value is a GetElementPtrInst, where it will return the DIType of the value when read from memory.
   */
  std::string getOriginalName(Value* V, DIType **FinalType) {
    // TODO handle globals as well
    if (auto GEP = dyn_cast<GetElementPtrInst>(V)) {
      return getOriginalPointerName(GEP, FinalType);
    }
    DbgVariableIntrinsic * DVI = getSingleDbgUser(V);
    if (!DVI) {
      if (auto UI = dyn_cast<UnaryInstruction>(V)) { // most unary instructions don't really alter the value that much
        std::string name = getOriginalName(UI->getOperand(0), FinalType); // TODO Figure out if this could ever cause an infinite loop in welformed programs. My guess is no.
        if (isa<LoadInst>(UI) && !isa<GetElementPtrInst>(UI->getOperand(0))) {
          // GEPs already return dereferenced name and type, so skip this if op is a GEP
          // otherwise add dereference to name and type for loads
          if (FinalType && *FinalType) {
            *FinalType = cast<DIDerivedType>(*FinalType)->getBaseType();
          }
          return name + "[0]";
        }
        return name;
      }
      if (auto C = dyn_cast<ConstantInt>(V)) {
        return std::to_string(C->getZExtValue());
      }
      return "tmp-null";
    }
    return getNameFromDbgVariableIntrinsic(DVI, FinalType);
  }

  void printIntrinsic(IntrinsicInst *I) {
    if(DbgDeclareInst *DDI = dyn_cast<DbgDeclareInst>(I)) {
      LLVM_DEBUG(errs() << "address: " << *DDI->getAddress() << "\n");
      LLVM_DEBUG(errs() << "location: " << DDI->getVariableLocation() << "\n");
      LLVM_DEBUG(DDI->getVariable()->print(errs(), nullptr, true));
      LLVM_DEBUG(errs() << "\n");
      LLVM_DEBUG(DDI->getRawVariable()->print(errs(), nullptr, true));
      LLVM_DEBUG(errs() << "\n");
      LLVM_DEBUG(DDI->getExpression()->print(errs(), nullptr, true));
      LLVM_DEBUG(errs() << "\n");
      LLVM_DEBUG(DDI->getRawExpression()->print(errs(), nullptr, true));
      LLVM_DEBUG(errs() << "\n");
    } else if(DbgValueInst *DDI = dyn_cast<DbgValueInst >(I)) {
      LLVM_DEBUG(errs() << "DBGVALUE\n");
      //errs() << "address: " << *DDI->getAddress() << "\n";
      LLVM_DEBUG(errs() << "location: " << DDI->getVariableLocation() << "\n");
      LLVM_DEBUG(DDI->getVariable()->print(errs(), nullptr, true));
      LLVM_DEBUG(errs() << "\n");
      LLVM_DEBUG(DDI->getRawVariable()->print(errs(), nullptr, true));
      LLVM_DEBUG(errs() << "\n");
      LLVM_DEBUG(DDI->getExpression()->print(errs(), nullptr, true));
      LLVM_DEBUG(errs() << "\n");
      LLVM_DEBUG(DDI->getRawExpression()->print(errs(), nullptr, true));
      LLVM_DEBUG(errs() << "\n");
    } else {
      errs() << "UNKNOWN INTRINSIC\n";
      I->dump();
    }
    for (auto &U : I->operands()) {
      LLVM_DEBUG(errs() << "uselmao: ");
      LLVM_DEBUG(U->print(errs()));
      LLVM_DEBUG(errs() << "\n");
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

  struct SourceNameWrapper : public FunctionPass {
    static char ID; // Pass identification, replacement for typeid
    SourceNameWrapper() : FunctionPass(ID) {}

    bool runOnFunction(Function &F) override {
      LLVM_DEBUG(errs() << "Source Names: ");
      LLVM_DEBUG(errs().write_escaped(F.getName()) << '\n');
      for(auto &BB : F.getBasicBlockList()) {
        for(auto &I : BB.getInstList()) {
          LLVM_DEBUG(errs() << I << "\n");
          if (auto GEP = dyn_cast<GetElementPtrInst>(&I)) {
            errs() << I << " --> " << getOriginalPointerName(GEP) << "\n";
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

char SourceNameWrapper::ID = 0;
static RegisterPass<SourceNameWrapper> X("source-names", "Pass printing the C-style names for values, for test purposes");

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
