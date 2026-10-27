#include "SnapshotCodecSupport.h"

namespace nativesharp::frontend
{
void writeSnapshotFlags(PrecompiledSnapshotWriter& writer, std::initializer_list<bool> flags)
{
    for (const bool flag : flags) writer.byte(flag ? 1U : 0U);
}

void readSnapshotFlags(PrecompiledSnapshotReader& reader, std::initializer_list<bool*> flags)
{
    for (bool* const flag : flags) *flag = reader.byte() != 0U;
}

void writeSnapshotOptional(PrecompiledSnapshotWriter& writer, const std::optional<std::uint64_t>& value)
{
    writer.byte(value.has_value() ? 1U : 0U);
    if (value.has_value()) writer.u64(*value);
}

std::optional<std::uint64_t> readSnapshotOptional(PrecompiledSnapshotReader& reader)
{
    if (reader.byte() == 0U) return std::nullopt;
    return reader.u64();
}

void writeSnapshotTexts(PrecompiledSnapshotWriter& writer, const std::vector<std::string>& values)
{
    writer.u32(static_cast<std::uint32_t>(values.size()));
    for (const std::string& value : values) writer.text(value);
}

std::vector<std::string> readSnapshotTexts(PrecompiledSnapshotReader& reader)
{
    std::vector<std::string> values;
    const std::uint32_t count = reader.u32();
    for (std::uint32_t index = 0; index < count && !reader.failed(); ++index) values.push_back(reader.text());
    return values;
}

void writeSnapshotTextPairs(
    PrecompiledSnapshotWriter& writer,
    const std::vector<std::pair<std::string, std::string>>& values)
{
    writer.u32(static_cast<std::uint32_t>(values.size()));
    for (const std::pair<std::string, std::string>& value : values)
    {
        writer.text(value.first);
        writer.text(value.second);
    }
}

std::vector<std::pair<std::string, std::string>> readSnapshotTextPairs(PrecompiledSnapshotReader& reader)
{
    std::vector<std::pair<std::string, std::string>> values;
    const std::uint32_t count = reader.u32();
    for (std::uint32_t index = 0; index < count && !reader.failed(); ++index)
    {
        std::string first = reader.text();
        values.emplace_back(std::move(first), reader.text());
    }
    return values;
}

void writeSnapshotEscapes(PrecompiledSnapshotWriter& writer, const std::vector<language::EscapeClass>& values)
{
    writer.u32(static_cast<std::uint32_t>(values.size()));
    for (const language::EscapeClass value : values) writer.byte(static_cast<std::uint8_t>(value));
}

std::vector<language::EscapeClass> readSnapshotEscapes(PrecompiledSnapshotReader& reader)
{
    std::vector<language::EscapeClass> values;
    const std::uint32_t count = reader.u32();
    for (std::uint32_t index = 0; index < count && !reader.failed(); ++index)
        values.push_back(static_cast<language::EscapeClass>(reader.byte()));
    return values;
}

void writeSnapshotParameter(PrecompiledSnapshotWriter& writer, const MethodParameterSymbol& parameter)
{
    writer.node(parameter.syntax);
    writer.text(parameter.name);
    writer.byte(static_cast<std::uint8_t>(parameter.passKind));
    writer.text(parameter.pointeeTypeKey);
    writer.type(parameter.typeSyntax);
    writer.text(parameter.typeKey);
    writeSnapshotFlags(writer, {parameter.pointerPointeeReadonly, parameter.pointerNullable,
        parameter.pointerHasProvenance, parameter.isParameterPack, parameter.ffiCallback,
        parameter.ffiCallbackMayEscape});
    writer.u64(parameter.pointerKnownAlignment);
    writer.u32(parameter.pointerAddressSpace);
    writer.byte(static_cast<std::uint8_t>(parameter.ownership));
    writer.text(parameter.lifetimeName);
    writer.text(parameter.parameterPackElementTypeKey);
    writer.text(parameter.ffiLengthParameterName);
    writeSnapshotOptional(writer, parameter.ffiLengthParameterOrdinal.has_value()
        ? std::optional<std::uint64_t>(*parameter.ffiLengthParameterOrdinal) : std::nullopt);
    writer.node(parameter.defaultValueSyntax);
}

bool readSnapshotParameter(PrecompiledSnapshotReader& reader, MethodParameterSymbol& parameter)
{
    parameter.syntax = reader.node();
    parameter.name = reader.text();
    parameter.passKind = static_cast<ParameterPassKind>(reader.byte());
    parameter.pointeeTypeKey = reader.text();
    parameter.typeSyntax = reader.type();
    parameter.typeKey = reader.text();
    readSnapshotFlags(reader, {&parameter.pointerPointeeReadonly, &parameter.pointerNullable,
        &parameter.pointerHasProvenance, &parameter.isParameterPack, &parameter.ffiCallback,
        &parameter.ffiCallbackMayEscape});
    parameter.pointerKnownAlignment = static_cast<std::size_t>(reader.u64());
    parameter.pointerAddressSpace = reader.u32();
    parameter.ownership = static_cast<sema::OwnershipKind>(reader.byte());
    parameter.lifetimeName = reader.text();
    parameter.parameterPackElementTypeKey = reader.text();
    parameter.ffiLengthParameterName = reader.text();
    const std::optional<std::uint64_t> lengthOrdinal = readSnapshotOptional(reader);
    if (lengthOrdinal.has_value()) parameter.ffiLengthParameterOrdinal = static_cast<std::size_t>(*lengthOrdinal);
    parameter.defaultValueSyntax = reader.node();
    return !reader.failed() && parameter.syntax != nullptr;
}

void writeSnapshotParameters(PrecompiledSnapshotWriter& writer, const std::vector<MethodParameterSymbol>& parameters)
{
    writer.u32(static_cast<std::uint32_t>(parameters.size()));
    for (const MethodParameterSymbol& parameter : parameters) writeSnapshotParameter(writer, parameter);
}

bool readSnapshotParameters(PrecompiledSnapshotReader& reader, std::vector<MethodParameterSymbol>& parameters)
{
    const std::uint32_t count = reader.u32();
    for (std::uint32_t index = 0; index < count && !reader.failed(); ++index)
    {
        MethodParameterSymbol parameter;
        if (!readSnapshotParameter(reader, parameter)) return false;
        parameters.push_back(std::move(parameter));
    }
    return !reader.failed();
}
}
