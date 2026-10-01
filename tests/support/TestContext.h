#pragma once

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <system_error>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace nativesharp::tests
{
#ifdef _WIN32
// A CRASH IS A TEST RESULT, NOT A USER-INTERFACE EVENT.
//
// CTest's `--interactive-debug-mode 0` protects the direct test process, but many integration
// tests launch a second generation of freshly linked NativeSharp executables through
// std::system().  Installing the policy again in the test process makes that inheritance
// explicit and also closes the process-local Windows Error Reporting path.  This is enabled only
// by the builder's NSC_TEST_NONINTERACTIVE environment contract, so a developer who starts a test
// manually can still use the normal debugger UI.
inline void configureNonInteractiveErrorHandling() noexcept
{
    char* nonInteractive = nullptr;
    std::size_t nonInteractiveLength = 0;
    const bool requestedByEnvironment =
        _dupenv_s(
            &nonInteractive, &nonInteractiveLength, "NSC_TEST_NONINTERACTIVE") == 0 &&
        nonInteractive != nullptr;
    std::free(nonInteractive);
    if (!requestedByEnvironment)
    {
        return;
    }

    constexpr UINT requested =
        SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX;
    const UINT previous = ::SetErrorMode(requested);
    ::SetErrorMode(previous | requested);

    DWORD previousThread = 0;
    if (::SetThreadErrorMode(requested, &previousThread) != FALSE)
    {
        DWORD ignored = 0;
        ::SetThreadErrorMode(previousThread | requested, &ignored);
    }

    using WerSetFlagsFunction = HRESULT(WINAPI*)(DWORD);
    if (HMODULE wer = ::LoadLibraryW(L"wer.dll"); wer != nullptr)
    {
        const auto setFlags = reinterpret_cast<WerSetFlagsFunction>(
            ::GetProcAddress(wer, "WerSetFlags"));
        if (setFlags != nullptr)
        {
            constexpr DWORD werFaultReportingNoUi = 0x20;
            setFlags(werFaultReportingNoUi);
        }
        ::FreeLibrary(wer);
    }
}
#endif

// DONDE TRABAJA UNA PRUEBA QUE CONSTRUYE COSAS DE VERDAD, y por que hace falta poder moverlo.
//
// Estas pruebas se hacen su propia raiz y dejan ahi los .obj, los .lib, los ejecutables y los
// logs del enlazador. Por defecto va bajo temp_directory_path(), que en Windows es %TEMP%, y eso
// esta bien para una corrida local: es temporal, y resultKeeping() lo borra cuando la prueba pasa.
//
// El problema aparece cuando la prueba falla y quien tiene que leer la evidencia NO esta sentado
// delante de esa maquina. Medido el 2026-09-02: tres pruebas de host en rojo, la evidencia
// conservada correctamente en %TEMP%\nativesharp_stage48_3_c_host_integration\... y sin manera de
// leerla desde fuera, porque %TEMP% cae fuera de las carpetas del repositorio. El diagnostico
// salio igual, por eliminacion, pero costo una corrida entera de comprobacion que el log habria
// resuelto en dos segundos.
//
// NSC_TEST_EVIDENCE_DIR arregla eso: si esta puesta, las raices cuelgan de ahi en vez de %TEMP%.
// run_windows_checklist.bat la pone a %ROOT%\_evidencia\<NSC_LOG_DIR> antes de ctest, asi que
// cada pasada de all.bat deja su evidencia dentro del repositorio, separada de la otra pasada y
// al lado de sus logs. Sin la variable el comportamiento es exactamente el de antes.
//
// La raiz NO se crea aqui: cada prueba ya la borra y la crea al arrancar, que es lo que las
// mantiene aisladas entre si cuando ctest corre con -j.
inline std::filesystem::path evidenceRoot(std::string_view name)
{
    std::string base;
#ifdef _WIN32
    char* value = nullptr;
    std::size_t length = 0;
    if (_dupenv_s(&value, &length, "NSC_TEST_EVIDENCE_DIR") == 0 && value != nullptr)
    {
        base.assign(value);
        std::free(value);
    }
#else
    if (const char* value = std::getenv("NSC_TEST_EVIDENCE_DIR"); value != nullptr)
    {
        base.assign(value);
    }
#endif
    if (base.empty())
    {
        return std::filesystem::temp_directory_path() / name;
    }
    return std::filesystem::path(base) / name;
}

class TestContext final
{
public:
    TestContext() noexcept
    {
#ifdef _WIN32
        configureNonInteractiveErrorHandling();
#endif
    }

    void expect(bool condition, std::string_view message)
    {
        ++checks_;
        if (!condition)
        {
            ++failures_;
            std::cerr << "FAIL: " << message << '\n';
        }
    }

    // Cierra la prueba SIN BORRAR LA EVIDENCIA CUANDO FALLA.
    //
    // POR QUE EXISTE. Las pruebas que construyen algo de verdad --DLLs, bibliotecas de
    // importacion y anfitriones nativos de C/C++-- trabajan en un directorio temporal y lo
    // borraban al terminar, PASARAN O NO. Cuando una fallaba, lo unico que quedaba era el nombre
    // del check; el comando, el log del enlazador y los objetos se habian ido. Medido el
    // 2026-09-02: una integracion externa fallo y su directorio temporal ya habia desaparecido
    // cuando se intento inspeccionar el log.
    //
    // Una prueba que borra su evidencia al fallar obliga a reproducir a mano lo que acaba de
    // pasar, y eso es justo lo que una suite existe para evitar. En verde se limpia igual, que
    // para eso es temporal.
    int resultKeeping(const std::filesystem::path& evidence)
    {
        const int outcome = result();
        std::error_code ignored;
        if (outcome == 0)
        {
            std::filesystem::remove_all(evidence, ignored);
        }
        else
        {
            std::cerr << "EVIDENCIA CONSERVADA EN: " << evidence.string() << '\n';
        }
        return outcome;
    }

    int result() const noexcept
    {
        if (failures_ == 0)
        {
            std::cout << "PASS: " << checks_ << " checks\n";
            return 0;
        }

        std::cerr << "FAILED: " << failures_ << " of " << checks_ << " checks\n";
        return 1;
    }

private:
    int checks_ = 0;
    int failures_ = 0;
};
}
