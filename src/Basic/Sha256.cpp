#include "nativesharp/Basic/Sha256.h"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <vector>

namespace nativesharp::basic {
namespace {
std::uint32_t rotateRight(std::uint32_t value, unsigned amount) noexcept {
  return (value >> amount) | (value << (32U - amount));
}

constexpr std::array<std::uint32_t, 64> Sha256Constants = {
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU,
    0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U, 0xd807aa98U, 0x12835b01U,
    0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U,
    0xc19bf174U, 0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU,
    0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU, 0x983e5152U,
    0xa831c66dU, 0xb00327c8U, 0xbf597fc7U, 0xc6e00bf3U, 0xd5a79147U,
    0x06ca6351U, 0x14292967U, 0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU,
    0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
    0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U, 0xd192e819U,
    0xd6990624U, 0xf40e3585U, 0x106aa070U, 0x19a4c116U, 0x1e376c08U,
    0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU,
    0x682e6ff3U, 0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
    0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U};

std::vector<std::uint8_t> padSha256Input(std::span<const std::uint8_t> input) {
  const std::uint64_t bitLength = static_cast<std::uint64_t>(input.size()) * 8ULL;
  const std::size_t paddedSize = ((input.size() + 9U + 63U) / 64U) * 64U;
  std::vector<std::uint8_t> padded(paddedSize, 0);
  std::copy(input.begin(), input.end(), padded.begin());
  padded[input.size()] = 0x80U;
  for (unsigned byte = 0; byte < 8U; ++byte) {
    padded[paddedSize - 1U - byte] =
        static_cast<std::uint8_t>((bitLength >> (byte * 8U)) & 0xffU);
  }
  return padded;
}

std::array<std::uint32_t, 64> sha256Words(
    const std::vector<std::uint8_t> &padded, std::size_t block) {
  std::array<std::uint32_t, 64> words{};
  for (std::size_t index = 0; index < 16U; ++index) {
    const std::size_t offset = block + index * 4U;
    words[index] = (static_cast<std::uint32_t>(padded[offset]) << 24U) |
                   (static_cast<std::uint32_t>(padded[offset + 1U]) << 16U) |
                   (static_cast<std::uint32_t>(padded[offset + 2U]) << 8U) |
                   static_cast<std::uint32_t>(padded[offset + 3U]);
  }
  for (std::size_t index = 16; index < words.size(); ++index) {
    const std::uint32_t x = words[index - 15U];
    const std::uint32_t y = words[index - 2U];
    const std::uint32_t s0 = rotateRight(x, 7U) ^ rotateRight(x, 18U) ^ (x >> 3U);
    const std::uint32_t s1 = rotateRight(y, 17U) ^ rotateRight(y, 19U) ^ (y >> 10U);
    words[index] = words[index - 16U] + s0 + words[index - 7U] + s1;
  }
  return words;
}

void compressSha256Block(
    const std::array<std::uint32_t, 64> &words,
    std::array<std::uint32_t, 8> &state) {
  std::uint32_t a = state[0];
  std::uint32_t b = state[1];
  std::uint32_t c = state[2];
  std::uint32_t d = state[3];
  std::uint32_t e = state[4];
  std::uint32_t f = state[5];
  std::uint32_t g = state[6];
  std::uint32_t h = state[7];
  for (std::size_t index = 0; index < words.size(); ++index) {
    const std::uint32_t s1 = rotateRight(e, 6U) ^ rotateRight(e, 11U) ^ rotateRight(e, 25U);
    const std::uint32_t choice = (e & f) ^ ((~e) & g);
    const std::uint32_t temp1 = h + s1 + choice + Sha256Constants[index] + words[index];
    const std::uint32_t s0 = rotateRight(a, 2U) ^ rotateRight(a, 13U) ^ rotateRight(a, 22U);
    const std::uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
    const std::uint32_t temp2 = s0 + majority;
    h = g;
    g = f;
    f = e;
    e = d + temp1;
    d = c;
    c = b;
    b = a;
    a = temp1 + temp2;
  }
  state[0] += a;
  state[1] += b;
  state[2] += c;
  state[3] += d;
  state[4] += e;
  state[5] += f;
  state[6] += g;
  state[7] += h;
}

Sha256Digest sha256Digest(const std::array<std::uint32_t, 8> &state) {
  Sha256Digest digest{};
  for (std::size_t index = 0; index < state.size(); ++index) {
    digest[index * 4U] = static_cast<std::uint8_t>((state[index] >> 24U) & 0xffU);
    digest[index * 4U + 1U] = static_cast<std::uint8_t>((state[index] >> 16U) & 0xffU);
    digest[index * 4U + 2U] = static_cast<std::uint8_t>((state[index] >> 8U) & 0xffU);
    digest[index * 4U + 3U] = static_cast<std::uint8_t>(state[index] & 0xffU);
  }
  return digest;
}
} // namespace

Sha256Digest sha256(std::span<const std::uint8_t> input) {
  std::array<std::uint32_t, 8> state = {
      0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
      0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U};
  const std::vector<std::uint8_t> padded = padSha256Input(input);
  for (std::size_t block = 0; block < padded.size(); block += 64U) {
    compressSha256Block(sha256Words(padded, block), state);
  }
  return sha256Digest(state);
}

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
