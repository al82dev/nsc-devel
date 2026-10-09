#include "nsc_suite/TestRegistry.h"

#include "nativesharp/Basic/Sha256.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nsc::suite
{
namespace
{
using nativesharp::basic::Sha256Digest;

void addCase(
    TestRegistry& registry,
    std::string id,
    std::string contractId,
    std::string profile,
    TestFunction function)
{
    registry.add(TestCase{
        std::move(id),
        std::move(contractId),
        "Host.Basic",
        "unit",
        std::move(profile),
        {"basic", "sha256"},
        std::move(function),
    });
}

std::vector<std::uint8_t> asciiBytes(std::string_view text)
{
    std::vector<std::uint8_t> bytes;
    bytes.reserve(text.size());
    for (const char character : text)
    {
        bytes.push_back(static_cast<std::uint8_t>(static_cast<unsigned char>(character)));
    }
    return bytes;
}

std::uint8_t hexNibble(char character)
{
    if (character >= '0' && character <= '9')
    {
        return static_cast<std::uint8_t>(character - '0');
    }
    if (character >= 'a' && character <= 'f')
    {
        return static_cast<std::uint8_t>(character - 'a' + 10);
    }
    throw std::invalid_argument("invalid lowercase hexadecimal SHA-256 oracle");
}

Sha256Digest digestFromHex(std::string_view hexadecimal)
{
    if (hexadecimal.size() != 64)
    {
        throw std::invalid_argument("SHA-256 oracle must contain exactly 64 hexadecimal characters");
    }
    Sha256Digest digest{};
    for (std::size_t index = 0; index < digest.size(); ++index)
    {
        digest[index] = static_cast<std::uint8_t>(
            (hexNibble(hexadecimal[index * 2]) << 4U) |
            hexNibble(hexadecimal[index * 2 + 1]));
    }
    return digest;
}

void expectDigest(
    TestContext& context,
    const std::vector<std::uint8_t>& input,
    std::string_view expectedHex,
    std::string_view description)
{
    const Sha256Digest expected = digestFromHex(expectedHex);
    const Sha256Digest actual = nativesharp::basic::sha256(input);
    context.expect(actual == expected, std::string(description) + " must match the published digest bytes");
    context.expect(
        nativesharp::basic::sha256Hex(actual) == expectedHex,
        std::string(description) + " must match the published lowercase hexadecimal digest");
}

std::vector<std::uint8_t> repeatedByte(std::uint8_t value, std::size_t count)
{
    return std::vector<std::uint8_t>(count, value);
}

void registerMemoryCases(TestRegistry& registry)
{
    addCase(registry, "TB-BAS-024", "CB-BAS-007", "PR", [](TestContext& context) {
        expectDigest(
            context,
            {},
            "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
            "empty message");
    });
    addCase(registry, "TB-BAS-025", "CB-BAS-007", "PR", [](TestContext& context) {
        expectDigest(
            context,
            asciiBytes("abc"),
            "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
            "ASCII abc");
    });
    addCase(registry, "TB-BAS-026", "CB-BAS-007", "PR", [](TestContext& context) {
        expectDigest(
            context,
            asciiBytes("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"),
            "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1",
            "NIST 56-byte message");
    });
    addCase(registry, "TB-BAS-027", "CB-BAS-007", "PR", [](TestContext& context) {
        expectDigest(
            context,
            repeatedByte(static_cast<std::uint8_t>('a'), 55),
            "9f4390f8d30c2dd92ec9f095b65e2b9ae9b0a925a5258e241c9f1e910f734318",
            "55-byte padding boundary");
    });
    addCase(registry, "TB-BAS-028", "CB-BAS-007", "PR", [](TestContext& context) {
        expectDigest(
            context,
            repeatedByte(static_cast<std::uint8_t>('a'), 56),
            "b35439a4ac6f0948b6d6f9e3c6af0f5f590ce20f1bde7090ef7970686ec6738a",
            "56-byte padding boundary");
    });
    addCase(registry, "TB-BAS-029", "CB-BAS-007", "PR", [](TestContext& context) {
        expectDigest(
            context,
            repeatedByte(static_cast<std::uint8_t>('a'), 63),
            "7d3e74a05d7db15bce4ad9ec0658ea98e3f06eeecf16b4c6fff2da457ddc2f34",
            "63-byte block boundary");
        expectDigest(
            context,
            repeatedByte(static_cast<std::uint8_t>('a'), 64),
            "ffe054fe7ae0cb6dc65c3af9b61d5209f439851db43d0ba5997337df154668eb",
            "64-byte block boundary");
        expectDigest(
            context,
            repeatedByte(static_cast<std::uint8_t>('a'), 65),
            "635361c48bb9eab14198e76ea8ab7f1a41685d6ad62aa9146d301d4f17eb0ae0",
            "65-byte block boundary");
    });
    addCase(registry, "TB-BAS-030", "CB-BAS-007", "PR", [](TestContext& context) {
        std::vector<std::uint8_t> bytes;
        bytes.reserve(51);
        constexpr std::array pattern{std::uint8_t{0x00}, std::uint8_t{0x80}, std::uint8_t{0xff}};
        for (std::size_t repetition = 0; repetition < 17; ++repetition)
        {
            bytes.insert(bytes.end(), pattern.begin(), pattern.end());
        }
        expectDigest(
            context,
            bytes,
            "a6588755fa7a61ebcf69964f94db46f8baf592504b5d263e585cdb3fc160d040",
            "binary 00-80-ff pattern");
    });
    addCase(registry, "TB-BAS-031", "CB-BAS-007", "NIGHTLY", [](TestContext& context) {
        expectDigest(
            context,
            repeatedByte(static_cast<std::uint8_t>('a'), 1'000'000),
            "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0",
            "million-a NIST vector");
    });
    addCase(registry, "TB-BAS-032", "CB-BAS-007", "PR", [](TestContext& context) {
        const auto input = asciiBytes("NativeSharp deterministic SHA-256 state");
        const Sha256Digest first = nativesharp::basic::sha256(input);
        const Sha256Digest second = nativesharp::basic::sha256(input);
        context.expect(first == second, "repeated SHA-256 calls must not retain state");
    });
}

void registerHexadecimalCases(TestRegistry& registry)
{
    addCase(registry, "TB-BAS-033", "CB-BAS-009", "PR", [](TestContext& context) {
        const Sha256Digest digest{};
        context.expect(
            nativesharp::basic::sha256Hex(digest) == std::string(64, '0'),
            "zero digest must produce exactly 64 zero characters");
    });
    addCase(registry, "TB-BAS-034", "CB-BAS-009", "PR", [](TestContext& context) {
        Sha256Digest digest{};
        digest[0] = 0x0f;
        digest[1] = 0x10;
        digest[2] = 0xab;
        digest[3] = 0xff;
        const std::string hexadecimal = nativesharp::basic::sha256Hex(digest);
        context.expect(hexadecimal.size() == 64, "hexadecimal digest must have fixed width");
        context.expect(hexadecimal.starts_with("0f10abff"), "hexadecimal digest must preserve leading nibbles");
        context.expect(
            std::none_of(hexadecimal.begin(), hexadecimal.end(), [](char character) {
                return character >= 'A' && character <= 'F';
            }),
            "hexadecimal digest must use lowercase letters");
    });
}
}

void registerBasicSha256Module(TestRegistry& registry)
{
    registerMemoryCases(registry);
    registerHexadecimalCases(registry);
}
}
