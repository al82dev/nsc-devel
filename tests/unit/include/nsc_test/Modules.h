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
void registerProjectDriverMetadataUnitTests(TestRegistry& registry);
void registerRuntimeLinkerInteropUnitTests(TestRegistry& registry);
void registerSemanticUnitTests(TestRegistry& registry);
void registerSourceUnitTests(TestRegistry& registry);
void registerTransitionalLegacyUnitTests(TestRegistry& registry);
void registerArgumentLegacyUnitTests(TestRegistry& registry);
bool handleArgumentLegacyChild(int argumentCount, char** arguments, int& result);
}
