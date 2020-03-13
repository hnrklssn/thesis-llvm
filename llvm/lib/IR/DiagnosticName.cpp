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
#include "llvm/IR/Argument.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/GlobalValue.h"
#include "llvm/IR/InstrTypes.h"
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
#include "llvm/Support/ErrorHandling.h"
#include <deque>
#include <string>




#define DEBUG_TYPE "diagnostic-name"

using namespace llvm;

namespace {
  // forward declaration
  std::string getOriginalName(const Value* V, const DIType **FinalType);

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

  void printIntrinsic(const IntrinsicInst *I) {
    if(const DbgDeclareInst *DDI = dyn_cast<DbgDeclareInst>(I)) {
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
    } else if(const DbgValueInst *DDI = dyn_cast<DbgValueInst >(I)) {
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
      if(const IntrinsicInst *II = dyn_cast<IntrinsicInst>(U)) {
        LLVM_DEBUG(errs() << "--next intrinsic--\n");
        printIntrinsic(II);
      }
    }

  }

  std::string getFragmentTypeName(const DIType *const T, int64_t Offset,
                                  const DIType **const FinalType,
                                  std::string Sep = ".") {
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
          errs() << "T: " << *T << "\n";
          errs() << "E: " << *E << "\n";
          llvm_unreachable("Non derived type as struct member");
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
  std::string getFragmentTypeName(const DIType *T, const int64_t *Offsets_begin, const int64_t *Offsets_end, const DIType **FinalType, std::string Sep = ".") {
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

  std::string getNameFromDbgVariableIntrinsic(const DbgVariableIntrinsic *VI, const DIType **const FinalType) {
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
          if (auto GV = dyn_cast<GlobalValue>(MDV)) {
            LLVM_DEBUG(errs() << __FUNCTION__ << " GV: " << *GV << "\n");
            LLVM_DEBUG(errs() << __FUNCTION__ << " U: " << *U << "\n");
          }
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
  std::string getOriginalRelativePointerName(const Value *V, std::string &ArrayIdx, SmallVectorImpl<int64_t> &StructIndices, const DIType **FinalType) {
    // Use the value V to name this pointer prefix, and make it return metadata for accessing debug symbols for our pointer
    const DIType *T = nullptr;
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
  std::string getOriginalPointerName(const GetElementPtrInst *const GEP, const DIType **const FinalType) {
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
    const DIType *T = nullptr;
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

      const DIType *T2 = nullptr; // avoid aliasing T
      name += getFragmentTypeName(T, Indices.begin(), Indices.end(), &T2);
      T = T2;
    }

    if (FinalType) *FinalType = T;
    return name;
  }

  /// If this value is part of an use-def chain with a value that has a dbg variable intrinsic, name this value after that value. Only traverses backwards.
  /// Drawback is that operations may be skipped. If V is %i.next = phi [0, i + 1], we will name it "i", not "i + 1".
  std::string getOriginalInductionVariableName(const Value *V, const DIType **FinalType) {
    SmallSet<const Value *, 10> Visited;
    // want BFS under the assumption that the names of values fewer hops away are more likely to represent this variable well
    std::deque<const Value *> Queue;
    Queue.push_back(V);
    while(!Queue.empty()) {
      V = Queue.front();
      Queue.pop_front();
      Visited.insert(V);
      const DbgVariableIntrinsic * DVI = getSingleDbgUser(V);
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

  std::string getOriginalStoreName(const StoreInst *ST, const DIType **FinalType) {
    std::string PtrName = getOriginalName(ST->getPointerOperand(), nullptr);
    std::string ValueName = getOriginalName(ST->getValueOperand(), FinalType);
    return "*(" + PtrName + ") = " + ValueName;
  }

  std::string getOriginalCallName(const CallBase *Call, const DIType **FinalType) {
    std::string FuncName = Call->getName();
    std::string Name = FuncName + "(";
    User::const_op_iterator end;
    User::const_op_iterator it;
    for (it = Call->arg_begin(), end = Call->arg_end(); it != end; it++) {
      Name += getOriginalName(it->get());
      if (end - it > 1)
        Name += ", ";
    }
    Name += ")";
    return Name;
  }

  std::string getOriginalSwitchName(const SwitchInst *Switch, const DIType **FinalType) {
    std::string CondName = getOriginalName(Switch->getCondition());
    std::string Name = "switch (" + CondName + ") {\n";
    User::const_op_iterator end;
    User::const_op_iterator it;
    for (auto Case : Switch->cases()) {
      Name += "case " + getOriginalName(Case.getCaseValue()) + ":\n";
      Name += getOriginalName(Case.getCaseSuccessor());
    }
    Name += "}";
    return Name;
  }

  std::string getOriginalCmpName(const CmpInst *Cmp, const DIType **FinalType) {
    std::string name = "<unknown-cmp>";
    std::string Op1 = getOriginalName(Cmp->getOperand(0));
    std::string Op2 = getOriginalName(Cmp->getOperand(1));
    switch (Cmp->getPredicate()) {
    case CmpInst::FCMP_OEQ:
    case CmpInst::FCMP_UEQ:
    case CmpInst::ICMP_EQ:
      name = "==";
      break;
    case CmpInst::FCMP_OGT:
    case CmpInst::FCMP_UGT:
    case CmpInst::ICMP_UGT:
    case CmpInst::ICMP_SGT:
      name = ">";
      break;
    case CmpInst::FCMP_OGE:
    case CmpInst::FCMP_UGE:
    case CmpInst::ICMP_UGE:
    case CmpInst::ICMP_SGE:
      name = ">=";
      break;
    case CmpInst::FCMP_OLT:
    case CmpInst::FCMP_ULT:
    case CmpInst::ICMP_ULT:
    case CmpInst::ICMP_SLT:
      name = "<";
      break;
    case CmpInst::FCMP_OLE:
    case CmpInst::FCMP_ULE:
    case CmpInst::ICMP_ULE:
    case CmpInst::ICMP_SLE:
      name = "<=";
      break;
    case CmpInst::FCMP_ONE:
    case CmpInst::FCMP_UNE:
    case CmpInst::ICMP_NE:
      name = "!=";
      break;
    case CmpInst::FCMP_FALSE:
      return "FALSE";
    case CmpInst::FCMP_TRUE:
      return "TRUE";
    case CmpInst::BAD_FCMP_PREDICATE:
    case CmpInst::BAD_ICMP_PREDICATE:
      llvm_unreachable("invalid cmp inst");
      break;
    case CmpInst::FCMP_UNO:
      return "isnan(" + Op1 + ") | isnan(" + Op2 + ")";
    case CmpInst::FCMP_ORD:
      return "!(isnan(" + Op1 + ") | isnan(" + Op2 + "))";
    }
    return Op1 + " " + name + " " + Op2;
  }

  std::string getOriginalSelectName(const SelectInst *Select, const DIType **FinalType) {
    return getOriginalName(Select->getCondition())
      + " ? " + getOriginalName(Select->getTrueValue())
      + " : " + getOriginalName(Select->getFalseValue());
  }

  std::string getOriginalPhiName(const PHINode *PHI, const DIType **FinalType) {
    // assume induction variable structure, and name after first DVI found
    std::string name = getOriginalInductionVariableName(PHI, FinalType);
    LLVM_DEBUG(errs() << "indvar name: " << name << "\n");
    if (!name.empty()) return name;

    // Found no dbg variable intrinsic. Since we've explored recursively all the way,
    // we already know that the operands will not be of much use in determining original name.
    return "<unknown-phi %" + PHI->getName().str() + ">";
  }

  std::string getOriginalBranchName(const BranchInst *Br, const DIType **FinalType) {
    if (Br->isUnconditional()) {
      return "goto " + Br->getSuccessor(0)->getName().str();
    }
    return "if (" + getOriginalName(Br->getCondition()) + ") goto " + Br->getSuccessor(0)->getName().str()
      + "; else goto " + Br->getSuccessor(1)->getName().str() + ";";
  }

  std::string getOriginalBinOpName(const BinaryOperator* BO, const DIType **FinalType) {
    LLVM_DEBUG(errs() << "bopname: " << *BO << " name: " << BO->getName() << " opcode name: " << BO->getOpcodeName() << "\n");
    std::string Name = "<unknown-binop>";
    switch (BO->getOpcode()) {
    case Instruction::Add: case Instruction::FAdd:
      Name = "+";
      break;
    case Instruction::Sub: case Instruction::FSub:
      Name = "-";
      break;
    case Instruction::Mul: case Instruction::FMul:
      Name = "*";
      break;
    case Instruction::UDiv: case Instruction::SDiv:
      Name = "/";
      break;
    case Instruction::URem: case Instruction::SRem: case Instruction::FRem:
      Name = "%";
      break;
    case Instruction::Shl:
      Name = "<<";
      break;
    case Instruction::LShr: case Instruction::AShr:
      Name = ">>";
      break;
    case Instruction::And:
      Name = "&";
      break;
    case Instruction::Or:
      Name = "|";
      break;
    case Instruction::Xor:
      Name = "^";
      break;
    default:
      break;
    }
    return getOriginalName(BO->getOperand(0)) + " " + Name + " " + getOriginalName(BO->getOperand(1));
  }

  std::string getOriginalInstructionName(const Instruction *const I,
                                         const DIType **const FinalType) {
    if (auto GEP = dyn_cast<GetElementPtrInst>(I)) {
      return getOriginalPointerName(GEP, FinalType);
    }
    if (auto UI = dyn_cast<UnaryInstruction>(I)) {
      // most unary instructions don't  really alter the value that much
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
    if (auto PHI = dyn_cast<PHINode>(I)) {
      return getOriginalPhiName(PHI, FinalType);
    }
    if (auto BOp = dyn_cast<BinaryOperator>(I))
      return getOriginalBinOpName(BOp, FinalType);
    if (auto ST = dyn_cast<StoreInst>(I))
      return getOriginalStoreName(ST, FinalType);
    if (auto Call = dyn_cast<CallBase>(I))
      return getOriginalCallName(Call, FinalType);
    if (auto Cmp = dyn_cast<CmpInst>(I))
      return getOriginalCmpName(Cmp, FinalType);
    if (auto Switch = dyn_cast<SwitchInst>(I)) return getOriginalSwitchName(Switch, FinalType);
    if (auto Select = dyn_cast<SelectInst>(I))
      return getOriginalSelectName(Select, FinalType);
    if (auto Br = dyn_cast<BranchInst>(I)) {
      return getOriginalBranchName(Br, FinalType);
    };
    errs() << "unhandled instruction type: " << *I << "\n";
    return "";
  }

  std::string getOriginalConstantName(const Constant *C,
                                      const DIType **const FinalType) {
    if (auto Addr = dyn_cast<BlockAddress>(C))
      return "BB-addr: " + Addr->getName().str();
    if (auto Aggr = dyn_cast<ConstantAggregate>(C)) {
      unsigned NumElems = Aggr->getNumOperands();
      std::string Name;
      for (unsigned i = 0; i < NumElems; i++) {
        Name += getOriginalConstantName(Aggr->getOperand(i), nullptr);
        if (NumElems - i > 1)
          Name += ", ";
      }
      return "{" + Name + "}";
    }
    if (auto CAZ = dyn_cast<ConstantAggregateZero>(C)) {
      return "CAZ"; // TODO: implement @henrik
    }
    if (auto CDS = dyn_cast<ConstantDataSequential>(C)) {
      if (CDS->isString()) {
        LLVM_DEBUG(errs() << __FUNCTION__ << " constant string: " << *C << " "
                          << CDS->getAsCString() << "\n");
        return CDS->getAsString();
      }
      std::string Name;
      unsigned NumElems = CDS->getNumElements();
      for (unsigned i = 0; i < NumElems; i++) {
        Name += getOriginalConstantName(CDS->getElementAsConstant(i), nullptr);
        if (NumElems - i > 1)
          Name += ", ";
      }
      if (CDS->getType()->isVectorTy())
        return "Vec{" + Name + "}";
      return "{" + Name + "}";
    }
    if (auto CFp = dyn_cast<ConstantFP>(C)) {
      return std::to_string(CFp->getValueAPF().convertToDouble());
    }
    if (auto CI = dyn_cast<ConstantInt>(C)) {
      return std::to_string(CI->getZExtValue());
    }
    if (isa<ConstantPointerNull>(C)) {
      return "null";
    }
    if (isa<ConstantTokenNone>(C)) {
      return "none";
    }
    if (isa<UndefValue>(C)) {
      return "undefined";
    }
    if (auto CExpr = dyn_cast<ConstantExpr>(C)) {
      Instruction *I = CExpr->getAsInstruction();
      std::string ConstExprStr = getOriginalInstructionName(I, FinalType);
      I->deleteValue();
      return ConstExprStr;
    }
    if (auto GV = dyn_cast<GlobalValue>(C)) {
      return GV->getName();
    }
    errs() << "unhandled constant type: " << *C << "\n";
    return "";
  }

  /// Reconstruct the original name of a value from debug symbols. Output string
  /// is in C syntax no matter the source language. If FinalType is given, it is
  /// set to point to the DIType of the value EXCEPT if the value is a
  /// GetElementPtrInst, where it will return the DIType of the value when
  /// dereferenced.
  std::string getOriginalName(const Value* V, const DIType **const FinalType) {
    assert(V != nullptr);
    LLVM_DEBUG(errs() << "gON: " << *V << "\n");

    DbgVariableIntrinsic * DVI = getSingleDbgUser(V);
    if (DVI) { // This is the gold standard, it will tell us the actual source name
      return getNameFromDbgVariableIntrinsic(DVI, FinalType);
    }
    LLVM_DEBUG(errs() << "gON: no DVI" << *V << "\n");

    if (auto I = dyn_cast<Instruction>(V))
      return getOriginalInstructionName(I, FinalType);
    if (auto C = dyn_cast<Constant>(V))
      return getOriginalConstantName(C, FinalType);
    if (auto BB = dyn_cast<BasicBlock>(V)) {
      std::string Name = "BB{\n";
      Name += BB->getName();
      Name += ":\n";
      for (auto &I : *BB) {
        if (isa<DbgInfoIntrinsic>(I)) continue; // these intrinsics are essentially metadata, not code
        Name += getOriginalInstructionName(&I, nullptr);
        Name += "\n";
      }
      Name += "}";
      return Name;
    }
    if (auto Arg = dyn_cast<Argument>(V)) {
      return Arg->getName();
    }
    errs() << "unhandled value type: " << *V << "\n";
    return "";
  }
} // end anonymous namespace

/// Reconstruct the original name of a value from debug symbols. Output string is in C syntax no matter the source language. Will fail if not compiled with debug symbols.
/// TODO: Handle returning multiple aliasing names
std::string llvm::getOriginalName(const Value* V) {
  return ::getOriginalName(V, nullptr);
}

