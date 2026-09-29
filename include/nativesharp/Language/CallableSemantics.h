#pragma once

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace nativesharp::language
{
enum class CallingConvention { NativeSharp, C, StdCall, FastCall };

struct FunctionType final
{
    std::vector<std::string> parameters;
    std::string result;
    CallingConvention callingConvention = CallingConvention::NativeSharp;
    std::string errorType;
    bool variadic = false;
    bool isUnsafe = false;
    bool nullable = false;
};

std::string functionTypeIdentity(const FunctionType& type);
bool functionTypesAssignable(const FunctionType& source, const FunctionType& target) noexcept;

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
