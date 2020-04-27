#include "clang/Sema/RemarkHint.h"
#include "clang/AST/Attrs.inc"
#include "clang/Basic/DiagnosticDriver.h"
#include "clang/Basic/DiagnosticParse.h"
#include "clang/Basic/DiagnosticSema.h"
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
  if (!AL.getNumArgs()) {
    S.Diag(AL.getLoc(), diag::err_pragma_missing_argument) << "clang remark";
    return nullptr;
  }
  if (!AL.isArgIdent(0)) {
    S.Diag(AL.getLoc(), diag::err_attribute_argument_n_type)
        << AL << 1 << AANT_ArgumentIdentifier;
    return nullptr;
  }

  StringRef OptStr = AL.getArgAsIdent(0)->Ident->getName();
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
    S.Diag(AL.getArgAsIdent(0)->Loc, diag::err_pragma_remark_invalid_option)
      << 0 << 2 << OptStr;
    return nullptr;
  }

  if (Opt == RemarkAttr::Conf && AL.getNumArgs() > 3) {
    auto D = S.Diag(AL.getArgAsIdent(0)->Loc, diag::err_attribute_too_many_arguments)
      << "#pragma clang remark conf" << 2;
    for (size_t i = 1; i < AL.getNumArgs(); i++) {
      D << AL.getArgAsExpr(i)->getSourceRange();
    }
  }

  SmallVector<StringRef, 2> Vals;

  for (size_t i = 1; i < AL.getNumArgs(); i++) {
    Expr *arg2 = AL.getArgAsExpr(i);
    assert(isa<StringLiteral>(arg2));
    Vals.push_back(cast<StringLiteral>(arg2)->getString());
  }

  return RemarkAttr::Create(S.Context, Opt, Vals.begin(),
                                Vals.size(), AL);
}
} // namespace clang
