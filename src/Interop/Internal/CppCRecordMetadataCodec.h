#pragma once

#include "nativesharp/Interop/CppSemanticRecord.h"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace nativesharp::interop::metadata_detail
{
// Internal catalogue codec. It does not by itself publish an NSMETA section.
// Both functions leave their output unchanged on failure.
bool encodeCRecordCatalog(std::span<const CppCRecordSnapshot> records,
                          std::vector<std::uint8_t>& output, std::string& error);
bool decodeCRecordCatalog(std::span<const std::uint8_t> bytes,
                          std::vector<CppCRecordSnapshot>& output, std::string& error);
}
