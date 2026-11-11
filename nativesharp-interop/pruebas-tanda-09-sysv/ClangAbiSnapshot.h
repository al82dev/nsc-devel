#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace clang { class CodeGenerator; class FunctionDecl; }

// Experimental value-only snapshot; not a production ABI or metadata contract.
namespace t09_probe
{
enum class TypeKind { Void, Integer, Float, Double, Pointer, Structure, Array };
struct Type
{
    TypeKind kind = TypeKind::Void;
    std::uint64_t widthOrCount = 0;
    bool packed = false;
    std::vector<Type> elements;
};
enum class Passing { Direct, Extend, Indirect, Ignore };
struct LogicalValue
{
    Passing passing = Passing::Ignore;
    std::optional<Type> coercion;
    std::uint64_t offset = 0;
    std::uint64_t alignment = 0;
    std::uint64_t firstPhysical = 0;
    std::uint64_t physicalCount = 0;
    bool flatten = false;
    bool byValue = false;
    bool signExtend = false;
    bool zeroExtend = false;
};
struct PhysicalParameter
{
    Type type;
    std::optional<Type> pointee;
    std::uint64_t alignment = 0;
    bool structureReturn = false;
    bool byValue = false;
    bool signExtend = false;
    bool zeroExtend = false;
    bool inRegister = false;
    bool noAlias = false;
};
struct Plan
{
    std::string symbol;
    std::string targetTriple;
    std::string dataLayout;
    unsigned callingConvention = 0;
    bool variadic = false;
    bool returnSignExtend = false;
    bool returnZeroExtend = false;
    Type physicalReturn;
    LogicalValue logicalReturn;
    std::vector<LogicalValue> logicalParameters;
    std::vector<PhysicalParameter> physicalParameters;
};

std::optional<Plan> copyPlan(clang::CodeGenerator& generator,
                             const clang::FunctionDecl& function,
                             std::string& error);
}
