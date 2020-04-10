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
StatementMatcher ForRangeMatcher = cxxForRangeStmt().bind("forRangeLoop");
StatementMatcher WhileMatcher = whileStmt().bind("whileLoop");
StatementMatcher DoMatcher = doStmt().bind("doLoop");
DeclarationMatcher FunctionMatcher = functionDecl().bind("functionDecl");

class Printer : public MatchFinder::MatchCallback {
public:
  virtual void run(const MatchFinder::MatchResult &Result) {
    ASTContext *Context = Result.Context;
    const ForStmt *FS = Result.Nodes.getNodeAs<ForStmt>("forLoop");
    const CXXForRangeStmt *FRS =
        Result.Nodes.getNodeAs<CXXForRangeStmt>("forRangeLoop");
    const WhileStmt *WS = Result.Nodes.getNodeAs<WhileStmt>("whileLoop");
    const DoStmt *DS = Result.Nodes.getNodeAs<DoStmt>("doLoop");
    const FunctionDecl *FD =
        Result.Nodes.getNodeAs<FunctionDecl>("functionDecl");
    SourceManager &SM = Context->getSourceManager();

    printStmtIfNotNull("ForRangeStmt;", FRS, SM);
    printStmtIfNotNull("ForStmt;", FS, SM);
    printStmtIfNotNull("WhileStmt;", WS, SM);
    printStmtIfNotNull("DoStmt;", DS, SM);
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

  std::string command;
  auto compilationDB =
      OptionsParser.getCompilations()
          .getCompileCommands(OptionsParser.getSourcePathList().front())
          .front();
  auto commandList = compilationDB.CommandLine;
  for (auto &part : commandList)
    command += " " + part;

  // todo use diff seperator?

  // allow moving the binary and still finding clang builtin headers.
  ArgumentsAdjuster ardj1 =
      getInsertArgumentAdjuster("-I/usr/local/bin/../lib/clang/10.0.0/include");
  Tool.appendArgumentsAdjuster(ardj1);
  // you can bundle the headers needed instead of looking for them

  llvm::outs() << "cd " << compilationDB.Directory << " && " << command << "\n";

  Printer Printer;
  MatchFinder Finder;
  Finder.addMatcher(FunctionMatcher, &Printer);
  Finder.addMatcher(WhileMatcher, &Printer);
  Finder.addMatcher(ForMatcher, &Printer);
  Finder.addMatcher(ForRangeMatcher, &Printer);
  Finder.addMatcher(DoMatcher, &Printer);

  return Tool.run(newFrontendActionFactory(&Finder).get());
}
