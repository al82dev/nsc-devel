#pragma once

namespace nsc::test
{
class TestRegistry;

void registerBackendUnitTests(TestRegistry& registry);
void registerBasicUnitTests(TestRegistry& registry);
void registerDiagnosticsUnitTests(TestRegistry& registry);
void registerFlowUnitTests(TestRegistry& registry);
void registerIrUnitTests(TestRegistry& registry);
void registerLexerUnitTests(TestRegistry& registry);
void registerNumericUnitTests(TestRegistry& registry);
void registerParserUnitTests(TestRegistry& registry);
void registerSemanticUnitTests(TestRegistry& registry);
void registerSourceUnitTests(TestRegistry& registry);
}
