#include "clang/Sema/RemarkHint.h"
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
RemarkAttr *handleRemarkAttr2(Sema &S, const ParsedAttr &AL) {
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
    llvm::errs() << __FUNCTION__ << "2.1\n";
    return nullptr;
  }
  IdentifierInfo *Ident = arg->Ident;
  if (!Ident) {
    llvm::errs() << __FUNCTION__ << "2.2\n";
    return nullptr;
  }
  SmallVector<StringRef, 2> Vals;
  for (size_t i = 1; i < AL.getNumArgs(); i++) {
    IdentifierLoc *arg2 = AL.getArgAsIdent(i);
    if (!arg2) {
      llvm::errs() << __FUNCTION__ << "2.11\n";
      return nullptr;
    }
    IdentifierInfo *Ident2 = arg2->Ident;
    if (!Ident2) {
      llvm::errs() << __FUNCTION__ << "2.22\n";
      return nullptr;
    }
    llvm::errs() << __FUNCTION__ << " " << Ident2->getName() << " 3.5\n";
    Vals.push_back(Ident2->getName());
  }
  llvm::errs() << __FUNCTION__ << "2.3\n";
  StringRef OptStr = Ident->getName();
  RemarkAttr::OptionType Opt;
  if (OptStr.equals("funct")) {
    Opt = RemarkAttr::Funct;
  } else if (OptStr.equals("loop")) {
    Opt = RemarkAttr::Loop;
  } else if (OptStr.equals("file")) {
    Opt = RemarkAttr::File;
  } else {
    /*S.Diag(AL.getArgAsIdent(0)->Loc, diag::err_attribute_argument_n_type)
      << Opt << ExpectedFunction;*/
    llvm::errs() << __FUNCTION__ <<  " " << OptStr << " 2.5\n";
    return nullptr;
  }

  llvm::errs() << __FUNCTION__ << " " << Opt << " 3\n";
  //StringRef Val = cast<StringLiteral>(arg2)->getString();
  return RemarkAttr::Create(S.Context, Opt, Vals.begin(),
                                Vals.size(), AL);
}
} // namespace clang
