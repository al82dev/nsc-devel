#pragma once

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <system_error>

namespace nativesharp::tests
{
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
    // importacion, anfitriones de C, C++ y .NET-- trabajan en un directorio temporal y lo
    // borraban al terminar, PASARAN O NO. Cuando una fallaba, lo unico que quedaba era el nombre
    // del check; el comando, el log del enlazador y los objetos se habian ido. Medido el
    // 2026-09-02: `nsc.stage48_5` fallo con NSLink, y al ir a leer su `dotnet_build.log` la
    // respuesta fue "The system cannot find the path specified" -- porque la prueba se lo habia
    // llevado por delante.
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
