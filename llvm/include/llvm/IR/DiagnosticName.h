
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

#include "llvm/IR/Value.h"
#include <string>

namespace llvm {

  /// Reconstruct the original name of a value from debug symbols.
  /// Output string is in C syntax no matter the source language.
  /// Will fail if input is not compiled with debug symbols (-g with Clang).
  std::string getOriginalName(const Value* V);

}
#endif // LLVM_IR_DIAGNOSTICNAME_H
