#pragma once

#include "../LlvmCApi.h"

namespace nativesharp::llvm_backend
{
template <typename AddAttribute>
void applySysVRecordByValue(llvm_api::LLVMContextRef context, llvm_api::LLVMTypeRef carrier,
                            AddAttribute&& addAttribute)
{
    using namespace llvm_api;
    const unsigned byValueKind = LLVMGetEnumAttributeKindForName("byval", 5);
    const unsigned alignKind = LLVMGetEnumAttributeKindForName("align", 5);
    addAttribute(2, LLVMCreateTypeAttribute(context, byValueKind, carrier));
    addAttribute(2, LLVMCreateEnumAttribute(context, alignKind, 8));
}
}
