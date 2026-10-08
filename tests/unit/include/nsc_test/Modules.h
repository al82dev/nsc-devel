#pragma once

namespace nsc::test
{
class TestRegistry;

void registerBasicUnitTests(TestRegistry& registry);
void registerDiagnosticsUnitTests(TestRegistry& registry);
void registerLexerUnitTests(TestRegistry& registry);
void registerNumericUnitTests(TestRegistry& registry);
void registerParserUnitTests(TestRegistry& registry);
void registerSourceUnitTests(TestRegistry& registry);
}
