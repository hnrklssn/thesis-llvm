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

StatementMatcher ForMatcher = forStmt().bind("forLoop");
StatementMatcher WhileMatcher = whileStmt().bind("whileLoop");
DeclarationMatcher FunctionMatcher = functionDecl().bind("functionDecl");

class Printer : public MatchFinder::MatchCallback {
public:
  virtual void run(const MatchFinder::MatchResult &Result) {
    ASTContext *Context = Result.Context;
    const ForStmt *FS = Result.Nodes.getNodeAs<ForStmt>("forLoop");
    const WhileStmt *WS = Result.Nodes.getNodeAs<WhileStmt>("whileLoop");
    const FunctionDecl *FD =
        Result.Nodes.getNodeAs<FunctionDecl>("functionDecl");
    SourceManager &SM = Context->getSourceManager();

    printStmtIfNotNull("ForStmt;", FS, SM);
    printStmtIfNotNull("WhileStmt;", WS, SM);
    printStmtIfNotNull("FuncDecl;", FD, SM);
  }

private:
  template <class T>
  void printStmtIfNotNull(std::string name, const T *Node,
                          SourceManager &SourceM) {
    if (!Node)
      return;

    if (!SourceM.isWrittenInMainFile(Node->getBeginLoc()))
      return;

    Node->getSourceRange().print(llvm::outs() << name, SourceM);
    llvm::outs() << "\n";
  }
};

int main(int argc, const char **argv) {
  CommonOptionsParser OptionsParser(argc, argv, MyToolCategory);
  ClangTool Tool(OptionsParser.getCompilations(),
                 OptionsParser.getSourcePathList());

  Printer Printer;
  MatchFinder Finder;
  Finder.addMatcher(FunctionMatcher, &Printer);
  Finder.addMatcher(WhileMatcher, &Printer);
  Finder.addMatcher(ForMatcher, &Printer);

  return Tool.run(newFrontendActionFactory(&Finder).get());
}
