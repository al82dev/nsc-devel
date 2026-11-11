#pragma once

#include "../LlvmCApi.h"

namespace nativesharp::llvm_backend
{
template <typename AddAttribute>
void applySysVRecordByValue(llvm_api::LLVMContextRef context, llvm_api::LLVMTypeRef carrier,
                            unsigned alignment, unsigned physicalOrdinal, AddAttribute&& addAttribute)
{
    using namespace llvm_api;
    const unsigned byValueKind = LLVMGetEnumAttributeKindForName("byval", 5);
    const unsigned alignKind = LLVMGetEnumAttributeKindForName("align", 5);
    addAttribute(physicalOrdinal + 1, LLVMCreateTypeAttribute(context, byValueKind, carrier));
    addAttribute(physicalOrdinal + 1, LLVMCreateEnumAttribute(context, alignKind, alignment));
}
}
