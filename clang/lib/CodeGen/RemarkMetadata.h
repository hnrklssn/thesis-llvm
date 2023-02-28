#ifndef LLVM_CLANG_CODEGEN_REMARK_METADATA_H
#define LLVM_CLANG_CODEGEN_REMARK_METADATA_H

#include "clang/AST/ASTContext.h"
#include "llvm/IR/Metadata.h"

namespace clang {
  llvm::MDNode *createRemarkMetadata(llvm::LLVMContext &C, const RemarkAttr &Attr);

} // namespace clang
#endif // LLVM_CLANG_CODEGEN_REMARK_METADATA_H
