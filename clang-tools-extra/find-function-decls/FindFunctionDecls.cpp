#include "clang/AST/ASTContext.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"

using namespace clang::tooling;
using namespace llvm;
using namespace clang;
using namespace clang::ast_matchers;

// Apply a custom category to all command-line options so that they are the
// only ones displayed.
static llvm::cl::OptionCategory MyToolCategory("my-tool options");

// CommonOptionsParser declares HelpMessage with a description of the common
// command-line options related to the compilation database and input files.
// It's nice to have this help message in all tools.
static cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);

// A help message for this specific tool can be added afterwards.
static cl::extrahelp MoreHelp("\nMore help text...\n");

StatementMatcher LoopMatcher = forStmt().bind("forLoop");
DeclarationMatcher FunctionMatcher = functionDecl().bind("functionDecl");

class LoopPrinter : public MatchFinder::MatchCallback {
public:
  virtual void run(const MatchFinder::MatchResult &Result) {
    ASTContext *Context = Result.Context;
    const ForStmt *FS = Result.Nodes.getNodeAs<ForStmt>("forLoop");

    if (!FS ||
        !Context->getSourceManager().isWrittenInMainFile(FS->getForLoc()))
      return;

    FS->getSourceRange().dump(Context->getSourceManager());
  }
};

class FunctionPrinter : public MatchFinder::MatchCallback {
public:
  virtual void run(const MatchFinder::MatchResult &Result) {
    ASTContext *Context = Result.Context;
    const FunctionDecl *FD =
        Result.Nodes.getNodeAs<FunctionDecl>("functionDecl");

    if (!FD ||
        !Context->getSourceManager().isWrittenInMainFile(FD->getLocation()))
      return;

    FD->getSourceRange().dump(Context->getSourceManager());
  }
};

int main(int argc, const char **argv) {
  CommonOptionsParser OptionsParser(argc, argv, MyToolCategory);
  ClangTool Tool(OptionsParser.getCompilations(),
                 OptionsParser.getSourcePathList());

  LoopPrinter Printer;
  FunctionPrinter Printer2;
  MatchFinder Finder;
  Finder.addMatcher(LoopMatcher, &Printer);
  Finder.addMatcher(FunctionMatcher, &Printer2);

  return Tool.run(newFrontendActionFactory(&Finder).get());
}
