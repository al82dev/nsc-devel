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
enum class CallingConvention { NativeSharp, C, StdCall, FastCall, VectorCall };

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
