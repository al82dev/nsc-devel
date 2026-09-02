// ID 131 -- acceso a un miembro estatico de un tipo generico CERRADO: `Caja<int>.Make(7)`.
//
// DE DONDE SALE. Se midio implementando el Bloque 3. `Span<int>.Of(items)` no compilaba, y el
// diagnostico no decia por que: "expected expression", otra vez "expected expression", y
// "expected ';', found 'Of'". Ninguno menciona genericos. El bloque salio adelante rodeandolo --
// las factorias se metieron en clases NO genericas (`Spans.Of`, `Optionals.Some`) -- y esa
// solucion no escala: el Bloque 4 es generico de punta a punta (`ByteBuffer`, `BinaryReader`,
// `BinaryWriter`, las variantes de bytes, todo sobre `Span<T>`), y habria acabado con una
// clase-sombra no generica por cada tipo.
//
// QUE ESTABA MAL, y son DOS sitios, no uno:
//
//   1. EN UNA EXPRESION. `looksLikeGenericCallTypeArguments` solo acepta una lista de argumentos
//      de tipo si tras el `>` de cierre viene un `(`. Por eso `Ayuda.Envolver<int>(1)` --
//      argumentos de tipo de un METODO-- entraba, y `Caja<int>.Make(7)` --argumentos de tipo de un
//      TIPO-- no: detras del `>` viene un `.`.
//
//   2. AL PRINCIPIO DE UNA SENTENCIA. `Caja<int>.Cuenta = 5;` empieza igual que la declaracion
//      `Caja<int> c = ...`, asi que `canStartTypeAt` decia "declaracion" y el parser terminaba
//      pidiendo un nombre de variable: "expected variable name, found '.'". Las distingue lo que
//      sigue al `>`: identificador en la declaracion, `.` en la expresion.
//
// POR QUE SE RESUELVE POR CLAVE DE TIPO Y NO POR NOMBRE. El parser deja colgado del identificador
// el TypeSyntax del tipo CERRADO, y el modelo de cuerpos lo resuelve con `canonicalTypeKey` --el
// mismo servicio que usan `new Caja<int>()` y la declaracion de un local-- en vez de con
// `resolveTypeIdentifier`, que busca por `identity.name()`. Esa diferencia es la prueba central de
// este archivo: para un generico, `identity.name()` es el nombre ABIERTO, asi que resolver por
// nombre habria devuelto `Caja<T>` donde se escribio `Caja<int>`, y todo lo deducido del miembro
// habria sido de otro tipo. Un compilador que acepta el programa y le pone el tipo equivocado es
// peor que uno que lo rechaza.
//
// LO QUE ESTE ARCHIVO NO MIDE, dicho para que nadie lo herede como cubierto: ID 132, que es el
// vecino. Los metodos genericos se identifican por `nombre``aridad` (ProjectGenericModel.cpp, la
// construccion de `canonicalKey`), asi que dos plantillas con la misma aridad y distinta lista de
// parametros chocan con "duplicate generic template identity". Arreglarlo pide meter la firma en
// la clave Y resolucion de sobrecargas entre plantillas en el punto de llamada; no es este cambio.
#include "nativesharp/Frontend/ProjectCompilation.h"

#include "../support/TestContext.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

namespace
{
using nativesharp::frontend::ProjectCompilation;
using nativesharp::tests::TestContext;

void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary);
    stream << text;
}

void writeProject(
    const std::filesystem::path& directory,
    std::string_view name,
    std::string_view source)
{
    writeFile(
        directory / (std::string(name) + ".nsproj"),
        "<Project><PropertyGroup><Name>" + std::string(name) +
            "</Name><OutputType>StaticLibrary</OutputType></PropertyGroup>"
            "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
    writeFile(directory / "Main.ns", source);
}

bool hasDiagnosticText(const ProjectCompilation& compilation, std::string_view text)
{
    return std::any_of(
        compilation.diagnostics().diagnostics().begin(),
        compilation.diagnostics().diagnostics().end(),
        [text](const auto& diagnostic)
        {
            return diagnostic.message().find(text) != std::string::npos;
        });
}

bool compileCase(
    const std::filesystem::path& root,
    std::string_view caseName,
    std::string_view source,
    ProjectCompilation& compilation,
    std::string& error)
{
    const std::filesystem::path directory = root / std::string(caseName);
    writeProject(directory, caseName, source);
    return compilation.compileProjectMethodBodies(
        directory / (std::string(caseName) + ".nsproj"), error);
}

void expectValid(
    TestContext& tests,
    const std::filesystem::path& root,
    std::string_view caseName,
    std::string_view source)
{
    ProjectCompilation compilation;
    std::string error;
    const bool ok = compileCase(root, caseName, source, compilation, error);
    tests.expect(ok, std::string(caseName) + " compiles (" + error + ")");
}

// Los tipos con los que se prueba. Se escriben aqui y no se toman de la biblioteca a proposito:
// esto mide LA REGLA DEL COMPILADOR, no `Span<T>`. Si la biblioteca cambia de forma, esto sigue
// midiendo lo mismo.
std::string prefix()
{
    return R"NS(
public class Caja<T>
{
    public T Item;
    public static int Cuenta;
    public static Caja<T> Make(T value)
    {
        Caja<T> c = new Caja<T>();
        c.Item = value;
        return c;
    }
}

public class Par<A, B>
{
    public A First;
    public B Second;
    public static int Aridad() { return 2; }
}

public class Ayuda
{
    public static T Envolver<T>(T value) { return value; }
}
)NS";
}

std::string program(std::string_view body)
{
    return "namespace Id131\n{\n" + prefix() +
        "\npublic class Program\n{\n    public static int Run()\n    {\n" +
        std::string(body) + "\n    }\n}\n}\n";
}
}

int main()
{
    TestContext tests;
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "nsc_stage59_closed_generic";
    std::filesystem::remove_all(root);

    // -----------------------------------------------------------------------------------------
    // 1. LA FORMA QUE NO PARSEABA, en sus variantes.
    // -----------------------------------------------------------------------------------------
    expectValid(tests, root, "llamada_estatica", program(
        "        Caja<int> c = Caja<int>.Make(7);\n"
        "        return c.Item;"));

    // Al principio de una sentencia. Este es el caso 2: sin el arreglo, el parser lo leia como
    // una declaracion y pedia un nombre de variable.
    expectValid(tests, root, "campo_estatico_como_sentencia", program(
        "        Caja<int>.Cuenta = 5;\n"
        "        return Caja<int>.Cuenta;"));

    expectValid(tests, root, "campo_estatico_en_expresion", program(
        "        int total = Caja<int>.Cuenta + 1;\n"
        "        return total;"));

    // Dos parametros de tipo: la coma dentro de la lista no puede confundirse con nada.
    expectValid(tests, root, "dos_parametros_de_tipo", program(
        "        return Par<int, int>.Aridad();"));

    // Anidado. El `>>` de cierre cuenta como DOS niveles, que es donde una implementacion
    // ingenua de la profundidad se rompe.
    expectValid(tests, root, "generico_anidado", program(
        "        Caja<int> dentro = Caja<int>.Make(3);\n"
        "        Caja<Caja<int>> fuera = Caja<Caja<int>>.Make(dentro);\n"
        "        return fuera.Item.Item;"));

    // El tipo cerrado como receptor de una llamada cuyo resultado se encadena.
    expectValid(tests, root, "encadenado", program(
        "        return Caja<int>.Make(9).Item;"));

    // -----------------------------------------------------------------------------------------
    // 2. LO QUE YA FUNCIONABA Y NO PUEDE ROMPERSE. Es la mitad importante: la desambiguacion
    //    entre "menor que" y "lista de argumentos de tipo" es exactamente donde un arreglo de
    //    esto se lleva por delante la aritmetica de comparaciones sin que nadie lo note.
    // -----------------------------------------------------------------------------------------
    expectValid(tests, root, "metodo_generico_sigue_igual", program(
        "        return Ayuda.Envolver<int>(7);"));

    expectValid(tests, root, "dos_comparaciones_seguidas", program(
        "        int a = 1; int b = 2; int c = 3; int d = 4;\n"
        "        if (a < b || c > d) { return 1; }\n"
        "        return 0;"));

    // La forma exacta que rompio la version anterior de `looksLikeGenericCallTypeArguments`
    // (ver el comentario de ese predicado): dos comparaciones con un parentesis de conversion
    // entremedias.
    expectValid(tests, root, "comparaciones_con_conversion", program(
        "        int wide = 5;\n"
        "        if (wide < (int)Caja<int>.Cuenta || wide > (int)Caja<int>.Cuenta) { return 1; }\n"
        "        return 0;"));

    expectValid(tests, root, "declaracion_de_local_generico", program(
        "        Caja<int> c = new Caja<int>();\n"
        "        c.Item = 4;\n"
        "        return c.Item;"));

    // -----------------------------------------------------------------------------------------
    // 3. LA PROPIEDAD CENTRAL: el tipo cerrado se resuelve CERRADO.
    //
    //    `Caja<int>.Make(7)` devuelve `Caja<int>`, no `Caja<T>`. Si se resolviera por nombre --que
    //    es lo que hacia el camino de antes-- el receptor seria la declaracion ABIERTA y el tipo
    //    del resultado no encajaria con el del local. Se comprueba asignandolo a un
    //    `Caja<string>`: tiene que ser RECHAZADO. Un compilador que acepta esto es uno que perdio
    //    el argumento de tipo por el camino, y ese fallo es silencioso hasta que alguien lee el
    //    IR.
    // -----------------------------------------------------------------------------------------
    {
        ProjectCompilation compilation;
        std::string error;
        const bool ok = compileCase(root, "el_tipo_cerrado_se_conserva", program(
            "        Caja<string> mal = Caja<int>.Make(7);\n"
            "        return 0;"), compilation, error);
        tests.expect(!ok, "Caja<int>.Make no se puede asignar a Caja<string>");
        tests.expect(
            !hasDiagnosticText(compilation, "expected expression"),
            "y se rechaza por el TIPO, no porque el parser no supiera leerlo");
    }

    std::filesystem::remove_all(root);
    return tests.result();
}
