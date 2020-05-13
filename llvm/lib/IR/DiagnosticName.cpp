//===- llvm/Support/DiagnosticName.cpp - Diagnostic Value Names -*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines utility functions for providing human friendly names to improve diagnostics.
//
//===----------------------------------------------------------------------===//

#include "llvm/IR/DiagnosticName.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/BinaryFormat/Dwarf.h"
#include "llvm/IR/Argument.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DIBuilder.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/GlobalIndirectSymbol.h"
#include "llvm/IR/GlobalValue.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/ModuleSlotTracker.h"
#include "llvm/IR/Use.h"
#include "llvm/IR/User.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"
#include <algorithm>
#include <bits/stdint-intn.h>
#include <bits/stdint-uintn.h>
#include <deque>
#include <sstream>
#include <string>
#include <utility>

#define DEBUG_TYPE "diagnostic-name"
#define DLOG(args) LLVM_DEBUG(errs() << "[" /*<< __FILE__ << ":"*/ << __LINE__ << "] "<< args << "\n")
STATISTIC(NumGON, "Number of calls to getOriginalName");
STATISTIC(NumGONImpl, "Number of calls to getOriginalNameImpl - higher ratio to getOriginalName means more recursion");
STATISTIC(NumNoDbgFrag, "Number of calls to getFragmentNameNoDbg");
STATISTIC(NumNoDbgFound, "Number of failed attempts to find debug info for getOriginalNameImpl");
STATISTIC(NumDbgRequested,
          "Number of calls to getOriginalNameImpl with FinalType non-null");
STATISTIC(NumBB, "Number calls to getOriginalNameImpl for basic blocks");
STATISTIC(NumBitcastFails, "Number of analyzed bitcast instructions for which DIType could not be found");
STATISTIC(NumBitcastWidenSuccesses,
          "Number of successfully analyzed widening bitcast instructions for "
          "which DIType could be found");
STATISTIC(NumBitcastNarrowingSuccesses,
          "Number of successfully analyzed narrowing bitcast instructions for "
          "which DIType could be found");

namespace llvm {

static DIType *trimNonPointerDerivedTypes(DIType *DITy) {
  DLOG("DITy: " << DITy);
  while (isa_and_nonnull<DIDerivedType>(DITy) &&
         DITy->getTag() != dwarf::DW_TAG_pointer_type) {
    DLOG("DITy: " << *DITy);
    DITy = cast<DIDerivedType>(DITy)->getBaseType();
  }
  DLOG("returning: " << DITy);
  return DITy;
}

  static void printIndent(unsigned int indent) {
    for (unsigned int i = 0; i < indent; i++)
      errs() << "  ";
  }
static void printValueType(const Type *T, unsigned int indent = 0) {
  printIndent(indent);
  if (indent > 5) {
    errs() << "[omitted]\n";
    return;
  }
  if (!T) {
    errs() << "[null]\n";
    return;
  }
  errs() << *T << "\n";
  if (auto PtrTy = dyn_cast<PointerType>(T)) {
    printValueType(PtrTy->getElementType(), indent + 1);
  }
  if (auto StructTy = dyn_cast<StructType>(T)) {
    for (unsigned i = 0; i < StructTy->getNumElements(); i++) {
      if (i > 3) {
        printIndent(indent + 1);
        errs() << "[rest of fields omitted]\n";
        break;
      }
      printValueType(StructTy->getStructElementType(i), indent + 1);
    }
  }
  if (auto SeqTy = dyn_cast<ArrayType>(T)) {
    printValueType(SeqTy->getElementType(), indent + 1);
  }
}

static void printDbgType(const DIType *T, unsigned int indent = 0) {
  printIndent(indent);
  if (indent > 8) {
    errs() << "[omitted]\n";
    return;
  }
  if (!T) {
    errs() << "[null]\n";
    return;
  }
  errs() << *T << "\n";
  if (auto Derived = dyn_cast<DIDerivedType>(T)) {
    printDbgType(Derived->getBaseType(), indent + 1);
  }
  if (auto Comp = dyn_cast<DICompositeType>(T)) {
    for (unsigned i = 0; i < Comp->getElements().size(); i++) {
      if (i > 5) {
        printIndent(indent+1);
        errs() << "[rest of fields omitted]\n";
        break;
      }
      auto Elem = Comp->getElements()[i];
      if (!isa<DIType>(Elem)) { // e.g. DISubrange, DIEnumerator
        printIndent(indent + 1);
        errs() << *Elem << "\n";
      } else {
        printDbgType(cast<DIType>(Elem), indent + 1);
      }
    }
  }
}

  static bool isFirstFieldNestedValueTypeOrEqual(const Type *Outer, const Type *Inner, unsigned &NestingDepth) {
  DLOG("isNestedOrEqual? " << *Outer << " vs " << *Inner);
  NestingDepth++;
  if (Outer == Inner) return true;
  if (auto StructTy = dyn_cast<StructType>(Outer)) {
    return isFirstFieldNestedValueTypeOrEqual(StructTy->getElementType(0), Inner, NestingDepth);
  }
  if (auto SeqTy = dyn_cast<ArrayType>(Outer)) {
    return isFirstFieldNestedValueTypeOrEqual(SeqTy->getElementType(),
                                              Inner, NestingDepth);
  }
  return false;
}

  static bool isFirstFieldNestedValueType(const Type *Outer, const Type *Inner, unsigned &NestingDepth) {
  DLOG("isNested? " << *Outer << " vs " << *Inner);
  while(Outer->isPointerTy() && Inner->isPointerTy()) {
    Outer = cast<PointerType>(Outer)->getElementType();
    Inner = cast<PointerType>(Inner)->getElementType();
  }
  NestingDepth = 0;
  return Outer != Inner && isFirstFieldNestedValueTypeOrEqual(Outer, Inner, NestingDepth);
}
} // namespace llvm

llvm::DiagnosticNameGenerator llvm::DiagnosticNameGenerator::create(Module *M) {
  if (!M) return DiagnosticNameGenerator(nullptr, nullptr);
  auto NamedMD = M->getNamedMetadata("llvm.dbg.cu");
  if (!NamedMD) return DiagnosticNameGenerator(nullptr, nullptr);

  if (auto CU = dyn_cast<DICompileUnit>(NamedMD->getOperand(0))) {
    return DiagnosticNameGenerator(
        M, new DIBuilder(*M, /*AllowUnresolved*/ true, CU));
  }
  return DiagnosticNameGenerator(M, nullptr);
}
llvm::DiagnosticNameGenerator::DiagnosticNameGenerator(Module *M, DIBuilder *B)
    : M(M), Builder(B) {
  if (!M)
    return;
  DIF.processModule(*M);
  for (auto User : DIF.types()) {
    if (auto Comp = dyn_cast<DICompositeType>(User)) {
      for (auto Elem : Comp->getElements()) {
        if (!isa_and_nonnull<DIType>(Elem))
          continue;
        // Skipping derived fluff types not only saves memory and shortens the
        // chain to traverse, but it also makes the length of the use chain
        // predictable. This lets us cap the number of uses to traverse.
        auto RealElem = trimNonPointerDerivedTypes(cast<DIType>(Elem));
        if (!DITypeUsers.count(RealElem)) {
          DITypeUsers.insert(
              std::make_pair(RealElem, new SmallVector<DIType *, 4>));
        }
        DITypeUsers.find(RealElem)->getSecond()->push_back(cast<DIType>(User));
      }
    } else if (auto Derived = dyn_cast<DIDerivedType>(User)) {
      auto Base = trimNonPointerDerivedTypes(Derived->getBaseType());
      if (!Base) continue;
      if (!DITypeUsers.count(Base)) {
        DITypeUsers.insert(
            std::make_pair(Base, new SmallVector<DIType *, 4>));
      }
      DITypeUsers.find(Base)->getSecond()->push_back(cast<DIType>(User));
    }
  }
}

llvm::DIDerivedType *llvm::DiagnosticNameGenerator::createPointerType(DIType *BaseTy) {
    if (!Builder) return nullptr;
    return Builder->createPointerType(BaseTy, M->getDataLayout().getPointerSize());
  }

  namespace {
    using namespace llvm;
    static TypeCompareResult compareValueTypeAndDebugTypeInternal(
        const Type *Ty, const DIType *DITy,
        DenseMap<const DICompositeType *, const StructType *>
            &EquivalentStructTypes) {
      if (!DITy) {
        DLOG("dity null, Ty: " << *Ty);
        llvm_unreachable("DITy null");
      }
      DLOG(__FUNCTION__ << "\n"
                        << "Ty: " << *Ty << "\n"
                        << "DITy: " << *DITy);

      // These types are abstractions that don't exist in IR types
      switch (DITy->getTag()) {
      case dwarf::DW_TAG_typedef:
      case dwarf::DW_TAG_member:
      case dwarf::DW_TAG_const_type:
      case dwarf::DW_TAG_atomic_type:
        auto NextTy = cast<DIDerivedType>(DITy)->getBaseType();
        if (!NextTy) { // void type
          return llvm::IncompleteTypeMatch;
        }
        return compareValueTypeAndDebugTypeInternal(Ty, NextTy,
                                                    EquivalentStructTypes);
      }
      TypeCompareResult res = Match;
      if (DITy->getTag() == dwarf::DW_TAG_union_type) {
        DLOG("union type: " << *DITy);
        auto DIUnionTy = cast<DICompositeType>(DITy);
        if (auto StructTy = dyn_cast<StructType>(Ty)) {
          // there is no union type in LLVM, just a struct of the largest
          // element size
          if (!StructTy->getNumElements()) {
            return IncompleteTypeMatch;
          }
          auto Elem = StructTy->getElementType(0);
          for (auto DIUnionElem : DIUnionTy->getElements()) {
            DLOG("union elem: " << *DIUnionElem);
            if (compareValueTypeAndDebugTypeInternal(
                    Elem, cast<DIType>(DIUnionElem), EquivalentStructTypes) ==
                Match)
              return Match;
          }
          res = IncompleteTypeMatch;
          // Unions aren't very typesafe, just hope that this is a struct field
          // that's not needed right now
          // TODO: look into supporting union bitcast pattern
        } else {
          res = NoMatch;
        }
      }
      if (auto StructTy = dyn_cast<StructType>(Ty)) {
        if (StructTy->getNumElements() == 0)
          return IncompleteTypeMatch; // essentially void type afaict
        if (DITy->getTag() != dwarf::DW_TAG_structure_type) {
          LLVM_DEBUG(errs() << "value type is struct, but debug type is "
                            << *DITy << "\n");
          return NoMatch;
        }
        DLOG("struct dity: " << *DITy);
        auto DIStructTy = cast<DICompositeType>(DITy);
        const auto PrevResult = EquivalentStructTypes.find(DIStructTy);
        if (PrevResult != EquivalentStructTypes.end()) {
          if (PrevResult->second == StructTy) {
            DLOG("looped around for " << *DITy);
            return Match; // We have looped back around and not found
                          // any contradictions
          }
          DLOG("looped around for " << *DITy << ", but types not matching");
          DLOG("prev: " << *PrevResult->second);
          DLOG("curr: " << *StructTy);
          return NoMatch;
        }
        DLOG("struct dity: " << *DIStructTy);
        DLOG("structty: " << *StructTy);
        DINodeArray Elems = DIStructTy->getElements();
        for (auto MD : Elems) {
          if (MD) {
            DLOG("MD: " << *MD);
          } else {
            DLOG("MD null");
          }
        }
        if (Elems.empty()) {
          DLOG("elems empty");
        }
        DLOG("num operands: " << Elems.size());
        EquivalentStructTypes.insert(std::make_pair(DIStructTy, StructTy));
        if (StructTy->getNumElements() !=
            DIStructTy->getElements().size()) {
          LLVM_DEBUG(errs() << "value type is struct with "
                            << StructTy->getNumElements() << " elements, "
                            << " but debug type is struct with "
                            << DIStructTy->getElements().size()
                            << " elements\n");
          return NoMatch;
        }
        if (!res) {
          llvm_unreachable("res false before checking fields");
        }
        for (unsigned i = 0; i < StructTy->getNumElements(); i++) {
          auto FieldTy = StructTy->getElementType(i);
          auto DIFieldTy = DIStructTy->getElements()[i];
          if (!isa<DIType>(DIFieldTy))
            llvm_unreachable("weird ditype");
          res = std::min(res, compareValueTypeAndDebugTypeInternal(
                                  FieldTy, cast<DIType>(DIFieldTy),
                                  EquivalentStructTypes));
          if (!res) {
            DLOG("struct field " << i << " does not match");
            DLOG("value field ty: " << *FieldTy);
            DLOG("dbg field ty: " << *DIFieldTy);
            DLOG("dbg ty: " << *DITy);
            DLOG("value ty: " << *Ty);
            break;
          }
        }
      } else if (auto PtrTy = dyn_cast<PointerType>(Ty)) {
        switch (DITy->getTag()) {
        case dwarf::DW_TAG_pointer_type: {
          DLOG("pointer dity: " << *DITy);
          auto DIPtrTy = cast<DIDerivedType>(DITy);
          if (!trimNonPointerDerivedTypes(DIPtrTy->getBaseType()))
            return IncompleteTypeMatch; // void pointer afaict
          res =
              std::min(res, compareValueTypeAndDebugTypeInternal(
                                PtrTy->getElementType(), DIPtrTy->getBaseType(),
                                EquivalentStructTypes));
          if (!res) {
            LLVM_DEBUG(errs() << "pointer base types do not match\n");
            if (PtrTy->getElementType()->isIntegerTy()) {
              res = IncompleteTypeMatch; // Could be void pointer
            }
            // LLVM_DEBUG(printValueType(Ty));
            // LLVM_DEBUG(printDbgType(DITy));
          }
        };
        break;
        case dwarf::DW_TAG_array_type: {
          /* Pointer to sequential is normal array type variable.
             Sequential is the array of elements in memory, which includes
             array fields in structs. Pointer to element type can be e.g. function argument.
             So DW_TAG_array_type matches sequential types,
             pointers to sequentials, and normal pointers.
          */
          if (!isa<ArrayType>(PtrTy->getElementType())) {
            DLOG("dity is array, but ty does not point to seq");
            // Value type is just pointer, so just compare base types
            res = compareValueTypeAndDebugTypeInternal(
                PtrTy->getElementType(),
                cast<DICompositeType>(DITy)->getBaseType(),
                EquivalentStructTypes);
          } else {
            // Pointer to sequential type, so we can compare array sizes.
            // If sizes mismatch we still get incomplete type match since
            // the smaller array is a subtype of the larger (provided base types match).
            res = compareValueTypeAndDebugTypeInternal(
                PtrTy->getElementType(), DITy, EquivalentStructTypes);
          }
        }; break;
        default:
          LLVM_DEBUG(errs() << "value type is pointer, but debug type is "
                            << *DITy << "\n");
          return NoMatch;
        }
      } else if (auto IntTy = dyn_cast<IntegerType>(Ty)) {
        DLOG("IntTy");
        unsigned IntSize = IntTy->getIntegerBitWidth();
        if (auto Basic = dyn_cast<DIBasicType>(DITy)) {
          DLOG("dbg size in bits: " << Basic->getSizeInBits());
          DLOG("value size in bits: " << IntSize);
          DLOG("int tag: " << Basic->getTag());
          res = Basic->getSizeInBits() == IntSize ? Match : NoMatch;
        } else if (DITy->getTag() == dwarf::DW_TAG_enumeration_type) {
          DLOG("value size in bits: " << IntSize);
          uint64_t EnumSize = DITy->getSizeInBits();
          for (auto Elem : cast<DICompositeType>(DITy)->getElements()) {
            DLOG("enumerator: " << *Elem);
            auto EnumValue = cast<DIEnumerator>(Elem)->getValue();
          }
          DLOG("enum size in bits: " << EnumSize);
          res = IntSize == EnumSize ? Match : IncompleteTypeMatch;
        } else {
          DLOG("DITy not matching integer: " << *DITy);
          return NoMatch;
        }
      } else if (Ty->isFloatingPointTy()) {
        if (!isa<DIBasicType>(DITy)) {
          return NoMatch;
        }
        unsigned FloatSize = Ty->getPrimitiveSizeInBits();
        auto Basic = cast<DIBasicType>(DITy);
        res = Basic->getSizeInBits() == FloatSize ? Match : NoMatch;
      } else if (auto FuncTy = dyn_cast<FunctionType>(Ty)) {
        DLOG("FuncTy " << *FuncTy);
        if (auto DIFuncTy = dyn_cast<DISubroutineType>(DITy)) {
          DLOG("DIFuncTy " << *DIFuncTy);
          auto NumParams = FuncTy->getNumParams();
          auto DITypes = DIFuncTy->getTypeArray();
          if (NumParams != DITypes.size() - 1) {
            DLOG("value params: " << NumParams
                                  << " di params: " << DITypes.size() - 1);
            return NoMatch;
          }
          DLOG("num params: " << NumParams);
          DLOG("return type: " << *FuncTy->getReturnType());
          DLOG("ditypes begin: " << *DITypes.begin());
          if (FuncTy->getReturnType()->isVoidTy()) {
            if (!DITypes[0]) {
              res = Match;
            } else {
              res = IncompleteTypeMatch;
            }
          } else {
            res = std::min(res, compareValueTypeAndDebugTypeInternal(
                                    FuncTy->getReturnType(),
                                    cast<DIType>(DITypes[0]),
                                    EquivalentStructTypes));
          }
          if (!res) {
            DLOG("return type mismatch");
          }
          for (unsigned i = 0; i < NumParams; i++) {

            if (DITypes) {
              DLOG("param dity: " << *DITypes[i + 1]);
              res = std::min(res, compareValueTypeAndDebugTypeInternal(
                                      FuncTy->getParamType(i),
                                      cast<DIType>(DITypes[i + 1]),
                                      EquivalentStructTypes));
            } else {
              llvm_unreachable("DITypes null");
            }
            if (!res) {
              DLOG("param type mismatch: " << i);
            }
          }
        }
      } else if (auto SeqTy = dyn_cast<ArrayType>(Ty)) {
        DLOG("seqty: " << *SeqTy);
        DLOG("dity: " << *DITy);
        switch(DITy->getTag()) {
        default:
          DLOG("unexpected dity tag for seq");
          LLVM_DEBUG(printDbgType(DITy));
          return NoMatch;
        // check element type match
        case dwarf::DW_TAG_array_type: {
          auto DIArrayTy = cast<DICompositeType>(DITy);
          res = compareValueTypeAndDebugTypeInternal(SeqTy->getElementType(),
                                                     DIArrayTy->getBaseType(),
                                                     EquivalentStructTypes);
          if (SeqTy->getElementType()->isPointerTy() &&
              DIArrayTy->getBaseType()->getTag() ==
                  dwarf::DW_TAG_pointer_type) {
            // TODO: double check this
            res = std::max(res, IncompleteTypeMatch);
          }
          DLOG("elements[0]: " << *DIArrayTy->getElements()[0]);
          auto DISubr = cast<DISubrange>(DIArrayTy->getElements()[0]);
          if (DISubr->getTag() != dwarf::DW_TAG_subrange_type) {
            DLOG("array dity without subrange base");
            return NoMatch;
          }
          // check size match
          if (auto Count = DISubr->getCount().dyn_cast<ConstantInt *>()) {
            DLOG("count: " << *Count);
            if (Count->getZExtValue() != SeqTy->getNumElements()) {
              res = std::min(res, IncompleteTypeMatch); // Subtype relationship
            }
          } else {
            DLOG("SeqTy: " << *SeqTy);
            DLOG("subr: " << *DISubr);
            if (DISubr->getCount().isNull()) {
              llvm_unreachable("subrange size null");
            }
            DLOG("nonconst subrange size: "
                 << *DISubr->getCount().get<DIVariable *>());
            llvm_unreachable("nonconst subrange size");
          }
        }
        }
      } else if (Ty->isVoidTy()) {
        res = IncompleteTypeMatch;
      } else {
        errs() << "Ty: " << *Ty << "\n";
        errs() << "DITy: " << *DITy << "\n";
        llvm_unreachable("unhandled comparison type for value");
      }
      return res;
    }
    } // namespace

  TypeCompareResult DiagnosticNameGenerator::compareValueTypeAndDebugType(const Type *Ty, const DIType *DITy) {
    if (!DITy) return NoMatch;
    LLVM_DEBUG(errs() << "valuetype: " << *Ty << " dbg type: " << *DITy << "\n");
    //if (DITy->getTag() == dwarf::DW_TAG_member) return NoMatch;
    DenseMap<const DICompositeType*, const StructType*> EquivalentStructTypes;
    auto ret =  compareValueTypeAndDebugTypeInternal(Ty, DITy, EquivalentStructTypes);
    //LLVM_DEBUG(errs() << "leaving " << __FUNCTION__ << "\n");
    return ret;
  }

  /// returns a non-zero value if Ty is a pointer to a type that
  /// compareValueTypeAndDebugType would match with DITy, or to a type that
  /// isPointerChainToType would return a non-zero value for. Return value
  /// represents the number of pointers in the chain. Zero means that the types do not match.
std::pair<TypeCompareResult, uint32_t> DiagnosticNameGenerator::isPointerChainToType(const PointerType *Ty,
                                                         DIType *DITy) {
    Type *BaseTy = Ty->getElementType();
    if (auto res = compareValueTypeAndDebugType(BaseTy, DITy))
      return std::make_pair(res, 1);
    if (auto PointerTy = dyn_cast<PointerType>(BaseTy)) {
      auto Ret = isPointerChainToType(PointerTy, DITy);
      if (Ret.first)
        return std::make_pair(Ret.first, Ret.second + 1);
    }
    return std::make_pair(NoMatch, 0);
  }

  // TODO @henrik: prevent infinite loop
  std::pair<TypeCompareResult, DIType*>
  DiagnosticNameGenerator::calibrateDebugType(const Type *Ty, DIType *DITy) {
    DLOG("entering " << __FUNCTION__);
    DLOG("ty: " << *Ty);
    DLOG("dity: " << *DITy);
    TypeCompareResult res = NoMatch;
    SmallSet<DIType*, 8> Visited;
    while (res = compareValueTypeAndDebugType(Ty, DITy), DITy && !res) {
      LLVM_DEBUG(errs() << __FUNCTION__ << "\n");
      if (Visited.count(DITy)) {
        DLOG("looped back around to " << *DITy);
        return std::make_pair(NoMatch, nullptr);
      }
      if (res) {
        DLOG("res: " << res);
        llvm_unreachable("res invalid");
      }
      if (auto PointerTy = dyn_cast<PointerType>(Ty)) {
        if (Builder) {
          auto res = isPointerChainToType(PointerTy, DITy);
          if (uint32_t PtrChainLength = res.second) {
            // Sometimes we have a pointer to a e.g. a non pointer struct field.
            // The debug type tree does not contain a pointer type to this type,
            // so we construct it ourselves. Does not work if we do not have a
            // DIBuilder instance
            for (uint32_t i = 0; i < PtrChainLength; i++)
              DITy = createPointerType(DITy);
            LLVM_DEBUG(errs() << "leaving " << __FUNCTION__ << "\n");
            return std::make_pair(res.first, DITy);
          }
        }
      }
      Visited.insert(DITy);
      if (auto Comp = dyn_cast<DICompositeType>(DITy)) {
        switch (Comp->getTag()) {
        case dwarf::DW_TAG_enumeration_type:
             LLVM_DEBUG(errs() << "could not match types\n");
             LLVM_DEBUG(errs() << "leaving " << __FUNCTION__ << "\n");
             return std::make_pair(NoMatch, nullptr);
        case dwarf::DW_TAG_array_type:
          DITy = Comp->getBaseType();
          break;
        default:
          if (Comp->getElements().empty()) {
            DITy = nullptr;
            break;
          }
          DLOG("elements[0]" << *Comp->getElements()[0]);
          DITy = cast<DIType>(Comp->getElements()[0]);
          break;
        }
      } else if (auto Derived = dyn_cast<DIDerivedType>(DITy)) {
        DITy = Derived->getBaseType();
      } else {
        LLVM_DEBUG(errs() << "could not match types\n");
        LLVM_DEBUG(errs() << "leaving " << __FUNCTION__ << "\n");
        return std::make_pair(NoMatch, nullptr);
      }
    }
    LLVM_DEBUG(errs() << "leaving " << __FUNCTION__ << "\n");
    return std::make_pair(res, DITy);
  }

  void printAllMetadata(const Instruction &I) {
    /*errs() << "metadata\n";
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
    errs() << "end metadata\n";*/
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
      LLVM_DEBUG(errs() << "file: " << *DDI->getVariable()->getFile() << " endfile\n");
      LLVM_DEBUG(DDI->getVariable()->print(errs(), nullptr, true));
      LLVM_DEBUG(errs() << "\n");
      if (DDI->getVariable()->getType()) {
        DLOG("type: ");
        printDbgType(DDI->getVariable()->getType());
      }
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

// TODO: dedup
  std::string llvm::DiagnosticNameGenerator::getFragmentTypeName(
      DIType *const T, int64_t Offset, DIType **const FinalType,
      std::string Sep /* = "."*/) {
    assert(T && "fragment type null");
    if (!T) return "";
    DLOG("offset: " << Offset);
    if (T->getTag() == dwarf::DW_TAG_enumeration_type) {
      return ""; // fragments of enums are not relevant?
    }
    if (auto Comp = dyn_cast<DICompositeType>(T)) {
      DIDerivedType *tmpT = nullptr;
      auto elements = Comp->getElements();
      switch (Comp->getTag()) {
      case dwarf::DW_TAG_array_type: {
        auto Base = Comp->getBaseType();
        auto Size = Base->getSizeInBits();
        auto Idx = Offset / Size;
        return "[" + std::to_string(Idx) + "]" +
               getFragmentTypeName(Base, Offset - Idx * Size, FinalType);
      }
      case dwarf::DW_TAG_union_type: {
        DLOG("union: " << *T);
        llvm_unreachable("union type in fragment bit offset");
      }
      default:
        for (auto E : Comp->getElements()) {
          // LLVM_DEBUG(E->dump());
          if (auto E2 = dyn_cast<DIDerivedType>(E)) {
            int64_t O2 = E2->getOffsetInBits();
            if (O2 > Offset) {
              break;
            }
            tmpT = E2;
          } else {
            DLOG("E: " << *E);
            llvm_unreachable("non derived struct member");
          }
        }
        if (!tmpT) {
          DLOG("T: " << *T);
          for (auto E2 : Comp->getElements()) {
            DLOG("E2: " << *E2);
          }
          llvm_unreachable("no-elements-in-struct?");
        }
        return Sep + tmpT->getName().str() +
               getFragmentTypeName(tmpT, Offset - tmpT->getOffsetInBits(),
                                   FinalType);
      }
      }
    if (auto Derived = dyn_cast<DIDerivedType>(T)) {
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

namespace llvm {

  std::string DiagnosticNameGenerator::getFragmentTypeName(DIType *T, const int64_t *Offsets_begin, const int64_t *Offsets_end, const Type *ValueTy, DIType **FinalType, std::string Sep/* = "."*/) {
    int64_t Offset = -1;
    if (ValueTy) {
      DLOG("valuety: " << *ValueTy);
    }
    if (!T) {
      DLOG("fragment type null");
      return "fragment-type-null";
    }
    DLOG("getFragmentTypeName: " << *T);
    if (auto Derived = dyn_cast<DIDerivedType>(T)) {
      if(Derived->getBaseType()) {
        /*LLVM_DEBUG(errs() << "basetype: " << *Derived->getBaseType() << " " << Derived->getBaseType()->getName() << "\n");
        LLVM_DEBUG(Derived->getBaseType()->dump());
        LLVM_DEBUG(Derived->getBaseType()->printAsOperand(errs()));*/
      } else {
        LLVM_DEBUG(errs() << "no basetype\n");
      }
    }
    if (Offsets_begin == Offsets_end) {
      DLOG("offsets_begin == offsets_end");
    } else {
      Offset = *Offsets_begin;
      DLOG("offset: " << Offset);
    }
    // derived non pointer types don't count, so we still haven't reached the final type if we encounter one of those
    if (Offsets_begin == Offsets_end  && (!isa<DIDerivedType>(T) || T->getTag() == dwarf::DW_TAG_pointer_type)) {
      if(FinalType){
        *FinalType = T;
        DLOG("setting finaltype to " << *T);
      }
      return "";
    }
    if (auto Comp = dyn_cast<DICompositeType>(T)) {
      DLOG("Comp");
      auto elements = Comp->getElements();
      switch (Comp->getTag()) {
      case dwarf::DW_TAG_array_type:
        DLOG("array");
        LLVM_DEBUG({
            auto Subr = cast<DISubrange>(elements[0]);
            if (auto CI = Subr->getCount().dyn_cast<ConstantInt *>()) {
              if (Offset >= CI->getSExtValue()) {
                errs() << "Offset: " << Offset << "\n";
                errs() << "subr.getCount(): " << CI << "\n";
                errs() << "subr: " << *Subr << "\n";
                errs() << "elements: " << *Comp->getElements() << "\n";
                errs() << "Comp: " << *Comp << "\n";
                DLOG("not-enough-elements-in-array?");
              }
            }
        });
        return "[" + std::to_string(Offset) + "]" +
               getFragmentTypeName(Comp->getBaseType(), Offsets_begin + 1,
                                   Offsets_end, ValueTy->getArrayElementType(),
                                   FinalType);
      case dwarf::DW_TAG_union_type: {
        DLOG("union");
        DLOG("valuety: " << *ValueTy);
        Type *NextType = ValueTy->getContainedType(Offset);
        DIType *BestMatch = nullptr;
        for (auto elem : elements) {
          auto UnionElem = cast<DIType>(elem);
          auto res = compareValueTypeAndDebugType(NextType, UnionElem);
          if (res == Match) {
            BestMatch = UnionElem;
            DLOG("found complete union match: " << *UnionElem);
            break;
          } else if (res == IncompleteTypeMatch) {
            DLOG("incomplete type match");
            BestMatch = UnionElem;
          }
        }
        if (!BestMatch) {
          if (FinalType) *FinalType = nullptr;
          DLOG("found no union match");
          LLVM_DEBUG(printDbgType(T));
          LLVM_DEBUG(printValueType(ValueTy));
          return getFragmentNameNoDbg(NextType, Offsets_begin + 1, Offsets_end);
        }
        DLOG("found some match: " << *BestMatch);
        return getFragmentTypeName(BestMatch, Offsets_begin + 1, Offsets_end,
                                   NextType,
                                   FinalType);
      }
      default:
        DLOG("default");
        if (Offset >= elements.size()) {
          errs() << "Offset: " << Offset << "\n";
          errs() << "elements.size(): " << elements.size() << "\n";
          errs() << "elements: " << *elements << "\n";
          errs() << "Comp: " << *Comp << "\n";
          for (auto elem : elements) {
            errs() << "elem: " << *elem << "\n";
          }
          llvm_unreachable("not-enough-elements-in-struct?");
        }
        if (auto NextT = dyn_cast<DIDerivedType>(elements[Offset])) {
          DLOG("valuety: " << *ValueTy);
          return Sep + NextT->getName().str() +
                 getFragmentTypeName(NextT, Offsets_begin + 1, Offsets_end,
                                     ValueTy->getContainedType(Offset), FinalType);
        } else {
          errs() << "non derived struct member: " << elements[Offset] << "\n";
          errs() << "struct: " << Comp << "\n";
          llvm_unreachable(
              "Non derived type as struct member? Should not happen");
        }
      }
      llvm_unreachable("wtf");
    } else if (auto Derived = dyn_cast<DIDerivedType>(T)) {
      if (FinalType) {
        *FinalType = Derived;
        DLOG("setting finaltype to " << *Derived);
      }
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
      DLOG("non-ptr derived, skipping");
      return getFragmentTypeName(Derived->getBaseType(), Offsets_begin, Offsets_end, ValueTy, FinalType, Sep);
    }
    DLOG("fallback");
    DLOG("T: " << *T);
    DLOG("valuety: " << *ValueTy);
    DLOG("offset: " << Offset);
    return ""; // This is the end of the chain, the type name is no longer part of the variable name
  }

  /// Store const uses as int64_t in a vector.
  /// Returns pointer to first non-const use, or Offsets_end if all constant.
  const Use *getConstOffsets(const Use *Offsets_begin, const Use *Offsets_end, SmallVectorImpl<int64_t> &vec) {
    while (Offsets_begin < Offsets_end) {
      Value *Offset = Offsets_begin->get();
      if (auto ConstOffset = dyn_cast<Constant>(Offset)) {
        //LLVM_DEBUG(errs() << "getConstOffsets: " << ConstOffset->getUniqueInteger().getSExtValue() << "\n");
        vec.push_back(ConstOffset->getUniqueInteger().getSExtValue());
      } else break;
      Offsets_begin++;
    }
    return Offsets_begin;
  }

  std::string DiagnosticNameGenerator::getNameFromDbgVariableIntrinsic(const DbgVariableIntrinsic *VI, DIType **const FinalType) {
    DILocalVariable *Val = VI->getVariable();
    DIType *Type = Val->getType();
    DIExpression *Expr = VI->getExpression();
    if(!Expr->isFragment()) {
      if (FinalType) *FinalType = Type;
      return Val->getName().str();
    }
    DLOG("expr: " << *Expr);
    DLOG("type: " << *Type);
    DLOG("val: " << *Val);
    int64_t Offset = -1;
    if (Expr->extractIfOffset(Offset)) { // FIXME extractIfOffset seems broken. Workaround below for now.
      llvm_unreachable("extractIfOffset works?");
      if (FinalType) *FinalType = Type;
      return Val->getName().str();
    }

    Optional<DIExpression::FragmentInfo> FIO = Expr->getFragmentInfo();
    if(FIO) {
      Offset = FIO->OffsetInBits;
    }
    LLVM_DEBUG(errs() << "original offset " << Offset << " num " << Expr->getNumElements() << " " << FIO->SizeInBits << " " << FIO->OffsetInBits << "\n");
    if (auto Derived = dyn_cast<DIDerivedType>(Type)) { // FIXME this needs more thorough testing
      //LLVM_DEBUG(errs() << "derived: " << Derived->getOffsetInBits() << "\n");
      //LLVM_DEBUG(Derived->dump());
      Type = Derived->getBaseType();
      //LLVM_DEBUG(errs() << "base:\n");
      if(!Type) llvm_unreachable("missing-base-type");
      //LLVM_DEBUG(Type->dump());
    }

    if (auto Comp = dyn_cast<DICompositeType>(Type)) {
      //LLVM_DEBUG(errs() << "composite:\n");
      //LLVM_DEBUG(Comp->dump());
      std::string Name = (Val->getName() + getFragmentTypeName(Comp, Offset, FinalType)).str();
      if (!FinalType) return Name;
      Value *V = VI->getVariableLocation();
      if (!compareValueTypeAndDebugType(V->getType(), *FinalType)) {
        LLVM_DEBUG(errs() << "mismatched types1 for " << *V << "\n");
        LLVM_DEBUG(errs() << "type: " << *V->getType() << "\n");
        LLVM_DEBUG(errs() << "dbg type: " << **FinalType << "\n");
        DLOG("VI: " << *VI);
        DLOG("Val: " << *Val);
        DLOG("scope: " << *Val->getScope());
        if (auto I = dyn_cast<Instruction>(V)) {
          DLOG("BB: " << *I->getParent());
        }
        printDbgType(*FinalType);
        printValueType(V->getType());
        *FinalType = nullptr;
        //llvm_unreachable("mismatched types1");
      }
      return Name;
    }
    llvm_unreachable("unknown-debug-info-pattern");
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


  /// A Value can have multiple non-identical debug intrinsics due to inlining
  /// and potentially also due to aliasing in general. We pick the first one with a
  /// matching DIType, if the DIType is requested. Otherwise just the first one.
  /// If no DbgVariableIntrinsic could be found (or one with matching DIType),
  /// an empty string is returned.
  /// TODO: Handle returning multiple aliasing names. Try closest dominance of an optional secondary value as tiebreaker?.
  std::string DiagnosticNameGenerator::tryGetNameFromDbgValue(const Value *V, DIType **FinalType) {
    SmallVector<DbgVariableIntrinsic *, 4> DbgValues;
    testFindDbgUsers(DbgValues, V); // TODO upstream fix
    //findDbgUsers(DbgValues, V);
    std::string NameWithoutFT = "";
    for (auto &DVI : DbgValues) {
      //LLVM_DEBUG(printIntrinsic(DVI));

      DLOG("DVI: " << *DVI);
      if (!V) {
        DLOG("V null");
        llvm_unreachable("V cannot be null");
      }
      DLOG("V ptr: " << V);
      DLOG("found dvi for: " << *V);
      DIType *T = nullptr;
      auto Name = getNameFromDbgVariableIntrinsic(DVI, &T);
      if (Name.empty()) continue;
      if (!FinalType) // don't check type match if not needed
        return Name;
      // hypothesis: most diffs here
      if (T) {
        DLOG("calibrating type");
        *FinalType = T; // save T for debug logs
        T = calibrateDebugType(V->getType(), T).second;
        if (!T) {
          DLOG("mismatched types8 for " << *V << "\n");
          DLOG("type: " << *V->getType() << "\n");
          DLOG("name: " << Name);
          LLVM_DEBUG(printDbgType(*FinalType));
          if (auto I = dyn_cast<Instruction>(V)) {
            auto F = I->getParent()->getParent();
            DLOG("function: " << *F);
          }
        } else {
          DLOG("matching types for " << *V << " T: " << *T);
          *FinalType = T;
          return Name;
        }
      }
      *FinalType = nullptr;
      NameWithoutFT = Name;
    }
    return NameWithoutFT;
  }

  // TODO check which string type is appropriate here
  /// Helper function for naming a pointer relative to some other base pointer
  std::string DiagnosticNameGenerator::getOriginalRelativePointerName(const Value *V, StringRef ArrayIdx, SmallVectorImpl<int64_t> &StructIndices, DIType **FinalType) {
    // Use the value V to name this pointer prefix, and make it return metadata for accessing debug symbols for our pointer
    DIType *T = nullptr;
    LLVM_DEBUG(errs() << "getting pointer prefix for " << *V << "\n");
    std::string ptrName;
    Type *ValueTy = V->getType();
    DLOG("valuety: " << *ValueTy);
    if (auto LI = dyn_cast<LoadInst>(V)) {
      ptrName = getOriginalNameImpl(LI->getPointerOperand(), &T);
    } else {
      ptrName = getOriginalNameImpl(V, &T);
      DLOG("valuety: " << *ValueTy);
      if (!isa<PointerType>(ValueTy)) {
        llvm_unreachable("valuety not pointer");
      }
      ValueTy = cast<PointerType>(ValueTy)->getElementType();
      DLOG("base valuety: " << *ValueTy);
    }
    DLOG("prefix: " << ptrName << " arrayidx: " << ArrayIdx);
    if (!T) {
      DLOG("T null for " << *V);
      return ptrName + ArrayIdx.str() +
             getFragmentNameNoDbg(V->getType(), StructIndices.begin(),
                                  StructIndices.end());
    }
    /*if (!(T = calibrateDebugType(V->getType(), T))) {

      LLVM_DEBUG(errs() << "mismatched types2 for " << *V << "\n");
      LLVM_DEBUG(errs() << "type: " << *V->getType() << "\n");
      LLVM_DEBUG(errs() << "dbg type: " << *T<< "\n");
      llvm_unreachable("mismatched types2");
    } else {
      LLVM_DEBUG(errs() << "matching types for " << *V << "\n");
      }*/
    DLOG("GEP: " << *V);
    if (auto PT1 = dyn_cast<DIDerivedType>(T)) {
      DLOG("PT1: " << *PT1);
      T = PT1->getBaseType();
    } else if (T->getTag() == dwarf::DW_TAG_array_type) {
      auto AT = cast<DICompositeType>(T);
      DLOG("AT: " << *AT);
      T = AT->getBaseType();
    }
    T = trimNonPointerDerivedTypes(T);
    if (!T) {
      DLOG("basetype null for " << *V);
      return ptrName + ArrayIdx.str() + getFragmentNameNoDbg(V->getType(), StructIndices.begin(), StructIndices.end());
    } else {
      DLOG("basetype: " << *T);
    }
    DLOG("valuety: " << *ValueTy);
    if (auto PT = dyn_cast<DIDerivedType>(T); PT && PT->getBaseType()) {
      DLOG("GEP: " << *V);
      DLOG("PT: " << *PT);
      std::string Sep = ".";
      if (ArrayIdx.empty() && PT->getTag() == dwarf::DW_TAG_pointer_type) Sep = "->"; // if explicit array indexing, dereference is already done
      DLOG("valuety: " << *ValueTy);
      if (auto PT2 = dyn_cast<PointerType>(ValueTy)) {
        ValueTy = PT2->getElementType();
      } else {
        ValueTy = cast<ArrayType>(ValueTy)->getArrayElementType();
      }
      return ptrName + ArrayIdx.str() +
             getFragmentTypeName(trimNonPointerDerivedTypes(PT->getBaseType()),
                                 StructIndices.begin(), StructIndices.end(),
                                 ValueTy, FinalType, Sep);
    }
    DLOG("non ptr type: " << *T);
    return ptrName + ArrayIdx.str() +
           getFragmentTypeName(T, StructIndices.begin(), StructIndices.end(),
                               ValueTy, FinalType);
  }

  template <typename IdxType>
  std::string getFragmentNameNoDbgImpl(const Type *Ty, IdxType Idx, std::string IdxName) {
    std::string name = "";
    if (auto StructTy = dyn_cast<StructType>(Ty)) {
      Ty = StructTy->getTypeAtIndex(Idx);
      // This is not valid C/C++ syntax, but indexing structs by field index
      // is not valid C/C++. The field access is the normal kind in the source
      // code, but we don't have access to the field names here since we lack
      // debug info.
      name += ".getElem<";
      raw_string_ostream SSO(name);
      Ty->print(SSO);
      SSO.flush();
      name += ">(";
      name += IdxName;
      name += ")";
    } else if (auto SeqTy = dyn_cast<ArrayType>(Ty)) {
      name += "[";
      name += IdxName;
      name += "]";
      Ty = SeqTy->getElementType();
    } else if (auto SeqTy = dyn_cast<VectorType>(Ty)) {
      name += "[";
      name += IdxName;
      name += "]";
      Ty = SeqTy->getElementType();
    } else if (auto PtrTy = dyn_cast<PointerType>(Ty)) {
      name += "[";
      name += IdxName;
      name += "]";
      Ty = PtrTy->getElementType();
    } else {
      DLOG("Ty: " << *Ty);
      DLOG("Idx: " << Idx);
      DLOG("name: " << name);
      llvm_unreachable("unexpected type");
    }
    return name;
  }

  std::string DiagnosticNameGenerator::getFragmentNameNoDbg(
      const Type *Ty, const int64_t *idx_begin, const int64_t *idx_end) {
    std::string name = "";
    ++NumNoDbgFrag;
    for (auto itr = idx_begin; itr < idx_end; itr++) {
      auto Idx = *itr;
      auto IdxName = std::to_string(Idx);
      name += getFragmentNameNoDbgImpl<int64_t>(Ty, Idx, IdxName);
    }
    return name;
  }

  std::string DiagnosticNameGenerator::getFragmentNameNoDbg(
      const Type *Ty, const Use *idx_begin, const Use *idx_end) {
    std::string name = "";
    ++NumNoDbgFrag;
    for (auto itr = idx_begin; itr < idx_end; itr++) {
      auto Idx = itr->get();
      auto IdxName = getOriginalNameImpl(Idx, nullptr);
      name += getFragmentNameNoDbgImpl<Value *>(Ty, Idx, IdxName);
    }
    return name;
  }

  Type *getTypeAtOffset(Type *Ty, const SmallVectorImpl<int64_t> &Offsets) {
    for (auto Idx : Offsets) {
      if (auto StructTy = dyn_cast<StructType>(Ty)) {
        Ty = StructTy->getElementType(Idx);
      } else if (auto ArrayTy = dyn_cast<ArrayType>(Ty)) {
        Ty = ArrayTy->getElementType();
      } else {
        DLOG("valuety: " << *Ty);
        llvm_unreachable("unexpected value type");
      }
    }
    return Ty;
  }

  // TODO: look into simplifying this, it has grown organically when encountering more and more cases
  std::string DiagnosticNameGenerator::getOriginalPointerName(const GetElementPtrInst *const GEP, DIType **const FinalType) {
    LLVM_DEBUG(errs() << "GEP!\n");
    LLVM_DEBUG(GEP->dump());
    auto OP = GEP->getPointerOperand();
    //getFragmentNameNoDbg(OP->getType(), GEP->idx_begin(), GEP->idx_end());
    DLOG("operand: " << *OP);
    auto ValueTy = cast<PointerType>(GEP->getPointerOperandType())->getElementType();
    std::string ArrayIdx = "";
    assert(GEP->idx_begin() != nullptr);
    auto FirstOffset = GEP->idx_begin()->get();
    assert(FirstOffset != nullptr);
    LLVM_DEBUG(errs() << "first offset: " << *FirstOffset << "\n");
    if (!isa<Constant>(FirstOffset) || !cast<Constant>(FirstOffset)->isZeroValue()) {
      ArrayIdx = "[" + getOriginalNameImpl(FirstOffset, nullptr) + "]";
    }
    LLVM_DEBUG(errs() << "arrayidx: " << ArrayIdx << "\n");
    if (GEP->getNumIndices() == 1) {
      std::string arrayName = getOriginalNameImpl(OP, FinalType);
      if (FinalType && *FinalType) {
        if (!isa<DIDerivedType>(*FinalType)) {
          errs() << "not derived type: " << **FinalType << "\n";
          errs() << "GEP: " << *GEP << "\n";
          errs() << "OP: " << *OP << "\n";
          llvm_unreachable("not derived type");
        }// else *FinalType = cast<DIDerivedType>(*FinalType)->getBaseType();
      }
      return arrayName + ArrayIdx;
    }
    SmallVector<int64_t, 2> Indices;
    std::string name = "";
    if (!GEP->idx_begin()) {
      llvm_unreachable("idx begin null");
    }
    const Use *idx_last_const = getConstOffsets(GEP->idx_begin() + 1, GEP->idx_end(), Indices); // skip first offset, it doesn't matter if it's constant
    DIType *T = nullptr;
    if (auto LI = dyn_cast<LoadInst>(OP)) {
      // This means our pointer originated in the dereference of another pointer
      DLOG("getting relative ptr");
      name += getOriginalRelativePointerName(LI, ArrayIdx, Indices, &T);
      ValueTy = getTypeAtOffset(ValueTy, Indices);
      DLOG("back from load: " << *LI << " GEP: " << *GEP);
      if (!T) DLOG("T == nullptr");
    } else if (auto GEP2 = dyn_cast<GetElementPtrInst>(OP)) {
      // This means our pointer is some linear offset of another pointer, e.g. a subfield of a struct, relative to the pointer to the base of the struct
      DLOG("getting relative ptr");
      name += getOriginalRelativePointerName(GEP2, ArrayIdx, Indices, &T);
      ValueTy = getTypeAtOffset(ValueTy, Indices);
      DLOG("back from inner GEP: " << *GEP2 << " outer GEP: " << *GEP);
    } else {
      DLOG("fallback attempt");
      name = getOriginalNameImpl(OP, &T);
      if (!T) {
        // The code was compiled without debug info, or was optimised to the
        // point where it's no longer accessible. Give best effort indexing.
        DLOG("getting fragment type");
        return name + getFragmentNameNoDbg(OP->getType(), GEP->idx_begin(), GEP->idx_end());
      }
      if (T && !compareValueTypeAndDebugType(OP->getType(), T)) {
        LLVM_DEBUG(errs() << "mismatched types5 for " << *OP << "\n");
        LLVM_DEBUG(errs() << "type: " << *OP->getType() << "\n");
        LLVM_DEBUG(errs() << "dbg type: " << *T << "\n");
        llvm_unreachable("mismatched types5");
      } else {
        DLOG("matching types for " << *OP);
        DLOG("T: " << *T);
      }
      if (!OP->getType()->isPointerTy()) {
        llvm_unreachable("non-pointer OP in GEP");
      }
      T = trimNonPointerDerivedTypes(T);
      if (T)
        DLOG("T: " << *T);
      auto BaseType = T->getTag() == dwarf::DW_TAG_array_type
                          ? T
                          : cast<DIDerivedType>(T)->getBaseType();
      if (BaseType) {
        DLOG("baseType: " << *BaseType);
      } else {
        DLOG("basetype null");
      }
      T = nullptr;
      DLOG("getting fragment type");
      name += ArrayIdx + getFragmentTypeName(BaseType, Indices.begin(), Indices.end(), ValueTy, &T);
      ValueTy = getTypeAtOffset(ValueTy, Indices);
    }
    DLOG("back in GEP: " << *GEP << " OP: " << *OP);
    DLOG("name so far: " << name);
    if (T) DLOG("T: " << *T);
    else DLOG("T == null");

    // in case not all offsets (excluding the first one) were constant.
    // this can happen for example with arrays in structs
    while(idx_last_const < GEP->idx_end() && T) {
      DLOG("naming non const idx: " << *idx_last_const->get());
      name += "[" + getOriginalNameImpl(idx_last_const->get(), nullptr) + "]";

      DLOG("typing non const idx: " << *idx_last_const->get());
      if (auto ArrayTy = dyn_cast<ArrayType>(ValueTy)) {
        ValueTy = ArrayTy->getArrayElementType();
      } else if (auto StructTy = dyn_cast<StructType>(ValueTy)) {
        DLOG("comp type: " << *StructTy);
        ValueTy = StructTy->getTypeAtIndex(idx_last_const->get());
      } else {
        DLOG("valuety: " << *ValueTy);
        llvm_unreachable("unexpected value type");
      }

      if (auto Derived = dyn_cast<DIDerivedType>(T)) T = Derived->getBaseType();
      else if (auto Composite = dyn_cast<DICompositeType>(T)) T = Composite->getBaseType();
      else {
        errs() << "unhandled ditype: " << *T << "\n";
        llvm_unreachable("Unhandled DIType");
      }

      Indices.clear(); // collect potential remaining constant indices
      idx_last_const = getConstOffsets(idx_last_const + 1, GEP->idx_end(), Indices);

      DIType *T2 = nullptr; // avoid aliasing T
      name += getFragmentTypeName(T, Indices.begin(), Indices.end(), ValueTy, &T2);
      ValueTy = getTypeAtOffset(ValueTy, Indices);
      T = T2;
      if (T) DLOG("T: " << *T);
    }
    if (!FinalType || !T) return name;

    DLOG("T: " << *T);
    DLOG("Ty: " << *GEP->getType());
    DIType *T2 = nullptr; // avoid aliasing T for debug logs
    DLOG("calibrating type");
    if (!(T2 = calibrateDebugType(GEP->getType(), T).second)) {
      DebugInfoFinder DIF;
      DLOG("looking for uses of " << *T);
      DIF.processModule(*M);
      for (auto DITy : DIF.types()) {
        DLOG("Testing DITy: " << *DITy);
        auto Res = calibrateDebugType(GEP->getType(), DITy);
        if (Res.first == Match) {
          DLOG("Found matching type for " << *Res.second);
          *FinalType = Res.second;
          return name;
        }
        if (Res.first == IncompleteTypeMatch) {
          DLOG("IncompleteTypeMatch for " << *Res.second);
          T2 = Res.second;
        }
      }
      if (!T2) {
        DLOG("mismatched types6 for " << *OP);
        DLOG("type: " << *GEP->getType());
        DLOG("dbg type ptr: " << T);
        if (T) {
          DLOG("dbg type: " << *T);
          printDbgType(T);
        }
        llvm_unreachable("mismatched types6");
      }
    } else {
      DLOG("matching types for " << *GEP);
      T = T2;
    }

    *FinalType = T;
    DLOG("returning from GEP: " << *GEP);
    return name;
  }

  /// If this value is part of an use-def chain with a value that has a dbg variable intrinsic, name this value after that value. Only traverses backwards.
  /// Drawback is that operations may be skipped. If V is %i.next = phi [0, i + 1], we will name it "i", not "i + 1".
  /// Returns the number of the predecessor that led to this value. This is negative if no name was found.
  std::pair<std::string, int32_t> DiagnosticNameGenerator::getOriginalInductionVariableName(const User *U, DIType **FinalType) {
    SmallSet<const Value *, 10> Visited;
    // want BFS under the assumption that the names of values fewer hops away are more likely to represent this variable well
    std::deque<std::pair<const Value *, int32_t>> Queue;
    DLOG("U: " << U);
    if (auto PHI = dyn_cast<PHINode>(U)) {
      DLOG("phi: " << *PHI);
    }
    for (uint32_t i = 0; i < U->getNumOperands(); i++) {
      DLOG("U[" << i << "]" << U->getOperand(i));
      DLOG("*U[" << i << "]" << *U->getOperand(i));
      Queue.push_back(std::make_pair(U->getOperand(i), (int32_t)i));
    }
    while(!Queue.empty()) {
      const auto [V, pred] = Queue.front();
      DLOG("V: " << *V);
      Queue.pop_front();
      Visited.insert(V);
      auto Name = tryGetNameFromDbgValue(V, FinalType);
      if (!Name.empty()) {
        return std::make_pair(Name, pred);
      }

      if (auto U = dyn_cast<User>(V)) {
        for (const Use &OP : U->operands()) {
          if (Visited.count(OP)) continue;
          Queue.push_back(std::make_pair(OP.get(), pred));
        }
      }
    }
    return std::make_pair("", -1);
  }

  std::string DiagnosticNameGenerator::getOriginalStoreName(const StoreInst *ST, DIType **FinalType) {
    std::string PtrName = getOriginalNameImpl(ST->getPointerOperand(), nullptr);
    std::string ValueName = getOriginalNameImpl(ST->getValueOperand(), FinalType);
    return "*(" + PtrName + ") = " + ValueName;
  }

  std::string DiagnosticNameGenerator::getOriginalCallName(const CallBase *Call, DIType **FinalType) { // TODO: handle more intrinsics
    // TODO: return finaltype
    DLOG("call: " << *Call);
    std::string FuncName = Call->getName().str() + " (fallback name)";
    auto CalledFunc = Call->getCalledFunction();
    if (CalledFunc) {
      FuncName = CalledFunc->getName().str();
      if (auto IntrID = CalledFunc->getIntrinsicID()) {
        switch (IntrID) {
        case Intrinsic::memset:
          FuncName = "memset";
          break;
        case Intrinsic::memcpy:
          FuncName = "memcpy";
          break;
        case Intrinsic::memmove:
          FuncName = "memmove";
          break;
        case Intrinsic::sqrt:
          FuncName = "sqrt";
          break;
        default:
          errs() << "unhandled intrinsic: " << FuncName << "\n";
        }
      }
    }
    std::string Name = FuncName + "(";
    User::const_op_iterator end;
    User::const_op_iterator it;
    for (it = Call->arg_begin(), end = Call->arg_end(); it != end; it++) {
      Name += getOriginalNameImpl(it->get(), nullptr);
      if (end - it > 1)
        Name += ", ";
    }
    Name += ")";
    DLOG("call name: " << Name);
    return Name;
  }

  std::string DiagnosticNameGenerator::getOriginalSwitchName(const SwitchInst *Switch, DIType **FinalType) {
    std::string CondName = getOriginalNameImpl(Switch->getCondition(), nullptr);
    std::string Name = "switch (" + CondName + ") {\n";
    User::const_op_iterator end;
    User::const_op_iterator it;
    for (auto Case : Switch->cases()) {
      Name += "case " + getOriginalNameImpl(Case.getCaseValue(), nullptr) + ":\n";
      Name += getOriginalNameImpl(Case.getCaseSuccessor(), nullptr);
    }
    Name += "}";
    return Name;
  }

  std::string DiagnosticNameGenerator::getOriginalCmpName(const CmpInst *Cmp, DIType **FinalType) {
    std::string name = "<unknown-cmp>";
    std::string Op1 = getOriginalNameImpl(Cmp->getOperand(0), nullptr);
    std::string Op2 = getOriginalNameImpl(Cmp->getOperand(1), nullptr);
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

  std::string DiagnosticNameGenerator::getOriginalSelectName(const SelectInst *Select, DIType **FinalType) {
    return getOriginalNameImpl(Select->getCondition(), nullptr)
      + " ? " + getOriginalNameImpl(Select->getTrueValue(), nullptr)
      + " : " + getOriginalNameImpl(Select->getFalseValue(), nullptr);
  }

  std::string DiagnosticNameGenerator::getOriginalPhiName(const PHINode *PHI, DIType **FinalType) {
    // assume induction variable structure, and name after first DVI found
    DLOG("PHI: " << PHI);
    DLOG("*PHI: " << *PHI);
    if (CurrentPhis.count(PHI)) {
      DLOG("recursive phi call detected");
      return "{recursively-defined-structure}";
    }
    CurrentPhis.insert(PHI);
    auto [name, pred] = getOriginalInductionVariableName(PHI, FinalType);
    LLVM_DEBUG(errs() << "indvar name: " << name << "\n");
    if (pred < 0) {
      // Found no dbg variable intrinsic. Since we've explored recursively all
      // the way, we already know that the operands will not be of much use in
      // determining original name.
      name = "<unknown-phi %" + PHI->getName().str() + ">";
      goto cleanup;
    }

    if (!FinalType || !*FinalType) {
      goto cleanup;
    }
    {
      auto Res = calibrateDebugType(PHI->getType(), *FinalType);
      if (!Res.first) {
        // The phi might not be an induction variable since the type doesn't
        // match. We know which operand had the shortest distance to a name, so
        // use that to name normally. This has the advantage of adjusting the
        // FinalType at intermediate instructions that change the type, so we
        // have higher chance of a type match. We also know we will find a name,
        // and not loop around forever.
        *FinalType = nullptr;
        name = getOriginalNameImpl(PHI->getOperand(pred), FinalType);
        goto cleanup;
      }

      *FinalType = Res.second;
    }
  cleanup:
    CurrentPhis.erase(PHI);
    return name;
  }

  std::string DiagnosticNameGenerator::getOriginalAsmName(const InlineAsm *Asm,
                                    DIType **FinalType) {
    return "asm (" + Asm->getAsmString() + ")";
  }

  std::string DiagnosticNameGenerator::getOriginalReturnName(const ReturnInst *Return,
                                    DIType **FinalType) {
    return "return " + getOriginalNameImpl(Return->getReturnValue(), nullptr);
  }

  std::string DiagnosticNameGenerator::getOriginalBranchName(const BranchInst *Br, DIType **FinalType) {
    if (Br->isUnconditional()) {
      return "goto " + Br->getSuccessor(0)->getName().str();
    }
    return "if (" + getOriginalNameImpl(Br->getCondition(), nullptr) + ") goto " + Br->getSuccessor(0)->getName().str()
      + "; else goto " + Br->getSuccessor(1)->getName().str() + ";";
  }

  std::string DiagnosticNameGenerator::getOriginalBinOpName(const BinaryOperator* BO, DIType **FinalType) {
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
    return getOriginalNameImpl(BO->getOperand(0), nullptr)
      + " " + Name + " " +
      getOriginalNameImpl(BO->getOperand(1), nullptr);
  }
  std::string
  DiagnosticNameGenerator::getOriginalCastName(const CastInst *Cast,
                                               DIType **const FinalType) {
    if (auto BCast = dyn_cast<BitCastInst>(Cast)) {
      return getOriginalBitCastName(BCast, FinalType);
    }
    if (auto PCast = dyn_cast<IntToPtrInst>(Cast)) {
      auto Name = getOriginalNameImpl(PCast->getOperand(0), nullptr);
      if (FinalType) {
        // This search is unnecessary if we do not want the type to begin with
        DebugInfoFinder DIF;
        DIF.processModule(*M);
        for (auto DITy : DIF.types()) {
          if (compareValueTypeAndDebugType(PCast->getType(), DITy) == Match) {
            DLOG("found ditype for " << *PCast);
            DLOG("ditype: " << *DITy);
            *FinalType = DITy;
            break;
          }
        }
      }
      return Name;
    }
    auto OP = Cast->getOperand(0);
    // Most of the remaining cast instructions are numerical types, where Finaltype
    // does not matter much anyways.
    return getOriginalNameImpl(OP, nullptr);
  }

  std::string
  DiagnosticNameGenerator::getOriginalBitCastName(const BitCastInst *BC,
                                                  DIType **const FinalType) {
    DLOG("naming bitcast: " << *BC);
    const Value *OP = BC->getOperand(0);
    std::string name = getOriginalNameImpl(OP, FinalType);
    if (!(FinalType && *FinalType))
      return name;
    if (compareValueTypeAndDebugType(BC->getType(), *FinalType) == Match) {
      DLOG("BC didn't need calibration: " << *BC << " DIType: " << *FinalType);
      return name;
    }
    unsigned NestingDepth;
    if (isFirstFieldNestedValueType(OP->getType(), BC->getType(), NestingDepth)) {
      DLOG("narrowing cast");
      *FinalType = calibrateDebugType(BC->getType(), *FinalType).second;
      if (*FinalType) ++NumBitcastNarrowingSuccesses;
    } else if (isFirstFieldNestedValueType(BC->getType(), OP->getType(), NestingDepth)) {
      DLOG("widening cast");
      if (!M) {
        llvm_unreachable("no module!");
        *FinalType =
            nullptr; // it's better to return no type than an incorrect type
        return name;
      }
      // We want to find a DIType that contains (potentially transitively) the
      // DIType of OP, and also matches the value type of BC
      SmallVector<DIType *, 8> Users;
      SmallPtrSet<DIType *, 32> VisitedUsers;
      DIType *T = *FinalType;
      // If pointer, we want to get the subtype relationship of the base type
      if (auto Derived = dyn_cast<DIDerivedType>(trimNonPointerDerivedTypes(T)))
        T = Derived->getBaseType();
      if (!T) { // We're not getting anywhere with a void pointer
        *FinalType = nullptr;
        return name;
      }
      auto it = DITypeUsers.find(T);
      if (it != DITypeUsers.end()) {
        for (auto User : *it->getSecond()) {
          Users.push_back(User);
        }
      }
      // TODO: cap number of iterations to nesting distance if more performance
      // is needed
      *FinalType = nullptr;
      unsigned Depth = 0;
      while (!Users.empty() && Depth++ <= NestingDepth) {
        DLOG("iterating users...");
        SmallVector<DIType *, 8> PrevUsers;
        for (auto User : Users) {
          DLOG("User type: " << *User);
          if (VisitedUsers.count(User))
            continue;
          if (auto res = calibrateDebugType(BC->getType(), User); res.first) {
            *FinalType = res.second;
            DLOG("found matching type! ");
            // printDbgType(*FinalType);
            if (res.first == Match) {
              ++NumBitcastWidenSuccesses;
              return name;
            }
            // if matching includes incomplete types, keep looking for exact
            // match
          }
          VisitedUsers.insert(User);
          PrevUsers.push_back(User);
        }
        DLOG("round finished");
        Users.clear();
        for (auto User : PrevUsers) { // types of BC and OP can differ several
                                      // nesting levels
          auto it = DITypeUsers.find(User);
          if (it != DITypeUsers.end()) {
            for (auto NextUser : *it->getSecond()) {
              if (NextUser->getTag() == dwarf::DW_TAG_structure_type) {
                auto Comp = cast<DICompositeType>(NextUser);
                auto FirstElem = Comp->getElements()[0];
                // Subtype relationship will be relevant only if first struct field
                if (trimNonPointerDerivedTypes(cast<DIType>(FirstElem)) != User) continue;
              }
              Users.push_back(NextUser);
            }
          }
        }
      }
      DLOG("did not find matching type!");
    } else {
      DLOG("BC type:");
      LLVM_DEBUG(printValueType(BC->getType()));
      DLOG("OP type:");
      LLVM_DEBUG(printValueType(OP->getType()));
      DLOG("BB: " << *BC->getParent());
      if (auto I = dyn_cast<Instruction>(OP);
          I && I->getParent() != BC->getParent()) {
        DLOG("OP-BB: " << *I->getParent());
      }
      if (auto GEP = dyn_cast<GetElementPtrInst>(OP)) {
        DLOG("OP-OP: " << *GEP->getPointerOperand());
      }
      for (auto U : BC->users()) {
        DLOG("BC-user: " << *U);
      }
      SmallVector<std::pair<unsigned int, MDNode *>, 2> MDs;
      BC->getAllMetadata(MDs);
      for (auto MD : MDs) {
        DLOG("BC dbg: " << *MD.second);
        for (auto &MD2 : MD.second->operands()) {
          DLOG("\t" << *MD2.get());
        }
      }

      // llvm_unreachable("no relationship between before and after cast");
      // We could not find a simple subtype relationship before and after cast
      // The programmer knows something we don't
      *FinalType = nullptr;
    }
    ++NumBitcastFails;
    return name;
  }

  std::string DiagnosticNameGenerator::getOriginalInstructionName(
      const Instruction *const I, DIType **const FinalType) {
    DLOG("inst: " << *I);
    if (auto GEP = dyn_cast<GetElementPtrInst>(I)) {
      auto Name = getOriginalPointerName(GEP, FinalType);
      DLOG("left gep");
      return Name;
    }
    if (auto Cast = dyn_cast<CastInst>(I)) {
      return getOriginalCastName(Cast, FinalType);
    }
    if (auto Load = dyn_cast<LoadInst>(I)) {
      const Value *OP = Load->getOperand(0);
      DLOG("OP: " << *OP);
      std::string name = getOriginalNameImpl(OP, FinalType);
      if (!isa<GlobalVariable>(OP)) { // TODO: maybe advance FT even for GV?
        // In C syntax globals don't act like pointers, but they are in IR.
        // otherwise add dereference to name and type for loads.
        if (FinalType && *FinalType) {
          DLOG("advancing finaltype to base pointer");
          DLOG("finaltype: " << *FinalType);
          *FinalType = cast<DIDerivedType>(*FinalType)->getBaseType();
          DLOG("base:" << *FinalType);
        }
        // GEPs already return dereferenced name, so skip this if operand is a GEP
        if (isa<GetElementPtrInst>(OP)) return name;
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
    if (auto Return = dyn_cast<ReturnInst>(I))
      return getOriginalReturnName(Return, FinalType);
    if (auto Br = dyn_cast<BranchInst>(I)) {
      return getOriginalBranchName(Br, FinalType);
    };
    if (auto Alloc = dyn_cast<AllocaInst>(I)) {
      std::string TypeName;
      raw_string_ostream SS(TypeName);
      SS << Alloc->getType();
      if (FinalType) {
        // This search is unnecessary if we do not want the type to begin with
        // TODO: evaluate whether this full search is worth the time
        for (auto DITy : DIF.types()) {
          if (compareValueTypeAndDebugType(Alloc->getType(), DITy) == Match) {
            DLOG("found ditype for " << *Alloc);
            DLOG("ditype: " << *DITy);
            *FinalType = DITy;
            break;
          }
        }
      }
      return "alloca<" + TypeName + ">";
    }
    errs() << "unhandled instruction type: " << *I << "\n";
    return "";
  }

  std::string DiagnosticNameGenerator::getOriginalConstantName(const Constant *C,
                                      DIType **const FinalType) {
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
        return CDS->getAsString().str();
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
      if (CI->isNegative()) return std::to_string(CI->getSExtValue());
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
    if (auto GIS = dyn_cast<GlobalIndirectSymbol>(C)) {
      errs() << "GIS: " << *GIS << " base obj: " << *GIS->getBaseObject() << "\n";
      return GIS->getName().str();
    }
    if (auto GF = dyn_cast<Function>(C)) {
      return GF->getName().str();
    }
    if (auto GVar = dyn_cast<GlobalVariable>(C)) {
      DLOG("GVar: " << *GVar << " getname: " << GVar->getName()
           << " getvaluename: " << GVar->getValueName()->first());
      DLOG("baseobj: " << *GVar->getBaseObject());
      DLOG("global id: " << GVar->getGlobalIdentifier());
      DLOG("type: " << *GVar->getType());
      DLOG("var type: " << *GVar->getType()->getElementType());
      if (GVar->isConstant() && GVar->hasGlobalUnnamedAddr() && GVar->hasDefinitiveInitializer()) {
        // The address, and therefore the pointer name, is irrelevant in the source code,
        // so we only care about the value. This could be something like a string literal
        ModuleSlotTracker MST(GVar->getParent(),
                              /*ShouldInitializeAllMetadata*/ false);
        std::string name;
        raw_string_ostream OS(name);
        GVar->getInitializer()->printAsOperand(OS, /*isForDebug*/ false, MST);
        OS.flush();
        return name;
        return getOriginalNameImpl(GVar->getInitializer(), FinalType);
      }
      return GVar->getName().str();
    }
    errs() << "unhandled constant type: " << *C << "\n";
    return "";
  }

  /// Reconstruct the original name of a value from debug symbols. Output string
  /// is in C syntax no matter the source language. If FinalType is given, it is
  /// set to point to the DIType of the value, if it can be found.
  std::string DiagnosticNameGenerator::getOriginalNameImpl(const Value *V,
                                  DIType **const FinalType) {
    ++NumGONImpl;
    assert(V != nullptr);
    LLVM_DEBUG(errs() << "gON: " << *V << "\n");

    std::string Name = tryGetNameFromDbgValue(V, FinalType);
    if (!Name.empty()) { // This is the gold standard, it will tell us the actual source name
      goto ret;
    }

    if (auto I = dyn_cast<Instruction>(V)) {
      Name = getOriginalInstructionName(I, FinalType);
    } else if (auto C = dyn_cast<Constant>(V))
      Name = getOriginalConstantName(C, FinalType);
    else if (auto BB = dyn_cast<BasicBlock>(V)) {
      ++NumBB;
      Name = "BB{\n";
      Name += BB->getName();
      Name += ":\n";
      for (auto &I : *BB) {
        if (isa<DbgInfoIntrinsic>(I)) continue; // these intrinsics are essentially metadata, not code
        Name += getOriginalInstructionName(&I, nullptr);
        Name += "\n";
      }
      Name += "}";
    } else if (auto Arg = dyn_cast<Argument>(V)) {
      Name = Arg->getName().str();
    } else {
      errs() << "unhandled value type: " << *V << "\n";
      Name = ""; // TODO: fallback
      goto ret;
    }
    if (FinalType && *FinalType) {
      DLOG("calibrating type");
      DIType *T = calibrateDebugType(V->getType(), *FinalType).second;
      if (!T) {
        DLOG("mismatched types7 for " << *V << "\n");
        DLOG("type: " << *V->getType() << "\n");
        DLOG("dbg type ptr: " << *FinalType << "\n");
        DLOG("dbg type: " << **FinalType << "\n");
        LLVM_DEBUG(printDbgType(*FinalType));
        if (auto I = dyn_cast<Instruction>(V)) {
          auto F = I->getParent()->getParent();
          DLOG("function: " << *F);
        }
        llvm_unreachable("mismatched types7");
      } else {
        DLOG("matching types for " << *V << " T: " << *T);
        *FinalType = T;
      }
    }
  ret:
    if (FinalType) {
      ++NumDbgRequested;
      if (!*FinalType) ++NumNoDbgFound;
    }
    return Name;
  }
} // namespace llvm

/// Reconstruct the original name of a value from debug symbols. Output string is in C syntax no matter the source language. Will fail if not compiled with debug symbols.
/// TODO: Handle returning multiple aliasing names
std::string llvm::DiagnosticNameGenerator::getOriginalName(const Value* V) {
  errs().SetBufferSize(100000);
  ++NumGON;
  return getOriginalNameImpl(V, nullptr);
}

