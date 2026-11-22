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
    // EN: LKF-400: a byte outside the enumeration was cast as is; it now reads as Unknown, the most
    //     conservative escape class.
    // ES: LKF-400: un byte fuera de la enumeración se convertía tal cual; ahora se lee como Unknown, la
    //     clase de escape más conservadora.
    for (std::uint32_t index = 0; index < count && !reader.failed(); ++index)
    {
        const std::uint8_t value = reader.byte();
        values.push_back(value <= static_cast<std::uint8_t>(language::EscapeClass::ForeignImport)
            ? static_cast<language::EscapeClass>(value) : language::EscapeClass::Unknown);
    }
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
    const std::uint8_t passKind = reader.byte();
    parameter.passKind = static_cast<ParameterPassKind>(passKind);
    parameter.pointeeTypeKey = reader.text();
    parameter.typeSyntax = reader.type();
    parameter.typeKey = reader.text();
    readSnapshotFlags(reader, {&parameter.pointerPointeeReadonly, &parameter.pointerNullable,
        &parameter.pointerHasProvenance, &parameter.isParameterPack, &parameter.ffiCallback,
        &parameter.ffiCallbackMayEscape});
    parameter.pointerKnownAlignment = static_cast<std::size_t>(reader.u64());
    parameter.pointerAddressSpace = reader.u32();
    const std::uint8_t ownership = reader.byte();
    parameter.ownership = static_cast<sema::OwnershipKind>(ownership);
    parameter.lifetimeName = reader.text();
    parameter.parameterPackElementTypeKey = reader.text();
    parameter.ffiLengthParameterName = reader.text();
    const std::optional<std::uint64_t> lengthOrdinal = readSnapshotOptional(reader);
    if (lengthOrdinal.has_value()) parameter.ffiLengthParameterOrdinal = static_cast<std::size_t>(*lengthOrdinal);
    parameter.defaultValueSyntax = reader.node();
    // EN: LKF-400: a pass kind or an ownership outside its enumeration rejects the snapshot, which
    //     then falls back to the source (backup rule 0.4).
    // ES: LKF-400: un modo de paso o una propiedad fuera de su enumeración rechaza el snapshot, que
    //     vuelve entonces a la fuente (regla de respaldo 0.4).
    if (passKind > static_cast<std::uint8_t>(ParameterPassKind::In) ||
        ownership > static_cast<std::uint8_t>(sema::OwnershipKind::Borrowed)) return false;
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

bool anySnapshotParameterVolatile(const std::vector<MethodParameterSymbol>& parameters) noexcept
{
    for (const MethodParameterSymbol& parameter : parameters)
    {
        if (parameter.pointerPointeeVolatile || parameter.pointerLevelQualifiers != 0U) return true;
    }
    return false;
}

void writeSnapshotParameterVolatileFlags(
    PrecompiledSnapshotWriter& writer,
    const std::vector<MethodParameterSymbol>& parameters)
{
    writer.u32(static_cast<std::uint32_t>(parameters.size()));
    // C1-05: bit 0 is the volatile base; bits 1-6 the intermediate pointer levels. / El bit 0 es la base volatile;
    //     los bits 1-6 los niveles intermedios.
    for (const MethodParameterSymbol& parameter : parameters)
        writer.byte(static_cast<std::uint8_t>((parameter.pointerPointeeVolatile ? 1U : 0U) |
            (static_cast<unsigned>(parameter.pointerLevelQualifiers) << 1U)));
}

bool readSnapshotParameterVolatileFlags(
    PrecompiledSnapshotReader& reader,
    std::vector<MethodParameterSymbol>& parameters)
{
    const std::uint32_t count = reader.u32();
    if (reader.failed() || count != parameters.size()) return false;
    for (MethodParameterSymbol& parameter : parameters)
    {
        const std::uint8_t value = reader.byte();
        parameter.pointerPointeeVolatile = (value & 1U) != 0U;
        parameter.pointerLevelQualifiers = static_cast<std::uint8_t>(value >> 1U);
    }
    return !reader.failed();
}
}
