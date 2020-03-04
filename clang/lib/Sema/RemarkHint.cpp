#include "clang/Sema/RemarkHint.h"
#include "clang/AST/Attrs.inc"
#include "clang/Basic/DiagnosticDriver.h"
#include "llvm/ADT/StringRef.h"
//#include "clang/AST/ASTContext.h"
//#include "clang/Sema/SemaInternal.h"
//#include "clang/AST/Attrs.inc"
//#include "clang/Basic/DiagnosticSema.h"
//#include "clang/Sema/ParsedAttr.h"
//#include "clang/Sema/Sema.h"
//#include "llvm/Support/raw_ostream.h"

using namespace clang;
//using namespace sema;
namespace clang {
  RemarkAttr *handleRemarkAttr(Sema &S, const ParsedAttr &AL) {
  llvm::errs() << __FUNCTION__ << "\n";
  llvm::errs() << AL.getAttrName()->getName() << "\n";
  llvm::errs() << AL.getNumArgs() << "\n";
  if (!AL.getNumArgs()) {
    llvm::errs() << "no args\n";
  }
  llvm::errs() << __FUNCTION__ << "1\n";
  if (!AL.isArgIdent(0)) {
    S.Diag(AL.getLoc(), diag::err_attribute_argument_n_type)
        << AL << 1 << AANT_ArgumentIdentifier;
    return nullptr;
  }


  llvm::errs() << __FUNCTION__ << "2\n";
  IdentifierLoc *arg = AL.getArgAsIdent(0);
  if (!arg) {
    return nullptr;
  }
  IdentifierInfo *Ident = arg->Ident;
  if (!Ident) {
    return nullptr;
  }
  StringRef OptStr = Ident->getName();
  RemarkAttr::OptionType Opt;
  if (OptStr.equals("funct")) {
    Opt = RemarkAttr::Funct;
  } else if (OptStr.equals("loop")) {
    Opt = RemarkAttr::Loop;
  } else if (OptStr.equals("file")) {
    Opt = RemarkAttr::File;
  } else if (OptStr.equals("conf")) {
    Opt = RemarkAttr::Conf;
  } else {
    /*S.Diag(AL.getArgAsIdent(0)->Loc, diag::err_attribute_argument_n_type)
      << Opt << ExpectedFunction;*/
    llvm::errs() << __FUNCTION__ << " " << OptStr << " 2.5\n"; // TODO: diagnostics
    return nullptr;
  }

  if (Opt == RemarkAttr::Conf && AL.getNumArgs() > 3) {
    llvm::errs() << __FUNCTION__ << " too many args for conf: expected 1 or 2, got " << AL.getNumArgs() - 1
                 << "\n"; // TODO: diagnostics
  }

  SmallVector<StringRef, 2> Vals;

  for (size_t i = 1; i < AL.getNumArgs(); i++) {
    Expr *arg2 = AL.getArgAsExpr(i);
    if (auto StrLit = dyn_cast<StringLiteral>(arg2)) {
      Vals.push_back(StrLit->getString());
    } else {
      return nullptr;
    }
  }

  llvm::errs() << __FUNCTION__ << " " << Opt << " 3\n";
  return RemarkAttr::Create(S.Context, Opt, Vals.begin(),
                                Vals.size(), AL);
}
} // namespace clang
