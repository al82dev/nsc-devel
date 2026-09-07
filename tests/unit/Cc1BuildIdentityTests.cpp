// CC1-000 / CC1-001 -- la identidad del build tiene que distinguir dos compiladores.
//
// POR QUE EXISTE ESTE TEST. El 2026-08-31 un generador de bindings recibio un compilador nuevo,
// leyo las capacidades del ANTERIOR y las reporto como actuales. La causa no fue un bug de
// compilacion: fue que `nsc --version` decia `0.1.0-dev55` en los dos, porque esa cadena es una
// constante escrita a mano que nadie actualiza entre snapshots. El consumidor tuvo que calcular
// una huella del binario para poder distinguirlos.
//
// `kCompilerVersion` incluye ahora el build id, pero un valor generado por CMake puede quedarse vacio en silencio
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

    // La parte compatible queda estable, mientras la version visible contiene la identidad exacta
    // de esta construccion como metadata SemVer.
    tests.expect(
        nativesharp::kCompilerReleaseVersion == "0.9.0",
        "compiler compatibility identity is the stable 0.9.0 release");
    tests.expect(
        nativesharp::kCompilerVersion.starts_with("0.9.0+build."),
        "visible compiler version contains SemVer build metadata");
    tests.expect(
        nativesharp::kCompilerVersion.ends_with(buildId),
        "visible compiler version contains the exact build id");
    tests.expect(nativesharp::kLanguageVersion == "0.1", "language version is 0.1");
    tests.expect(nativesharp::kCompilerStage == "60", "compiler stage tracks the real snapshot");

    return tests.result();
}
