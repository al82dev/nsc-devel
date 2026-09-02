// Bloque 3 / SHR-013 -- "vistas y tiempo de vida": LA MITAD DEL CRITERIO QUE NO SE PUEDE EJECUTAR.
//
// El criterio del bloque (doc 81, escrito antes de implementarlo) tiene dos mitades:
//
//     "el lexer guarda lexemas como vistas sin copiar Y EL COMPILADOR RECHAZA DEVOLVER UNA VISTA
//      DE UN LOCAL"
//
// La primera se mide ejecutando: `tests/cc1/c23_views_and_lifetimes`, salida 42. La segunda NO
// PUEDE MEDIRSE ASI, y decirlo importa: un programa que el compilador rechaza no produce un
// ejecutable, asi que no hay codigo de salida que comprobar. Su medicion es esta: compilar cada
// forma prohibida y exigir el DIAGNOSTICO EXACTO.
//
// Por que el diagnostico exacto y no solo "fallo": un proyecto puede fallar por veinte razones,
// y una prueba que solo mire el booleano pasaria igual si el rechazo viniera de un error de
// sintaxis en la propia prueba. Es el mismo criterio que Stage50_7BorrowEscapeAnalysisTests --
// del que este archivo es el hermano: alli el prestamo lo dice el CALIFICATIVO sobre un tipo por
// referencia, aqui lo dice el TIPO de un valor por valor.
//
// ---------------------------------------------------------------------------------------------
// POR QUE HIZO FALTA UNA REGLA NUEVA, y esto es lo que hay que leer antes que el codigo.
//
// Stage 50.7 ya rechazaba `borrowed Resource` fugado. No atrapaba nada de esto, y no por
// descuido: su comprobacion esta detras de `returnTypeOwnable`, que vale
//
//     isReferenceTypeKey(t) || t == "builtin:string"
//
// y una vista es un STRUCT POR VALOR, asi que contesta que no y la comprobacion entera se apaga.
// El hueco no se veia leyendo la regla vieja: se ve preguntandole por un tipo que hasta el
// Bloque 3 no existia.
#include "nativesharp/Frontend/ProjectCompilation.h"

#include "../support/TestContext.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <ranges>
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
    return std::ranges::any_of(
        compilation.diagnostics().diagnostics(),
        [text](const auto& diagnostic)
        {
            return diagnostic.message().find(text) != std::string::npos;
        });
}

void expectInvalid(
    TestContext& tests,
    const std::filesystem::path& root,
    std::string_view caseName,
    std::string_view source,
    std::string_view diagnostic)
{
    const std::filesystem::path directory = root / std::string(caseName);
    writeProject(directory, caseName, source);

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectMethodBodies(
        directory / (std::string(caseName) + ".nsproj"), error);
    tests.expect(!ok, std::string(caseName) + " is rejected by the SHR-013 view lifetime rules");
    tests.expect(
        hasDiagnosticText(compilation, diagnostic),
        std::string(caseName) + " reports the expected SHR-013 diagnostic");
}

void expectValid(
    TestContext& tests,
    const std::filesystem::path& root,
    std::string_view caseName,
    std::string_view source)
{
    const std::filesystem::path directory = root / std::string(caseName);
    writeProject(directory, caseName, source);

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectMethodBodies(
        directory / (std::string(caseName) + ".nsproj"), error);
    tests.expect(ok, std::string(caseName) + " is accepted (" + error + ")");
}

// Una vista minima, escrita aqui y no tomada de la biblioteca a proposito: estas pruebas miden LA
// REGLA DEL COMPILADOR, no `StringView`. Si algun dia `StringView` cambia de forma, esto sigue
// midiendo lo mismo.
std::string prefix()
{
    return R"NS(
public borrowed struct Slice
{
    public int* start;
    public int count;
    public Slice(int* data, int length) { start = data; count = length; }
    public int Get(int index) { return start[index]; }
}

public struct Plain
{
    public int value;
    public Plain(int v) { value = v; }
}

public delegate int Reader();
)NS";
}

// -----------------------------------------------------------------------------------------------
// 1. LA LINEA DEL CRITERIO: devolver una vista enraizada en un local.
void testReturningAViewOfALocalIsRejected(TestContext& tests, const std::filesystem::path& root)
{
    // Un arreglo local: la memoria muere al volver, el struct sobrevive, el puntero queda
    // colgando. Nada en el tipo de retorno lo delataria.
    expectInvalid(tests, root, "ReturnViewOfLocalArray", prefix() + R"NS(
class Case
{
    public static Slice Leak()
    {
        int[] numbers = new int[4];
        return new Slice(&numbers[0], 4);
    }
}
)NS", "borrows from local 'numbers' and cannot be returned");

    // Con un local intermedio de tipo vista. El enraizamiento se graba al declararla, porque para
    // cuando alguien la devuelve la expresion que la creo ya no esta a la vista.
    expectInvalid(tests, root, "ReturnViewLocalRootedInLocal", prefix() + R"NS(
class Case
{
    public static Slice Leak()
    {
        int[] numbers = new int[4];
        Slice view = new Slice(&numbers[0], 4);
        return view;
    }
}
)NS", "borrows from local 'numbers' and cannot be returned");

    // Memoria de pila: el mismo defecto con una reserva que no se libera explicitamente, que es
    // lo que lo hace mas facil de pasar por alto.
    expectInvalid(tests, root, "ReturnViewOfStackalloc", prefix() + R"NS(
class Case
{
    public static Slice Leak()
    {
        int* buffer = stackalloc int[4];
        return new Slice(buffer, 4);
    }
}
)NS", "borrows from local 'buffer' and cannot be returned");

    // REASIGNACION. La vista nace enraizada en un parametro --lo cual seria correcto-- y despues
    // se reasigna desde un local. Una regla que solo mirara la declaracion aceptaria esto.
    expectInvalid(tests, root, "ReturnViewReassignedFromLocal", prefix() + R"NS(
class Case
{
    public static Slice Leak(int* source, int length)
    {
        Slice view = new Slice(source, length);
        int* buffer = stackalloc int[2];
        view = new Slice(buffer, 2);
        return view;
    }
}
)NS", "borrows from local 'buffer' and cannot be returned");

    // A TRAVES DE UN ARREGLO DE VISTAS. Es la misma fuga con un nivel de indireccion, y sin esta
    // comprobacion seria el camino obvio para rodear las anteriores.
    expectInvalid(tests, root, "ReturnViewArrayFilledFromLocal", prefix() + R"NS(
class Case
{
    public static Slice[] Leak()
    {
        int[] numbers = new int[4];
        Slice[] views = new Slice[2];
        views[0] = new Slice(&numbers[0], 4);
        return views;
    }
}
)NS", "borrows from local 'numbers' and cannot be returned");
}

// -----------------------------------------------------------------------------------------------
// 2. Un CAMPO de tipo vista. No hay comprobacion que hacer: un campo vive tanto como su objeto, y
// ese objeto no tiene relacion con el duenno de lo prestado. La respuesta honesta es no dejarlo
// declarar, que es exactamente lo que C# hace con `ref struct`.
void testViewFieldsAreRejected(TestContext& tests, const std::filesystem::path& root)
{
    expectInvalid(tests, root, "InstanceViewField", prefix() + R"NS(
class Case
{
    public Slice cached;
}
)NS", "cannot have view type");

    expectInvalid(tests, root, "StaticViewField", prefix() + R"NS(
class Case
{
    public static Slice cached;
}
)NS", "cannot have view type");

    // Un ARREGLO de vistas como campo: un arreglo no copia a sus elementos, asi que guardar
    // vistas en un `T[]` de campo es guardar vistas.
    expectInvalid(tests, root, "ViewArrayField", prefix() + R"NS(
class Case
{
    public Slice[] cached;
}
)NS", "cannot have view type");

    // Un campo de vista dentro de un STRUCT ORDINARIO tampoco: el struct puede ser campo de una
    // clase, y entonces la vista habria entrado por la puerta de atras.
    expectInvalid(tests, root, "ViewFieldInsidePlainStruct", prefix() + R"NS(
public struct Holder
{
    public Slice inner;
}
)NS", "cannot have view type");

    // Y POR MONOMORFIZACION: `Plain<Slice>` no se escribe como campo de vista, pero lo es en
    // cuanto se cierra. Es la razon por la que C# prohibe un `ref struct` como argumento generico,
    // y aqui sale gratis -- la regla mira el campo INSTANCIADO.
    expectInvalid(tests, root, "ViewAsGenericArgument", prefix() + R"NS(
public struct Wrapper<T>
{
    public T value;
    public Wrapper(T item) { value = item; }
}
class Case
{
    public static int Run(int* source)
    {
        Wrapper<Slice> boxed = new Wrapper<Slice>(new Slice(source, 1));
        return boxed.value.Get(0);
    }
}
)NS", "cannot have view type");
}

// -----------------------------------------------------------------------------------------------
// 3. Captura por lambda. El delegate se puede guardar en un campo, devolver, o llamarse horas
// despues; lo prestado no lo sabe. Es el mismo defecto que las dos familias anteriores llegando
// por un tercer camino.
void testViewCapturesAreRejected(TestContext& tests, const std::filesystem::path& root)
{
    expectInvalid(tests, root, "CaptureViewLocal", prefix() + R"NS(
class Case
{
    public static Reader Make(int* source)
    {
        Slice view = new Slice(source, 1);
        Reader reader = () => view.Get(0);
        return reader;
    }
}
)NS", "cannot be captured by a lambda");

    expectInvalid(tests, root, "CaptureViewParameter", prefix() + R"NS(
class Case
{
    public static Reader Make(Slice view)
    {
        Reader reader = () => view.Get(0);
        return reader;
    }
}
)NS", "cannot be captured by a lambda");
}

// -----------------------------------------------------------------------------------------------
// 4. `borrowed` describe el tiempo de vida de un VALOR. Una clase se posee por referencia y ya
// tiene `owned`/`borrowed` en cada uso; un enum y un delegate no prestan nada de nadie.
void testBorrowedAppliesOnlyToStructs(TestContext& tests, const std::filesystem::path& root)
{
    expectInvalid(tests, root, "BorrowedClass", R"NS(
public borrowed class Wrong
{
    public int value;
}
)NS", "applies only to struct declarations");

    expectInvalid(tests, root, "BorrowedEnum", R"NS(
public borrowed enum Wrong
{
    A = 0
}
)NS", "applies only to struct declarations");

    // Una UNION no tiene un miembro vivo conocido: no hay forma de decir de que presta, y una
    // vista cuyo origen no se puede nombrar es justo lo que la comprobacion existe para impedir.
    expectInvalid(tests, root, "BorrowedUnion", R"NS(
public borrowed union Wrong
{
    public int a;
    public float b;
}
)NS", "applies only to struct declarations");
}

// -----------------------------------------------------------------------------------------------
// 5. LO QUE TIENE QUE SEGUIR COMPILANDO. Una regla de rechazo sin la mitad positiva no dice si
// rechaza lo correcto o simplemente rechaza; la primera version de esta regla habria pasado los
// cinco casos de arriba prohibiendo las vistas por completo.
void testSafeViewUsesAreAccepted(TestContext& tests, const std::filesystem::path& root)
{
    expectValid(tests, root, "SafeViewUses", prefix() + R"NS(
class Holder
{
    private int[] storage;

    public Holder(int[] items) { storage = items; }

    // Enraizada en un CAMPO: el campo sobrevive a la llamada, asi que la vista puede salir. Es
    // exactamente la forma de un lexer que guarda la fuente y devuelve lexemas.
    public Slice View() { return new Slice(&storage[0], storage.Length); }
}

class Safe
{
    // Enraizada en un PARAMETRO.
    public static Slice FromParameter(int* source, int length)
    {
        return new Slice(source, length);
    }

    // Local de tipo vista enraizado en un parametro: sale.
    public static Slice ForwardLocal(int* source, int length)
    {
        Slice view = new Slice(source, length);
        return view;
    }

    // Reenviar el resultado de otra llamada que ya devuelve una vista.
    public static Slice Forward(int* source, int length)
    {
        return FromParameter(source, length);
    }

    // Un INDICE local no es un prestamo. Sin esta distincion la regla seria inutil: casi toda
    // expresion menciona algun local.
    public static Slice WithLocalIndex(int* source, int length)
    {
        int offset = 1;
        return new Slice(source + offset, length - offset);
    }

    // Una vista enraizada en un local es perfectamente correcta MIENTRAS NO SALGA. Lo prohibido
    // es que escape, no que exista.
    public static int ConsumeLocally()
    {
        int* buffer = stackalloc int[4];
        buffer[0] = 42;
        Slice view = new Slice(buffer, 4);
        return view.Get(0);
    }

    // Un arreglo local de vistas llenado desde un parametro: sale.
    public static Slice[] ArrayFromParameter(int* source, int length)
    {
        Slice[] views = new Slice[2];
        views[0] = new Slice(source, length);
        return views;
    }

    // Una vista DENTRO de otra vista: componer dos vistas da una vista, y el valor compuesto
    // muere donde muera el que lo contiene.
    public static int Composed(int* source)
    {
        Pair pair = new Pair(new Slice(source, 1), new Slice(source, 1));
        return pair.left.Get(0) + pair.right.Get(0);
    }
}

public borrowed struct Pair
{
    public Slice left;
    public Slice right;
    public Pair(Slice a, Slice b) { left = a; right = b; }
}
)NS");
}
}

int main()
{
    TestContext tests;
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "nsc_stage58_view_lifetimes_tests";
    std::error_code removeError;
    std::filesystem::remove_all(root, removeError);

    testReturningAViewOfALocalIsRejected(tests, root);
    testViewFieldsAreRejected(tests, root);
    testViewCapturesAreRejected(tests, root);
    testBorrowedAppliesOnlyToStructs(tests, root);
    testSafeViewUsesAreAccepted(tests, root);

    return tests.result();
}
