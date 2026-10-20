#pragma once

namespace nativesharp::llvm_api
{
enum LLVMDWARFEmissionKind
{
    LLVMDWARFEmissionNone = 0,
    LLVMDWARFEmissionFull = 1,
    LLVMDWARFEmissionLineTablesOnly = 2,
};
}
