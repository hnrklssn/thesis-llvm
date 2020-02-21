#include "RemarkMetadata.h"
#include "llvm/IR/Metadata.h"

using namespace llvm;
namespace clang {
MDNode *createRemarkMetadata(LLVMContext &C, const RemarkAttr &Attr) {
  SmallVector<Metadata *, 2> MetadataStrings;
  MetadataStrings.push_back(MDString::get(C, Attr.getSpelling())); // either remark, remark_missed or remark_analysis
  for (const auto Val : Attr.values()) {
    MetadataStrings.push_back(MDString::get(C, Val)); // pass names to add remarks to
  }
  return MDNode::get(C, llvm::makeArrayRef(MetadataStrings));
}
} // namespace clang
