#pragma once

#include <filesystem>
#include <iostream>
#include <string_view>
#include <system_error>

namespace nativesharp::tests
{
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
