#include "clang/Sema/RemarkHint.h"
#include "clang/AST/Attrs.inc"
#include "clang/Basic/DiagnosticDriver.h"
#include "clang/Basic/DiagnosticParse.h"
#include "clang/Basic/DiagnosticSema.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/ErrorHandling.h"

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

  std::string Group;
  switch (AL.getSemanticSpelling()) {
  case RemarkAttr::Spelling::Pragma_remark:
    Group = "pass";
    break;
  case RemarkAttr::Spelling::Pragma_remark_analysis:
    Group = "pass-analysis";
    break;
  case RemarkAttr::Spelling::Pragma_remark_missed:
    Group = "pass-missed";
    break;
  default:
    llvm_unreachable("pragma remark spelling missing");
  }
  // Signal that at least one remark in this group is activated,
  // otherwise they are all ignored
  S.Diags.setSeverityForGroup(diag::Flavor::Remark, Group,
                              diag::Severity::Remark);
  return RemarkAttr::Create(S.Context, Opt, Vals.begin(),
                                Vals.size(), AL);
}
} // namespace clang
