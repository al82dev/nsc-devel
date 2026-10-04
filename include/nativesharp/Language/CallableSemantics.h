#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace nativesharp::language
{
enum class FunctionCarrier { Pointer, Reference };
enum class CallingConvention { NativeSharp, C, StdCall, FastCall, VectorCall, Win64 };

struct FunctionType final
{
    std::vector<std::string> parameters;
    std::string result;
    FunctionCarrier carrier = FunctionCarrier::Pointer;
    CallingConvention callingConvention = CallingConvention::NativeSharp;
    std::string errorType;
    std::string lifetime;
    bool variadic = false;
    bool isUnsafe = false;
    bool isNoHeap = false;
    bool isAsync = false;
    bool nullable = false;
};

std::string functionTypeIdentity(const FunctionType& type);
std::optional<FunctionType> parseFunctionTypeIdentity(std::string_view identity);
bool isFunctionTypeIdentity(std::string_view identity) noexcept;
bool functionTypesAssignable(const FunctionType& source, const FunctionType& target) noexcept;
std::string_view functionCarrierName(FunctionCarrier carrier) noexcept;
std::string_view callingConventionName(CallingConvention convention) noexcept;

// T08-F02. A non-capturing lambda which is not immediately target-converted is a nominal,
// zero-size value. Its identity keeps the source-order owner/lambda ordinals separate from the
// structural call signature: two textually distinct lambdas never become the same type even when
// their parameter and result types are identical. The structural signature is embedded so later
// semantic and backend stages can invoke or convert the value without consulting source syntax.
struct AnonymousLambdaType final
{
    std::size_t ownerOrdinal = 0;
    std::size_t lambdaOrdinal = 0;
    FunctionType signature;
};

std::string anonymousLambdaTypeIdentity(const AnonymousLambdaType& type);
std::optional<AnonymousLambdaType> parseAnonymousLambdaTypeIdentity(std::string_view identity);
bool isAnonymousLambdaTypeIdentity(std::string_view identity) noexcept;
std::string anonymousLambdaGeneratedName(const AnonymousLambdaType& type);

enum class CaptureMode { Copy, Move, Reference, MutableReference };
struct Capture final
{
    std::string name;
    std::size_t size = 0;
    std::size_t alignment = 1;
    CaptureMode mode = CaptureMode::Copy;
    bool sourceOutlivesClosure = true;
};

struct ClosureLayout final { std::size_t size = 0; std::size_t alignment = 1; };
bool computeClosureLayout(std::span<const Capture> captures, ClosureLayout& layout, std::string& error);
bool validateClosureEscape(std::span<const Capture> captures, bool escapes, std::string& error);

enum class CallableTrait { Fn, FnMut, FnOnce };
CallableTrait callableTrait(std::span<const Capture> captures) noexcept;
bool nonCapturingLambdaConvertsToPointer(std::span<const Capture> captures) noexcept;
bool genericLambdaUsesErasedDispatch() noexcept;

enum class MethodValueKind { Group, Bound, FunctionPointer };
struct BoundMethodLayout final { std::size_t size; std::size_t alignment; };
BoundMethodLayout windowsX64BoundMethodLayout() noexcept;

bool validateMustTail(
    const FunctionType& caller,
    const FunctionType& callee,
    bool hasPendingCleanup,
    std::string& error);
}
