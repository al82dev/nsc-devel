#include "nativesharp/Basic/Sha256.h"

#include <fstream>
#include <iterator>
#include <vector>

namespace nativesharp::basic {
bool sha256File(const std::filesystem::path &path, Sha256Digest &digest,
                std::uintmax_t &size, std::string &error) {
  error.clear();
  std::ifstream stream(path, std::ios::binary);
  if (!stream) {
    error = "cannot open file for SHA-256: '" + path.string() + "'";
    return false;
  }

  std::vector<std::uint8_t> bytes;
  for (std::istreambuf_iterator<char> iterator(stream), end; iterator != end;
       ++iterator) {
    bytes.push_back(
        static_cast<std::uint8_t>(static_cast<unsigned char>(*iterator)));
  }
  if (stream.bad()) {
    error = "cannot read file for SHA-256: '" + path.string() + "'";
    return false;
  }

  size = static_cast<std::uintmax_t>(bytes.size());
  digest = sha256(bytes);
  return true;
}

std::string sha256Hex(const Sha256Digest &digest) {
  static constexpr char alphabet[] = "0123456789abcdef";
  std::string result;
  result.reserve(digest.size() * 2U);
  for (const std::uint8_t byte : digest) {
    result.push_back(alphabet[byte >> 4U]);
    result.push_back(alphabet[byte & 0x0fU]);
  }
  return result;
}
} // namespace nativesharp::basic
