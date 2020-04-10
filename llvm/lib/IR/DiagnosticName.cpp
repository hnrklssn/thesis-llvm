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
#include "llvm/IR/Module.h"
#include "llvm/IR/ModuleSlotTracker.h"
#include "llvm/IR/Use.h"
#include "llvm/IR/User.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"
#include <bits/stdint-uintn.h>
#include <deque>
#include <sstream>
#include <string>
#include <utility>

#define DEBUG_TYPE "diagnostic-name"
#define DLOG(args) LLVM_DEBUG(errs() << "[" /*<< __FILE__ << ":"*/ << __LINE__ << "] "<< args << "\n")

namespace llvm {
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
  if (auto SeqTy = dyn_cast<SequentialType>(T)) {
    printValueType(SeqTy->getElementType(), indent + 1);
  }
}

static void printDbgType(const DIType *T, unsigned int indent = 0) {
  printIndent(indent);
  if (indent > 7) {
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
      if (i > 3) {
        printIndent(indent+1);
        errs() << "[rest of fields omitted]\n";
        break;
      }
      printDbgType(cast<DIType>(Comp->getElements()[i]), indent + 1);
    }
  }
}
  // TODO: Cache def-use chains if needed performance-wise.
static void findAllDITypeUses(const DIType *T, SmallVectorImpl<DIType *> &Users,
                              const Module &M) {
  DebugInfoFinder DIF;
  DLOG("looking for uses of " << *T);
  DIF.processModule(M);
  for (auto User : DIF.types()) {
    DLOG("USER: " << *User);
    if (auto Comp = dyn_cast<DICompositeType>(User)) {
      for (auto Elem : Comp->getElements()) {
        if (!Elem)
          continue;
        DLOG("ELEM: " << *Elem);
        if (Elem == T) {
          DLOG("found use!");
          Users.push_back(cast<DIType>(User));
          break;
        }
      }
    } else if (auto Derived = dyn_cast<DIDerivedType>(User)) {
      if (Derived->getBaseType()) {
        DLOG("BASE: " << *Derived->getBaseType());
      } else {
        DLOG("BASE: nullptr");
      }
      if (Derived->getBaseType() == T) {
        DLOG("found use!");
        Users.push_back(Derived);
      }
    }
  }
}

static bool isFirstFieldNestedValueTypeOrEqual(const Type *Outer, const Type *Inner) {
  DLOG("isNestedOrEqual? " << *Outer << " vs " << *Inner);
  if (Outer == Inner) return true;
  if (auto StructTy = dyn_cast<StructType>(Outer)) {
    return isFirstFieldNestedValueTypeOrEqual(StructTy->getElementType(0), Inner);
  }
  if (auto SeqTy = dyn_cast<SequentialType>(Outer)) {
    return isFirstFieldNestedValueTypeOrEqual(SeqTy->getElementType(),
                                              Inner);
  }
  return false;
}

static bool isFirstFieldNestedValueType(const Type *Outer, const Type *Inner) {
  DLOG("isNested? " << *Outer << " vs " << *Inner);
  while(Outer->isPointerTy() && Inner->isPointerTy()) {
    Outer = cast<PointerType>(Outer)->getElementType();
    Inner = cast<PointerType>(Inner)->getElementType();
  }
  return Outer != Inner && isFirstFieldNestedValueTypeOrEqual(Outer, Inner);
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
    : M(M), Builder(B) {}

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
      /*DLOG(__FUNCTION__ << "\n"
                        << "Ty: " << *Ty << "\n"
                        << "DITy: " << *DITy);*/

      // These types are abstractions that don't exist in IR types
      if (DITy->getTag() == dwarf::DW_TAG_typedef ||
          DITy->getTag() == dwarf::DW_TAG_member ||
          DITy->getTag() == dwarf::DW_TAG_const_type)
        return compareValueTypeAndDebugTypeInternal(
            Ty, cast<DIDerivedType>(DITy)->getBaseType(),
            EquivalentStructTypes);

      TypeCompareResult res = Match;
      if (DITy->getTag() == dwarf::DW_TAG_union_type) {
        auto DIUnionTy = cast<DICompositeType>(DITy);
        if (auto StructTy = dyn_cast<StructType>(Ty)) {
          // there is no union type in LLVM, just a struct of the largest element size
          assert(StructTy->getNumElements() == 1);
          auto Elem = StructTy->getElementType(0);
          for (auto DIUnionElem : DIUnionTy->getElements()) {
            if (compareValueTypeAndDebugTypeInternal(Elem, cast<DIType>(DIUnionElem), EquivalentStructTypes) == Match)
              return Match;
          }
          return IncompleteTypeMatch;
          // Unions aren't very typesafe, just hope that this is struct field that's not needed right now
          // TODO: look into supporting union bitcast pattern
        }
        DLOG("union type: " << *DIUnionTy);
        printValueType(Ty);
        printDbgType(DIUnionTy);
        llvm_unreachable("union type!");
      }
      if (auto StructTy = dyn_cast<StructType>(Ty)) {
        if (StructTy->getNumElements() == 0)
          return IncompleteTypeMatch; // essentially void type afaict
        if (DITy->getTag() != dwarf::DW_TAG_structure_type) {
          LLVM_DEBUG(errs() << "value type is struct, but debug type is "
                            << *DITy << "\n");
          return NoMatch;
        }
        auto DIStructTy = cast<DICompositeType>(DITy);
        const auto PrevResult = EquivalentStructTypes.find(DIStructTy);
        if (PrevResult != EquivalentStructTypes.end()) {
          if (PrevResult->second == StructTy) {
            return Match; // We have looped back around and not found
                          // any contradictions
          }
          return NoMatch;
        }
        EquivalentStructTypes.insert(std::make_pair(DIStructTy, StructTy));
        if (StructTy->getNumElements() !=
            DIStructTy->getElements()->getNumOperands()) {
          LLVM_DEBUG(errs() << "value type is struct with "
                            << StructTy->getNumElements() << " elements, "
                            << " but debug type is struct with "
                            << DIStructTy->getElements()->getNumOperands()
                            << " elements\n");
          return NoMatch;
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
          }
        }
      } else if (auto PtrTy = dyn_cast<PointerType>(Ty)) {
        switch (DITy->getTag()) {
        case dwarf::DW_TAG_pointer_type: {
          auto DIPtrTy = cast<DIDerivedType>(DITy);
          if (!DIPtrTy->getBaseType())
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
             array fields in structs. So DW_TAG_array_type matches
             both pointer types and sequential types.
          */
          if (!isa<SequentialType>(PtrTy->getElementType())) {
            DLOG("dity is array, but ty does not point to seq");
            return NoMatch;
          }
          auto SeqTy = cast<SequentialType>(PtrTy->getElementType());
          res = compareValueTypeAndDebugTypeInternal(
                             SeqTy, DITy,
                             EquivalentStructTypes);
        }; break;
        default:
          LLVM_DEBUG(errs() << "value type is pointer, but debug type is "
                            << *DITy << "\n");
          return NoMatch;
        }
      } else if (auto IntTy = dyn_cast<IntegerType>(Ty)) {
        DLOG("IntTy");
        if (!isa<DIBasicType>(DITy)) {
          DLOG("DITy not matching integer: " << *DITy);
          return NoMatch;
        }
        unsigned IntSize = IntTy->getIntegerBitWidth();
        auto Basic = cast<DIBasicType>(DITy);
        res = Basic->getSizeInBits() == IntSize ? Match : NoMatch;
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

            res = std::min(res, compareValueTypeAndDebugTypeInternal(
                                    FuncTy->getParamType(i),
                                    cast<DIType>(DITypes[i + 1]),
                                    EquivalentStructTypes));
            if (!res) {
              DLOG("param type mismatch: " << i);
            }
          }
        }
      } else if (auto SeqTy = dyn_cast<SequentialType>(Ty)) {
        DLOG("seqty: " << *SeqTy);
        DLOG("dity: " << *DITy);
        switch(DITy->getTag()) {
        default:
          llvm_unreachable("unexpected dity tag for seq");
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
    if (DITy->getTag() == dwarf::DW_TAG_member) return NoMatch;
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
    while (res = compareValueTypeAndDebugType(Ty, DITy), DITy && !res) {
      LLVM_DEBUG(errs() << __FUNCTION__ << "\n");
      DLOG("res: " << res);
      if (res) {
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
      if (auto Comp = dyn_cast<DICompositeType>(DITy)) {
        //LLVM_DEBUG(errs() << "composite type: " << *Comp << "\n");
        DITy = cast<DIType>(Comp->getElements()[0]);
        //LLVM_DEBUG(errs() << "first elem: " << *DITy << "\n");
      } else if (auto Derived = dyn_cast<DIDerivedType>(DITy)) {
        //LLVM_DEBUG(errs() << "derived type: " << *Derived << "\n");
        DITy = Derived->getBaseType();
        //LLVM_DEBUG(errs() << "base type: " << *DITy << "\n");
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
std::string llvm::DiagnosticNameGenerator::getFragmentTypeName(DIType *const T, int64_t Offset,
                                  DIType **const FinalType,
                                  std::string Sep/* = "."*/) {
    assert(T && "fragment type null");
    if (!T) return "";
    if (auto Comp = dyn_cast<DICompositeType>(T)) {
      DIDerivedType *tmpT = nullptr;
      for (auto E : Comp->getElements()) {
        //LLVM_DEBUG(E->dump());
        if(auto E2 = dyn_cast<DIDerivedType>(E)) {
          int64_t O2 = E2->getOffsetInBits();
          if (O2 > Offset) {
            break;
          }
          tmpT = E2;
        } else if (Comp->getTag() == dwarf::DW_TAG_enumeration_type) {
          auto E2 = cast<DIEnumerator>(E);
          int64_t O2 = E2->getValue();
          if (O2 > Offset) {
            std::string res = Sep + E2->getName().str(); // TODO @henrik: test enums
            //DLOG("enum res: " << res);
            return res;
          }
        } else {
          DLOG("T: " << *T);
          for (auto E3 : Comp->getElements()) {
            DLOG("E3: " << *E3);
          }
          DLOG("E: " << *E);
          llvm_unreachable("Non derived type as struct member");
        }
      }
      if (!tmpT)
        llvm_unreachable("no-elements-in-struct?");
      return Sep + tmpT->getName().str() + getFragmentTypeName(tmpT, Offset - tmpT->getOffsetInBits(), FinalType);
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
  /// Get rest of name based on the type of the base value, and the offset.
  std::string DiagnosticNameGenerator::getFragmentTypeName(DIType *T, const int64_t *Offsets_begin, const int64_t *Offsets_end, DIType **FinalType, std::string Sep/* = "."*/) {
    int64_t Offset = -1;
    if (!T) return "fragment-type-null";
    LLVM_DEBUG(errs() << "getFragmentTypeName: " << *T << "\n");
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
      //LLVM_DEBUG(errs() << "offsets_begin == offsets_end \n");
    } else {
      Offset = *Offsets_begin;
    }
    // derived non pointer types don't count, so we still haven't reached the final type if we encounter one of those
    if (Offsets_begin == Offsets_end  && (!isa<DIDerivedType>(T) || T->getTag() == dwarf::DW_TAG_pointer_type)) {
      if(FinalType) *FinalType = T;
      return "";
    }
    //LLVM_DEBUG(errs() << "getFragmentTypeName offset: " << Offset << "\n");
    if (auto Comp = dyn_cast<DICompositeType>(T)) {
      auto elements = Comp->getElements();
      if (Comp->getTag() == dwarf::DW_TAG_array_type) {
        if (auto Subr = dyn_cast<DISubrange>(elements[0])) {
          if (auto CI = Subr->getCount().dyn_cast<ConstantInt*>()) {
            if (Offset >= CI->getSExtValue()) {
              errs() << "Offset: " << Offset << "\n";
              errs() << "subr.getCount(): " << CI << "\n";
              errs() << "subr: " << *Subr << "\n";
              errs() << "elements: " << *Comp->getElements() << "\n";
              errs() << "Comp: " << *Comp << "\n";
              llvm_unreachable("not-enough-elements-in-array?");
            }
          }
          return "[" + std::to_string(Offset) + "]" +
                 getFragmentTypeName(Comp->getBaseType(), Offsets_begin + 1,
                                     Offsets_end, FinalType);
        } else {
          errs() << "Offset: " << Offset << "\n";
          errs() << "elements.size(): " << elements.size() << "\n";
          errs() << "elements: " << *elements << "\n";
          errs() << "Comp: " << *Comp << "\n";
          for (auto elem : elements) {
            errs() << "elem: " << *elem << "\n";
          }
          llvm_unreachable("no subrange in array type");
        }
      }
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
      if(auto NextT = dyn_cast<DIDerivedType>(elements[Offset])) {
        //LLVM_DEBUG(errs() << "nextT: " << *NextT << "\n");
        return Sep + NextT->getName().str() + getFragmentTypeName(NextT, Offsets_begin + 1, Offsets_end, FinalType);
      } else {
        errs() << "non derived struct member: " << elements[Offset] << "\n";
        errs() << "struct: " << Comp << "\n";
        llvm_unreachable("Non derived type as struct member? Should not happen");
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
      return Val->getName();
    }
    int64_t Offset = -1;
    if (Expr->extractIfOffset(Offset)) { // FIXME extractIfOffset seems broken. Workaround below for now.
      llvm_unreachable("extractIfOffset works?");
      if (FinalType) *FinalType = Type;
      return Val->getName();
    }

    Optional<DIExpression::FragmentInfo> FIO = Expr->getFragmentInfo();
    if(FIO) {
      Offset = FIO->OffsetInBits;
    }
    //LLVM_DEBUG(errs() << "original offset " << Offset << " num " << Expr->getNumElements() << " " << FIO->SizeInBits << " " << FIO->OffsetInBits << "\n");
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
      auto Name = getFragmentTypeName(Comp, Offset, FinalType);
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
        llvm_unreachable("mismatched types1");
      } else {
        // LLVM_DEBUG(errs() << "matching types for " << *V << "\n");
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

  /// A Value can have multiple debug non-identical debug intrinsics due to inlining
  /// and potentially also due to aliasing in general.
  /// TODO: Handle returning multiple aliasing names. Try closest dominance of an optional secondary value.
  DbgVariableIntrinsic* DiagnosticNameGenerator::getSingleDbgUser(const Value *V) {
    SmallVector<DbgVariableIntrinsic *, 4> DbgValues;
    testFindDbgUsers(DbgValues, V); // TODO upstream fix
    //findDbgUsers(DbgValues, V);
    DbgVariableIntrinsic * DVI = nullptr;
    for (auto &VI : DbgValues) {
      //DLOG("VI: " << *VI);
      //LLVM_DEBUG(printIntrinsic(VI));
      if (DVI && VI->isIdenticalTo(DVI)) continue; // intrinsics can be duplicated
      DVI = VI;
    }
    return DVI;
  }

  // TODO check which string type is appropriate here
  /// Helper function for naming a pointer relative to some other base pointer
  std::string DiagnosticNameGenerator::getOriginalRelativePointerName(const Value *V, StringRef ArrayIdx, SmallVectorImpl<int64_t> &StructIndices, DIType **FinalType) {
    // Use the value V to name this pointer prefix, and make it return metadata for accessing debug symbols for our pointer
    DIType *T = nullptr;
    LLVM_DEBUG(errs() << "getting pointer prefix for " << *V << "\n");
    std::string ptrName;
    if (auto LI = dyn_cast<LoadInst>(V))
      ptrName = getOriginalNameImpl(LI->getPointerOperand(), &T);
    else
      ptrName = getOriginalNameImpl(V, &T);
    DLOG("prefix: " << ptrName << " arrayidx: " << ArrayIdx);
    if (!T) return ptrName + "->{unknownField}";
    /*if (!(T = calibrateDebugType(V->getType(), T))) {

      LLVM_DEBUG(errs() << "mismatched types2 for " << *V << "\n");
      LLVM_DEBUG(errs() << "type: " << *V->getType() << "\n");
      LLVM_DEBUG(errs() << "dbg type: " << *T<< "\n");
      llvm_unreachable("mismatched types2");
    } else {
      LLVM_DEBUG(errs() << "matching types for " << *V << "\n");
      }*/
    auto PT1 = cast<DIDerivedType>(T);
    DLOG("GEP: " << *V);
    DLOG("PT1: " << *PT1);
    DLOG("basetype: " << *PT1->getBaseType());
    T = PT1->getBaseType();
    if (auto PT = dyn_cast<DIDerivedType>(T); PT && PT->getBaseType()) {
      DLOG("GEP: " << *V);
      DLOG("PT: " << *PT);
      std::string Sep = ".";
      if (ArrayIdx.empty() && PT->getTag() == dwarf::DW_TAG_pointer_type) Sep = "->"; // if explicit array indexing, dereference is already done
      return ptrName + ArrayIdx.str() + getFragmentTypeName(PT->getBaseType(), StructIndices.begin(), StructIndices.end(), FinalType, Sep);
    }
    DLOG("non ptr type: " << *T);
    return ptrName + ArrayIdx.str() + getFragmentTypeName(T, StructIndices.begin(), StructIndices.end(), FinalType);
  }

  std::string DiagnosticNameGenerator::getFragmentNameNoDbg(const Value *V, const Use *idx_begin, const Use *idx_end) {
    std::string name = "";
    Type *Ty = V->getType();
    for (auto itr = idx_begin; itr < idx_end; itr++) {
      auto Idx = itr->get();
      auto IdxName = getOriginalNameImpl(Idx, nullptr);
      //DLOG("Ty: " << *Ty);

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
      } else if (auto SeqTy = dyn_cast<SequentialType>(Ty)) {
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
        DLOG("Idx: " << *Idx);
        DLOG("OPtype: " << *V->getType());
        DLOG("name: " << name);
        llvm_unreachable("unexpected type");
      }
    }
    //DLOG("name: " << name);
    return name;
  }

  // TODO: look into simplifying this, it has grown organically when encountering more and more cases
  std::string DiagnosticNameGenerator::getOriginalPointerName(const GetElementPtrInst *const GEP, DIType **const FinalType) {
    LLVM_DEBUG(errs() << "GEP!\n");
    LLVM_DEBUG(GEP->dump());
    auto OP = GEP->getPointerOperand();
    getFragmentNameNoDbg(OP, GEP->idx_begin(), GEP->idx_end());
    DLOG("operand: " << *OP);
    std::string ArrayIdx = "";
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
    const Use *idx_last_const = getConstOffsets(GEP->idx_begin() + 1, GEP->idx_end(), Indices); // skip first offset, it doesn't matter if it's constant
    DIType *T = nullptr;
    if (auto LI = dyn_cast<LoadInst>(OP)) {
      // This means our pointer originated in the dereference of another pointer
      DLOG("getting relative ptr");
      name += getOriginalRelativePointerName(LI, ArrayIdx, Indices, &T);
      DLOG("back from load: " << *LI << " GEP: " << *GEP);
      if (!T) DLOG("T == nullptr");
    } else if (auto GEP2 = dyn_cast<GetElementPtrInst>(OP)) {
      // This means our pointer is some linear offset of another pointer, e.g. a subfield of a struct, relative to the pointer to the base of the struct
      DLOG("getting relative ptr");
      name += getOriginalRelativePointerName(GEP2, ArrayIdx, Indices, &T);
      /*} else if (auto Global = dyn_cast<GlobalValue>(OP)) { // TODO: test this
      if (Global->hasAtLeastLocalUnnamedAddr()) {
        auto name = getOriginalNameImpl(Global, &T);
        if (T) {
          errs() << "ditype from global: " << *T << "\n";
        }
        errs() << "global GEP name: " << name << "\n";
        //return name;
        }*/
      DLOG("back from inner GEP: " << *GEP2 << " outer GEP: " << *GEP);
      /*} else if (DbgVariableIntrinsic *DVI = getSingleDbgUser(OP)) {
      // Base pointer is just a variable, fetch its debug info
      DLOG("DVI: " << *DVI);

      if(auto Val = dyn_cast<DbgValueInst>(DVI)) {
        auto Var = Val->getVariable();
        DLOG("var: " << *Var);
        auto Type = Var->getType();
        LLVM_DEBUG(errs() << "dbg type1: " << *Type << "\n");
        if (!compareValueTypeAndDebugType(OP->getType(), Type)) {
          DLOG("mismatched types3 for " << *OP);
          DLOG("type: " << *OP->getType());
          DLOG("dbg type2: " << *Type);
          DLOG("\n");
          printValueType(OP->getType());
          LLVM_DEBUG(errs() << "\n\n");
          printDbgType(Type);
          llvm_unreachable("mismatched types3");
        } else {
          LLVM_DEBUG(errs() << "matching types for " << *OP << "\n");
        }
        auto BaseType = cast<DIDerivedType>(Type)->getBaseType();
        std::string Sep = ".";
        if (ArrayIdx.empty()) Sep = "->";
        DLOG("getting fragment type");
        name += Var->getName().str() + ArrayIdx + getFragmentTypeName(BaseType, Indices.begin(), Indices.end(), &T, Sep);
      } else if(auto Decl = dyn_cast<DbgDeclareInst>(DVI)) {
        auto Var = Decl->getVariable();
        auto Type = Var->getType();
        DLOG("calibrating type");
        if (!(Type = calibrateDebugType(OP->getType(), Type).second)) {
          LLVM_DEBUG(errs() << "mismatched types4 for " << *OP << "\n");
          LLVM_DEBUG(errs() << "type: " << *OP->getType() << "\n");
          LLVM_DEBUG(errs() << "dbg type: " << *Var->getType() << "\n");
          llvm_unreachable("mismatched types4");
        } else {
          LLVM_DEBUG(errs() << "matching types for " << *OP << "\n");
        }
        auto BaseType = cast<DIDerivedType>(Type)->getBaseType();
        DLOG("getting fragment type");
        name += Var->getName().str() + ArrayIdx + getFragmentTypeName(BaseType, Indices.begin(), Indices.end(), &T);
      } else {
        llvm_unreachable("unknown-dbg-variable-intrinsic");
        }*/
    } else {
      DLOG("fallback attempt");
      name = getOriginalNameImpl(OP, &T);
      if (!T) {
        // The code was compiled without debug info, or was optimised to the
        // point where it's no longer accessible. Give best effort indexing.
        DLOG("getting fragment type");
        return name + getFragmentNameNoDbg(OP, GEP->idx_begin(), GEP->idx_end());
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
      auto BaseType = T->getTag() == dwarf::DW_TAG_array_type
                          ? T
                          : cast<DIDerivedType>(T)->getBaseType();
      T = nullptr;
      DLOG("getting fragment type");
      name += ArrayIdx + getFragmentTypeName(BaseType, Indices.begin(), Indices.end(), &T);
    }
    DLOG("back in GEP: " << *GEP << " OP: " << *OP);
    if (T) DLOG("T: " << *T);
    else DLOG("T == null");

    /*if (idx_last_const < GEP->idx_end() && T) {
      // We have already handled the offset of the pointer type,
      // skip it so the next offset indexes into the inner type
      T = cast<DIDerivedType>(T)->getBaseType();
      }*/
    // in case not all offsets (excluding the first one) were constant.
    // this can happen for example with arrays in structs
    while(idx_last_const < GEP->idx_end() && T) {
      DLOG("naming non const idx: " << *idx_last_const->get());
      name += "[" + getOriginalNameImpl(idx_last_const->get(), nullptr) + "]";

      if (auto Derived = dyn_cast<DIDerivedType>(T)) T = Derived->getBaseType();
      else if (auto Composite = dyn_cast<DICompositeType>(T)) T = Composite->getBaseType();
      else {
        errs() << "unhandled ditype: " << *T << "\n";
        llvm_unreachable("Unhandled DIType");
      }

      Indices.clear(); // collect potential remaining constant indices
      idx_last_const = getConstOffsets(idx_last_const + 1, GEP->idx_end(), Indices);

      DIType *T2 = nullptr; // avoid aliasing T
      name += getFragmentTypeName(T, Indices.begin(), Indices.end(), &T2);
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
    for (uint32_t i = 0; i < U->getNumOperands(); i++) {
      Queue.push_back(std::make_pair(U->getOperand(i), (int32_t)i));
    }
    while(!Queue.empty()) {
      const auto [V, pred] = Queue.front();
      Queue.pop_front();
      Visited.insert(V);
      const DbgVariableIntrinsic * DVI = getSingleDbgUser(V);
      if (DVI) {
        return std::make_pair(getNameFromDbgVariableIntrinsic(DVI, FinalType), pred);
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
    DLOG("call: " << *Call);
    std::string FuncName = Call->getName().str() + " (fallback name)";
    auto CalledFunc = Call->getCalledFunction();
    if (CalledFunc) {
      FuncName = CalledFunc->getName();
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
    auto [name, pred] = getOriginalInductionVariableName(PHI, FinalType);
    LLVM_DEBUG(errs() << "indvar name: " << name << "\n");
    if (pred < 0) {
      // Found no dbg variable intrinsic. Since we've explored recursively all
      // the way, we already know that the operands will not be of much use in
      // determining original name.
      return "<unknown-phi %" + PHI->getName().str() + ">";
    }

    if (!FinalType || !*FinalType) {
      return name;
    }
    auto Res = calibrateDebugType(PHI->getType(), *FinalType);
    if (!Res.first) {
      // The phi might not be an induction variable since the type doesn't match.
      // We know which operand had the shortest distance to a name, so use that
      // to name normally. This has the advantage of adjusting the FinalType at
      // intermediate instructions that change the type, so we have higher chance
      // of a type match. We also know we will find a name, and not loop around
      // forever.
      *FinalType = nullptr;
      return getOriginalNameImpl(PHI->getOperand(pred), FinalType);
    }

    *FinalType = Res.second;
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

  std::string DiagnosticNameGenerator::getOriginalBitCastName(const BitCastInst *BC, DIType **const FinalType) {
    DLOG("naming bitcast: " << *BC);
    const Value *OP = BC->getOperand(0);
    std::string name = getOriginalNameImpl(OP, FinalType);
    if (!(FinalType && *FinalType)) return name;
    if (compareValueTypeAndDebugType(BC->getType(), *FinalType) == Match) {
      DLOG("BC didn't need calibration: " << *BC << " DIType: " << *FinalType);
      return name;
    }
    if (isFirstFieldNestedValueType(OP->getType(), BC->getType())) {
      DLOG("narrowing cast");
      *FinalType = calibrateDebugType(BC->getType(), *FinalType).second;
    } else if (isFirstFieldNestedValueType(BC->getType(), OP->getType())) {
      DLOG("widening cast");
      if (!M) {
        llvm_unreachable("no module!");
        *FinalType = nullptr; // it's better to return no type than an incorrect type
        return name;
      }
      // We want to find a DIType that contains (potentially transitively) the DIType of OP,
      // and also matches the value type of BC
      SmallVector<DIType*, 4> Users;
      SmallPtrSet<DIType*, 8> PrevUsers;
      DIType *T = *FinalType;
      if (auto Derived = dyn_cast<DIDerivedType>(T)) T = Derived->getBaseType();
      findAllDITypeUses(T, Users, *M);
      // TODO: cap number of iterations to nesting distance if more performance
      // is needed
      *FinalType = nullptr;
      while (!Users.empty()) {
        DLOG("iterating users...");
        for (auto User : Users) {
          DLOG("User type: " << *User);
          if (PrevUsers.count(User)) continue;
          if (auto res = calibrateDebugType(BC->getType(), User); res.first) {
            *FinalType = res.second;
            DLOG("found matching type! ");
            //printDbgType(*FinalType);
            if (res.first == Match) return name;
            // if matching includes incomplete types, keep looking for exact match
          }
        }
        DLOG("round finished");
        SmallVector<DIType *, 4> PrevUsers;
        PrevUsers.swap(Users);
        for (auto User : PrevUsers) { // types of BC and OP can differ several nesting levels
          findAllDITypeUses(User, Users, *M);
        }
      }
      DLOG("did not find matching type!");
    } else {
      DLOG("BC type:");
      printValueType(BC->getType());
      DLOG("OP type:");
      printValueType(OP->getType());
      DLOG("BB: " << *BC->getParent());
      if (auto I = dyn_cast<Instruction>(OP); I && I->getParent() != BC->getParent()) {
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

      //llvm_unreachable("no relationship between before and after cast");
      // We could not find a simple subtype relationship before and after cast
      // The programmer knows something we don't
      *FinalType = nullptr;
    }
    return name;
  }

  std::string DiagnosticNameGenerator::getOriginalInstructionName(const Instruction *const I,
                                         DIType **const FinalType) {
    if (auto GEP = dyn_cast<GetElementPtrInst>(I)) {
      auto Name = getOriginalPointerName(GEP, FinalType);
      DLOG("left gep");
      return Name;
    }
    if (auto BCast = dyn_cast<BitCastInst>(I)) {
      return getOriginalBitCastName(BCast, FinalType);
    }
    if (auto UI = dyn_cast<UnaryInstruction>(I)) {
      DLOG("unary: " << UI);
      // Most unary instructions don't  really alter the value that much
      // so our default here is to just use the name of the operand.
      // Small exception is bitcast which is handled above because it can destroy DITypes.
      const Value *OP = UI->getOperand(0);
      std::string name = getOriginalNameImpl(OP, FinalType); // TODO Figure out if this could ever cause an infinite loop in welformed programs. My guess is no.
      if (isa<LoadInst>(UI) && !isa<GlobalVariable>(OP)) { // TODO: maybe advance FT even for GV?
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
      return GIS->getName();
    }
    if (auto GF = dyn_cast<Function>(C)) {
      return GF->getName();
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
      return GVar->getName();
    }
    errs() << "unhandled constant type: " << *C << "\n";
    return "";
  }

  /// Reconstruct the original name of a value from debug symbols. Output string
  /// is in C syntax no matter the source language. If FinalType is given, it is
  /// set to point to the DIType of the value EXCEPT if the value is a
  /// GetElementPtrInst, where it will return the DIType of the value when
  /// dereferenced.
  std::string DiagnosticNameGenerator::getOriginalNameImpl(const Value *V,
                                  DIType **const FinalType) {
    assert(V != nullptr);
    LLVM_DEBUG(errs() << "gON: " << *V << "\n");

    DbgVariableIntrinsic * DVI = getSingleDbgUser(V);
    if (DVI) { // This is the gold standard, it will tell us the actual source name
      DLOG("found dvi for: " << *V);
      auto Name = getNameFromDbgVariableIntrinsic(DVI, FinalType);
      // hypothesis: most diffs here
      if (FinalType && *FinalType) {
        DLOG("calibrating type");
        DIType *T = calibrateDebugType(V->getType(), *FinalType).second;
        if (!T) {
          DLOG("mismatched types8 for " << *V << "\n");
          DLOG("type: " << *V->getType() << "\n");
          DLOG("dbg type ptr: " << *FinalType << "\n");
          DLOG("dbg type: " << **FinalType << "\n");
          LLVM_DEBUG(printDbgType(*FinalType));
          if (auto I = dyn_cast<Instruction>(V)) {
            auto F = I->getParent()->getParent();
            DLOG("function: " << *F);
          }
          llvm_unreachable("mismatched types8");
        } else {
          DLOG("matching types for " << *V << " T: " << *T);
          *FinalType = T;
        }
      }
      return Name;
    }
    //LLVM_DEBUG(errs() << "gON: no DVI" << *V << "\n");

    std::string Name;
    if (auto I = dyn_cast<Instruction>(V)) {
      Name = getOriginalInstructionName(I, FinalType);
    } else if (auto C = dyn_cast<Constant>(V))
      Name = getOriginalConstantName(C, FinalType);
    else if (auto BB = dyn_cast<BasicBlock>(V)) {
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
      Name = Arg->getName();
    } else {
      errs() << "unhandled value type: " << *V << "\n";
      return "";
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
    return Name;
  }
} // namespace llvm

/// Reconstruct the original name of a value from debug symbols. Output string is in C syntax no matter the source language. Will fail if not compiled with debug symbols.
/// TODO: Handle returning multiple aliasing names
std::string llvm::DiagnosticNameGenerator::getOriginalName(const Value* V) {
  errs().SetBufferSize(100000);
  return getOriginalNameImpl(V, nullptr);
}

