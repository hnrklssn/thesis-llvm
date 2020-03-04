#include "RemarkMetadata.h"
#include "clang/AST/Attrs.inc"
#include "llvm/IR/Metadata.h"

using namespace llvm;
namespace clang {
MDNode *createRemarkMetadata(LLVMContext &C, const RemarkAttr &Attr) {
  SmallVector<Metadata *, 2> MetadataStrings;
  if (Attr.getOption() == RemarkAttr::Conf) {
    MetadataStrings.push_back(MDString::get(C, "remark_conf"));
  } else {
    // either remark, remark_missed or remark_analysis
    MetadataStrings.push_back(MDString::get(C, Attr.getSpelling()));
  }
  for (const auto Val : Attr.values()) {
    MetadataStrings.push_back(MDString::get(C, Val)); // pass names to add remarks to, or remark format if conf
  }
  return MDNode::get(C, llvm::makeArrayRef(MetadataStrings));
}
} // namespace clang
