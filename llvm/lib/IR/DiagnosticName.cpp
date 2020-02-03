//===- llvm/Support/DiagnosticName.cpp - Diagnostic Value Names -*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines utility functions for provide human friendly names to improve diagnostics.
//
//===----------------------------------------------------------------------===//

#include "llvm/IR/DiagnosticName.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Use.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Instruction.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Support/Debug.h"
#include "llvm/ADT/SmallSet.h"
#include <deque>




#define DEBUG_TYPE "diagnostic-name"

using namespace llvm;

namespace {
  // forward declaration
  std::string getOriginalName(const Value* V, DIType **FinalType);
  void printAllMetadata(const Instruction &I) {
    errs() << "metadata\n";
    if(I.hasMetadata()) {
      SmallVector< std::pair< unsigned, MDNode *>, 8> MDs;
      I.getAllMetadata(MDs);
      for(auto &pair : MDs) {
        const auto MD = pair.second;
        MD->print(errs(), nullptr, true);
        errs() << "\n";
        MD->printAsOperand(errs());
        errs() << "\n";
        if (auto loc = dyn_cast<DILocation>(MD)) {
          errs() << "scope: " << *loc->getScope() << "\n";
        }
      }
    } else {
      errs() << "no metadata\n";
    }
    errs() << "end metadata\n";
    /*if(IntrinsicInst *II = dyn_cast<IntrinsicInst>(&I)) {
      printIntrinsic(II);
    }
    if(AllocaInst *AI = dyn_cast<AllocaInst>(&I)) {
      printVariable(AI);
      }*/

  }

  void printIntrinsic(IntrinsicInst *I) {
    if(DbgDeclareInst *DDI = dyn_cast<DbgDeclareInst>(I)) {
      LLVM_DEBUG(errs() << "address: " << *DDI->getAddress() << "\n");
      LLVM_DEBUG(errs() << "location: " << *DDI->getVariableLocation() << " endloc\n");
      LLVM_DEBUG(DDI->getVariable()->print(errs(), nullptr, true));
      LLVM_DEBUG(errs() << "\n");
      LLVM_DEBUG(printAllMetadata(*I));
      LLVM_DEBUG(DDI->getVariable()->getScope()->print(errs(), nullptr, true));
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
      LLVM_DEBUG(errs() << "location: " << *DDI->getVariableLocation() << " endloc\n");
      LLVM_DEBUG(DDI->getVariable()->print(errs(), nullptr, true));
      LLVM_DEBUG(errs() << "\n");
      LLVM_DEBUG(printAllMetadata(*I));
      LLVM_DEBUG(DDI->getVariable()->getScope()->print(errs(), nullptr, true));
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
        LLVM_DEBUG(errs() << "--next intrinsic--\n");
        printIntrinsic(II);
      }
    }

  }

  std::string getFragmentTypeName(DIType *T, int64_t Offset, DIType **FinalType, std::string Sep = ".") {
    if (!T) return "fragment-type-null";
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

  /// Get rest of name based on the type of the base value, and the offset.
  std::string getFragmentTypeName(DIType *T, int64_t *Offsets_begin, int64_t *Offsets_end, DIType **FinalType, std::string Sep = ".") {
    int64_t Offset = -1;
    if (!T) return "fragment-type-null";
    LLVM_DEBUG(errs() << "getFragmentTypeName: " << *T << "\n");
    if (auto Derived = dyn_cast<DIDerivedType>(T)) {
      if(Derived->getBaseType()) {
        LLVM_DEBUG(errs() << "basetype: " << *Derived->getBaseType() << " " << Derived->getBaseType()->getName() << "\n");
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

  /// Store const uses as int64_t in a vector.
  /// Returns pointer to first non-const use, or Offsets_end if all constant.
  const Use *getConstOffsets(const Use *Offsets_begin, const Use *Offsets_end, SmallVectorImpl<int64_t> &vec) {
    while (Offsets_begin < Offsets_end) {
      Value *Offset = Offsets_begin->get();
      if (auto ConstOffset = dyn_cast<Constant>(Offset)) {
        LLVM_DEBUG(errs() << "getConstOffsets: " << ConstOffset->getUniqueInteger().getSExtValue() << "\n");
        vec.push_back(ConstOffset->getUniqueInteger().getSExtValue());
      } else break;
      Offsets_begin++;
    }
    return Offsets_begin;
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
      if(!Type) return "missing-base-type";
      LLVM_DEBUG(Type->dump());
    }

    if (auto Comp = dyn_cast<DICompositeType>(Type)) {
      LLVM_DEBUG(errs() << "composite:\n");
      LLVM_DEBUG(Comp->dump());
      return getFragmentTypeName(Comp, Offset, FinalType);
    }
    return "unknown-debug-info-pattern";
  }

  /// Fixed version of findDbgUsers
  void testFindDbgUsers(SmallVectorImpl<DbgVariableIntrinsic *> &DbgUsers,
                              const Value *V) {
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

  /// A Value can have multiple debug non-identical debug intrinsics due to inlining
  /// and potentially also due to aliasing in general.
  /// TODO: Handle returning multiple aliasing names
  DbgVariableIntrinsic* getSingleDbgUser(const Value *V) {
    SmallVector<DbgVariableIntrinsic *, 4> DbgValues;
    testFindDbgUsers(DbgValues, V); // TODO upstream fix
    //findDbgUsers(DbgValues, V);
    DbgVariableIntrinsic * DVI = nullptr;
    for (auto &VI : DbgValues) {
      LLVM_DEBUG(VI->dump());
      if (DVI && VI->isIdenticalTo(DVI)) continue; // intrinsics can be duplicated
      LLVM_DEBUG(
      if(DVI) {
        if (auto I = dyn_cast<Instruction>(V)) {
          printAllMetadata(*I);
        } else errs() << "not instruction\n";
        errs() << "printing intrinsics\n";
        printIntrinsic(DVI);
        printIntrinsic(VI);
      });
      DVI = VI;
    }
    return DVI;
  }

  // TODO check which string type is appropriate here
  /// Helper function for naming a pointer relative to some other base pointer
  std::string getOriginalRelativePointerName(const Value *V, std::string &ArrayIdx, SmallVectorImpl<int64_t> &StructIndices, DIType **FinalType = nullptr) {
    // Use the value V to name this pointer prefix, and make it return metadata for accessing debug symbols for our pointer
    DIType *T = nullptr;
    LLVM_DEBUG(errs() << "getting pointer prefix for " << *V << "\n");
    std::string ptrName = getOriginalName(V, &T);
    LLVM_DEBUG(errs() << "prefix: " << ptrName << " arrayidx: " << ArrayIdx << "\n");
    if (!T) return ptrName + "->{unknownField}";
    if (auto PT = dyn_cast<DIDerivedType>(T)) {
      std::string Sep = ".";
      if (ArrayIdx.empty() && PT->getTag() == dwarf::DW_TAG_pointer_type) Sep = "->"; // if explicit array indexing, dereference is already done
      return ptrName + ArrayIdx + getFragmentTypeName(PT->getBaseType(), StructIndices.begin(), StructIndices.end(), FinalType, Sep);
    }
    return ptrName + ArrayIdx + getFragmentTypeName(T, StructIndices.begin(), StructIndices.end(), FinalType);
  }

  /// This function sets FinalType to the type of the value when dereferenced due to DIDerivedType pointer types
  /// not always being available for subfields. If FinalType is needed, be aware that this is inconsistent with other similar functions.
  std::string getOriginalPointerName(const GetElementPtrInst *GEP, DIType **FinalType = nullptr) {
    LLVM_DEBUG(errs() << "GEP!\n");
    LLVM_DEBUG(GEP->dump());
    auto OP = GEP->getPointerOperand();
    LLVM_DEBUG(errs() << "operand: " << *OP << "\n");
    std::string ArrayIdx = "";
    auto FirstOffset = GEP->idx_begin()->get();
    assert(FirstOffset != nullptr);
    LLVM_DEBUG(errs() << "first offset: " << *FirstOffset << "\n");
    if (!isa<Constant>(FirstOffset) || !cast<Constant>(FirstOffset)->isZeroValue()) {
      ArrayIdx = "[" + getOriginalName(FirstOffset) + "]";
    }
    LLVM_DEBUG(errs() << "arrayidx: " << ArrayIdx << "\n");
    if (GEP->getNumIndices() == 1) {
      std::string arrayName = getOriginalName(OP, FinalType);
      if (FinalType && *FinalType) {
        if (!isa<DIDerivedType>(*FinalType)) {
          errs() << "not derived type: " << **FinalType << "\n";
        } else *FinalType = cast<DIDerivedType>(*FinalType)->getBaseType();
      }
      return arrayName + ArrayIdx;
    }
    SmallVector<int64_t, 2> Indices;
    std::string name = "";
    const Use *idx_last_const = getConstOffsets(GEP->idx_begin() + 1, GEP->idx_end(), Indices); // skip first offset, it doesn't matter if it's constant
    DIType *T = nullptr;
    if (auto LI = dyn_cast<LoadInst>(OP)) {
      // This means our pointer originated in the dereference of another pointer
      name += getOriginalRelativePointerName(LI->getPointerOperand(), ArrayIdx, Indices, &T);
    } else if (auto GEP2 = dyn_cast<GetElementPtrInst>(OP)) {
      // This means our pointer is some linear offset of another pointer, e.g. a subfield of a struct, relative to the pointer to the base of the struct
      name += getOriginalRelativePointerName(GEP2, ArrayIdx, Indices, &T);
    } else {
      // Base pointer is just a variable, fetch its debug info
      DbgVariableIntrinsic * DVI = getSingleDbgUser(OP);
      LLVM_DEBUG(errs() << DVI << "\n");
      if (!DVI) {
        // The code was compiled without debug info, or was optimised to the point where it's no longer accessible
        return "insert-some-fallback-here"; // FIXME handle gracefully (or return null potentially)
      }

      if(auto Val = dyn_cast<DbgValueInst>(DVI)) {
        auto Var = Val->getVariable();
        auto Type = Var->getType();
        auto BaseType = cast<DIDerivedType>(Type)->getBaseType();
        std::string Sep = ".";
        if (ArrayIdx.empty()) Sep = "->";
        name += Var->getName().str() + ArrayIdx + getFragmentTypeName(BaseType, Indices.begin(), Indices.end(), &T, Sep);
      } else if(auto Decl = dyn_cast<DbgDeclareInst>(DVI)) {
        auto Var = Decl->getVariable();
        auto Type = Var->getType();
        name += Var->getName().str() + ArrayIdx + getFragmentTypeName(Type, Indices.begin(), Indices.end(), &T);
      } else {
        return "unknown-dbg-variable-intrinsic";
      }
    }

    // in case not all offsets (excluding the first one) were constant.
    // this can happen for example with arrays in structs
    while(idx_last_const < GEP->idx_end() && T) {
      name += "[" + getOriginalName(idx_last_const->get()) + "]";

      if (auto Derived = dyn_cast<DIDerivedType>(T)) T = Derived->getBaseType();
      else if (auto Composite = dyn_cast<DICompositeType>(T)) T = Composite->getBaseType();
      else {
        errs() << "unhandled ditype: " << *T << "\n";
        //assert(!"Unhandled DIType");
      }

      Indices.clear(); // collect potential remaining constant indices
      idx_last_const = getConstOffsets(idx_last_const + 1, GEP->idx_end(), Indices);

      DIType *T2 = nullptr; // avoid aliasing T
      name += getFragmentTypeName(T, Indices.begin(), Indices.end(), &T2);
      T = T2;
    }

    if (FinalType) *FinalType = T;
    return name;
  }

  /// If this value is part of an use-def chain with a value that has a dbg variable intrinsic, name this value after that value. Only traverses backwards.
  /// Drawback is that operations may be skipped. If V is %i.next = phi [0, i + 1], we will name it "i", not "i + 1".
  std::string getOriginalInductionVariableName(const Value *V, DIType **FinalType) {
    SmallSet<const Value *, 10> Visited;
    // want BFS under the assumption that the names of values fewer hops away are more likely to represent this variable well
    std::deque<const Value *> Queue;
    Queue.push_back(V);
    while(!Queue.empty()) {
      V = Queue.front();
      Queue.pop_front();
      Visited.insert(V);
      DbgVariableIntrinsic * DVI = getSingleDbgUser(V);
      if (DVI) return getOriginalName(V);

      if (auto U = dyn_cast<User>(V)) {
        for (auto &OP : U->operands()) {
          if (Visited.count(OP)) continue;
          Queue.push_back(OP);
        }
      }
    }
    return "";
  }

  std::string getOriginalPhiName(const PHINode *PHI, DIType **FinalType) {
    // assume induction variable structure, and name after first DVI found
    std::string name = getOriginalInductionVariableName(PHI, FinalType);
    LLVM_DEBUG(errs() << "indvar name: " << name << "\n");
    if (!name.empty()) return name;

    // Found no dbg variable intrinsic. Since we've explored recursively all the way,
    // we already know that the operands will not be of much use in determining original name.
    return "<unknown-phi %" + PHI->getName().str() + ">";
  }

  std::string getOriginalBinOpName(const BinaryOperator* BO, DIType **FinalType) {
    LLVM_DEBUG(errs() << "bopname: " << *BO << " name: " << BO->getName() << " opcode name: " << BO->getOpcodeName() << "\n");
    std::string name = "<unknown-binop>";
    switch (BO->getOpcode()) {
    case Instruction::Add: case Instruction::FAdd:
      name = "+";
      break;
    case Instruction::Sub: case Instruction::FSub:
      name = "-";
      break;
    case Instruction::Mul: case Instruction::FMul:
      name = "*";
      break;
    case Instruction::UDiv: case Instruction::SDiv:
      name = "/";
      break;
    case Instruction::URem: case Instruction::SRem: case Instruction::FRem:
      name = "%";
      break;
    case Instruction::Shl:
      name = "<<";
      break;
    case Instruction::LShr: case Instruction::AShr:
      name = ">>";
      break;
    case Instruction::And:
      name = "&";
      break;
    case Instruction::Or:
      name = "|";
      break;
    case Instruction::Xor:
      name = "^";
      break;
    default:
      break;
    }
    return getOriginalName(BO->getOperand(0)) + " " + name + " " + getOriginalName(BO->getOperand(1));
  }

   /// Reconstruct the original name of a value from debug symbols. Output string is in C syntax no matter the source language.
   /// If FinalType is given, it is set to point to the DIType of the value EXCEPT if the value is a GetElementPtrInst, where it will return the DIType of the value when dereferenced.
  std::string getOriginalName(const Value* V, DIType **FinalType) {
    assert(V != nullptr);
    LLVM_DEBUG(errs() << "gON: " << *V << "\n");

    DbgVariableIntrinsic * DVI = getSingleDbgUser(V);
    if (DVI) { // This is the gold standard, it will tell us the actual source name
      return getNameFromDbgVariableIntrinsic(DVI, FinalType);
    }
    LLVM_DEBUG(errs() << "gON: no DVI" << *V << "\n");

    if (auto GEP = dyn_cast<GetElementPtrInst>(V)) {
      return getOriginalPointerName(GEP, FinalType);
    }
    if (auto UI = dyn_cast<UnaryInstruction>(V)) { // most unary instructions don't really alter the value that much
      // so our default here is to just use the name of the operand.
      const Value *OP = UI->getOperand(0);
      std::string name = getOriginalName(OP, FinalType); // TODO Figure out if this could ever cause an infinite loop in welformed programs. My guess is no.
      if (isa<LoadInst>(UI) && !(isa<GetElementPtrInst>(OP) || isa<GlobalVariable>(OP))) {
        // GEPs already return dereferenced name and type, so skip this if operand is a GEP
        // otherwise add dereference to name and type for loads.
        // Same for GlobalVariables. In C syntax globals don't act like pointers, but they are in IR.
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
    if (auto GV = dyn_cast<GlobalValue>(V)) {
      return GV->getName();
    }
    if (auto PHI = dyn_cast<PHINode>(V)) {
      return getOriginalPhiName(PHI, FinalType);
    }
    if (auto BOp = dyn_cast<BinaryOperator>(V))
      return getOriginalBinOpName(BOp, FinalType);
    errs() << "unhandled value type: " << *V << "\n";
    return "";
  }
} // end anonymous namespace

/// Reconstruct the original name of a value from debug symbols. Output string is in C syntax no matter the source language. Will fail if not compiled with debug symbols.
/// TODO: Handle returning multiple aliasing names
std::string llvm::getOriginalName(const Value* V) {
  return ::getOriginalName(V, nullptr);
}

