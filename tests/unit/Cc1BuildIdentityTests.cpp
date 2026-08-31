// CC1-000 / CC1-001 -- la identidad del build tiene que distinguir dos compiladores.
//
// POR QUE EXISTE ESTE TEST. El 2026-08-31 un generador de bindings recibio un compilador nuevo,
// leyo las capacidades del ANTERIOR y las reporto como actuales. La causa no fue un bug de
// compilacion: fue que `nsc --version` decia `0.1.0-dev55` en los dos, porque esa cadena es una
// constante escrita a mano que nadie actualiza entre snapshots. El consumidor tuvo que calcular
// una huella del binario para poder distinguirlos.
//
// `kCompilerBuildId` lo arregla, pero un valor generado por CMake puede quedarse vacio en silencio
// -- una sustitucion @NSC_BUILD_ID@ que no ocurre produce un string vacio, no un error de
// compilacion. Ese es exactamente el modo de falla que este test cubre: si la generacion se rompe,
// el build se pone rojo aqui en vez de volver a producir compiladores indistinguibles.

#include "nativesharp/Basic/Version.h"

#include "../support/TestContext.h"

#include <algorithm>
#include <string>
#include <string_view>

int main()
{
    using nativesharp::tests::TestContext;

    TestContext tests;

    const std::string_view buildId = nativesharp::kCompilerBuildId;

    tests.expect(!buildId.empty(), "build id is not empty");
    tests.expect(
        buildId != "@NSC_BUILD_ID@",
        "build id was substituted by CMake, not left as the template placeholder");
    tests.expect(
        buildId.size() == 16,
        "build id is 16 characters (" + std::to_string(buildId.size()) + ")");
    tests.expect(
        std::all_of(
            buildId.begin(),
            buildId.end(),
            [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); }),
        "build id is lowercase hexadecimal");

    // La identidad de release NO cambia entre snapshots y por eso no sirve para distinguir builds.
    // Se fija aqui para que quede escrito que esa estabilidad es intencional y no un descuido.
    tests.expect(
        nativesharp::kCompilerVersion == "0.1.0-dev55",
        "release identity is deliberately stable across snapshots");
    tests.expect(nativesharp::kLanguageVersion == "0.1", "language version is 0.1");
    tests.expect(nativesharp::kCompilerStage == "57", "compiler stage tracks the real snapshot");

    return tests.result();
}
