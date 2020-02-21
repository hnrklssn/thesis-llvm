#ifndef LLVM_CLANG_SEMA_REMARK_HINT_H
#define LLVM_CLANG_SEMA_REMARK_HINT_H

//#include "clang/AST/Attrs.inc"
#include "clang/AST/ASTContext.h"
#include "clang/Basic/DiagnosticSema.h"
#include "clang/Basic/IdentifierTable.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/Sema/Ownership.h"
#include "clang/Sema/ParsedAttr.h"
#include "clang/Sema/SemaInternal.h"

namespace clang {
struct RemarkHint {
  SourceRange Range;
  IdentifierLoc *PragmaNameLoc;
  IdentifierLoc *OptionLoc;
  SmallVector<IdentifierLoc *, 2> ValueLocs;

  RemarkHint()
    : Range(), PragmaNameLoc(nullptr), OptionLoc(nullptr), ValueLocs()  {}
};

RemarkAttr *handleRemarkAttr2(Sema &S, const ParsedAttr &AL);

} // namespace clang
#endif // LLVM_CLANG_SEMA_REMARK_HINT_H
