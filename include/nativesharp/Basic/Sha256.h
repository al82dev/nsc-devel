#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>

namespace nativesharp::basic {
using Sha256Digest = std::array<std::uint8_t, 32>;

Sha256Digest sha256(std::span<const std::uint8_t> input);

bool sha256File(const std::filesystem::path &path, Sha256Digest &digest,
                std::uintmax_t &size, std::string &error);

std::string sha256Hex(const Sha256Digest &digest);
} // namespace nativesharp::basic
