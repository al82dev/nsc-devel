// EN: NativeSharp 3.0 freeze (NSC-PLAN-CONGELAMIENTO-3.0.md). Each block reproduces, by compiling
//     and running a real Windows x64 program at -O0 and -O2, one contract the language promises
//     before the freeze: C-08 allocator alignment, C-06 published surface, C-07 IDisposable, C-01
//     lost ownership, C-02 synchronous await, C-05 file mode, C-11/C-12 value and array
//     destruction, C-13 throws contracts, C-14 future drop, C-16 generator cleanup and C-17
//     async-generator errors.
// ES: Congelamiento de NativeSharp 3.0 (NSC-PLAN-CONGELAMIENTO-3.0.md). Cada bloque reproduce,
//     compilando y ejecutando un programa Windows x64 real en -O0 y -O2, un contrato que el
//     lenguaje promete antes del congelamiento: C-08 alineacion de allocators, C-06 superficie
//     publicada, C-07 IDisposable, C-01 ownership perdido, C-02 await sincrono, C-05 modo
//     archivo, C-11/C-12 destruccion de valores y arreglos, C-13 contratos throws, C-14 drop de
//     futures, C-16 cleanup de generators y C-17 errores de async-generators.

#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Basic/Version.h"
#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Interop/LibraryMetadata.h"
#include "nativesharp/Interop/LibraryMetadataCodec.h"

#include "../support/TestContext.h"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

namespace
{
using nativesharp::ExitCode;
using nativesharp::driver::Driver;
using nativesharp::tests::TestContext;

void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << text;
}

/// EN: Writes `directory/<name>.nsproj` compiling every .ns file and referencing NativeSharp.Core
///     (and NativeSharp.Core.Collections.Generic when `collections` is set).
/// ES: Escribe `directory/<name>.nsproj`, que compila todo .ns y referencia NativeSharp.Core
///     (y NativeSharp.Core.Collections.Generic cuando `collections` esta activo).
std::filesystem::path writeProject(
    const std::filesystem::path& directory,
    std::string_view name,
    bool collections = false,
    bool unwindPanics = false)
{
    const std::filesystem::path lib = std::filesystem::path(NSC_SOURCE_DIR) / "lib";
    const std::filesystem::path core = lib / "NativeSharp.Core/NativeSharp.Core.nsproj";
    const std::filesystem::path generic =
        lib / "NativeSharp.Core.Collections.Generic/NativeSharp.Core.Collections.Generic.nsproj";
    std::string references = "<ProjectReference Include=\"" + core.generic_string() + "\" />";
    if (collections)
        references += "<ProjectReference Include=\"" + generic.generic_string() + "\" />";
    const std::filesystem::path project = directory / (std::string(name) + ".nsproj");
    writeFile(project,
        "<Project><PropertyGroup><Name>" + std::string(name) + "</Name>" +
            (unwindPanics ? "<PanicProfile>unwind-native</PanicProfile>" : "") +
            "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup>"
            "<ItemGroup>" + references + "</ItemGroup></Project>");
    return project;
}

/// EN: Compiles `project` into `executable` at `level`; returns the exit code, `diagnostic` gets
///     the driver's output and error streams.
/// ES: Compila `project` en `executable` con `level`; devuelve el codigo de salida y `diagnostic`
///     recibe la salida y el error del driver.
ExitCode compile(
    const std::filesystem::path& project,
    const std::filesystem::path& executable,
    std::string_view level,
    std::string& diagnostic)
{
    const std::string projectText = project.string();
    const std::string executableText = executable.string();
    const std::array<std::string_view, 6> arguments = {
        level, "--no-artifact-cache", "--emit-project-oop-exe", projectText, "-o", executableText};
    Driver driver;
    std::ostringstream output;
    std::ostringstream error;
    const ExitCode code = driver.run(arguments, output, error);
    diagnostic = output.str() + error.str();
    return code;
}

/// EN: `source` must compile at -O0 and -O2 and both programs must exit with `expected`.
/// ES: `source` debe compilar en -O0 y -O2 y ambos programas deben salir con `expected`.
void expectRun(
    TestContext& tests,
    const std::filesystem::path& root,
    std::string_view name,
    std::string_view source,
    int expected,
    bool collections = false)
{
    const std::filesystem::path directory = root / std::string(name);
    const std::filesystem::path project = writeProject(directory, name, collections);
    writeFile(directory / "Main.ns", source);
    const std::array<std::string_view, 2> levels = {"-O0", "-O2"};
    for (const std::string_view level : levels)
    {
        const std::filesystem::path executable =
            directory / (std::string(name) + std::string(level.substr(1)) + ".exe");
        std::string diagnostic;
        const ExitCode code = compile(project, executable, level, diagnostic);
        const std::string label = std::string(name) + " [" + std::string(level) + "]";
        tests.expect(code == ExitCode::Success, label + ": compiles: " + diagnostic);
        if (code != ExitCode::Success) continue;
        const std::string command = "\"\"" + executable.string() + "\"\"";
        const int observed = std::system(command.c_str());
        tests.expect(observed == expected,
            label + ": exits " + std::to_string(expected) + ", observed " + std::to_string(observed));
    }
}

/// EN: Like expectRun, and the program's standard output must equal `output`. `unwindPanics`
///     selects the unwind-native panic profile, so a panic runs the active cleanups.
/// ES: Como expectRun, y la salida estandar del programa debe ser `output`. `unwindPanics` elige
///     el perfil de panic unwind-native, para que un panic corra los cleanups activos.
void expectRunOutput(
    TestContext& tests,
    const std::filesystem::path& root,
    std::string_view name,
    std::string_view source,
    int expected,
    std::string_view output,
    bool unwindPanics)
{
    const std::filesystem::path directory = root / std::string(name);
    const std::filesystem::path project = writeProject(directory, name, false, unwindPanics);
    writeFile(directory / "Main.ns", source);
    const std::array<std::string_view, 2> levels = {"-O0", "-O2"};
    for (const std::string_view level : levels)
    {
        const std::string stem = std::string(name) + std::string(level.substr(1));
        const std::filesystem::path executable = directory / (stem + ".exe");
        const std::filesystem::path captured = directory / (stem + ".out");
        std::string diagnostic;
        const ExitCode code = compile(project, executable, level, diagnostic);
        const std::string label = std::string(name) + " [" + std::string(level) + "]";
        tests.expect(code == ExitCode::Success, label + ": compiles: " + diagnostic);
        if (code != ExitCode::Success) continue;
        const std::string command =
            "\"\"" + executable.string() + "\" > \"" + captured.string() + "\"\"";
        const int observed = std::system(command.c_str());
        std::ifstream stream(captured, std::ios::binary);
        const std::string text((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
        tests.expect(observed == expected && text == output,
            label + ": exits " + std::to_string(expected) + " printing '" + std::string(output) +
                "', observed " + std::to_string(observed) + " printing '" + text + "'");
    }
}

/// EN: `source` must be rejected and the diagnostic must contain `fragment`. `collections` adds the
///     NativeSharp.Core.Collections.Generic reference.
/// ES: `source` debe rechazarse y el diagnostico debe contener `fragment`. `collections` agrega la
///     referencia a NativeSharp.Core.Collections.Generic.
void expectReject(
    TestContext& tests,
    const std::filesystem::path& root,
    std::string_view name,
    std::string_view source,
    std::string_view fragment,
    bool collections = false)
{
    const std::filesystem::path directory = root / std::string(name);
    const std::filesystem::path project = writeProject(directory, name, collections);
    writeFile(directory / "Main.ns", source);
    std::string diagnostic;
    const ExitCode code = compile(project, directory / (std::string(name) + ".exe"), "-O0", diagnostic);
    tests.expect(code != ExitCode::Success && diagnostic.find(fragment) != std::string::npos,
        std::string(name) + ": is rejected with \"" + std::string(fragment) + "\": " + diagnostic);
}

/// EN: C-08. `new (alloc) T` asks IAllocator.Allocate for sizeof(T) and alignof(T): an align(32)
///     struct lands on a 32-byte boundary even when the allocator would hand out 8-aligned
///     blocks for smaller requests, and the object's Deallocate receives the same size and
///     alignment its Allocate received.
/// ES: C-08. `new (alloc) T` pide a IAllocator.Allocate sizeof(T) y alignof(T): un struct
///     align(32) queda en un limite de 32 aunque el allocator entregue bloques alineados a 8 para
///     pedidos menores, y el Deallocate del objeto recibe el mismo tamano y alineacion que su
///     Allocate.
void testAllocatorAlignment(TestContext& tests, const std::filesystem::path& root)
{
    expectRun(tests, root, "C08AllocatorAlignment", R"ns(
using NativeSharp.Core;

public align(32) struct Wide
{
    public long A;
    public long B;
}

class Box
{
    public long V;
    public Box(long v) { V = v; }
}

class ExactAllocator : IAllocator
{
    private owned byte[] pool;
    private int next;
    public static long AllocBytes = 0;
    public static int AllocAlign = 0;
    public static long FreeBytes = 0;
    public static int FreeAlign = 0;
    public static int Frees = 0;

    public ExactAllocator(int size) { pool = new byte[size]; next = 0; }

    // Honors exactly the requested alignment and nothing more: the block starts at a 64-byte
    // boundary plus `alignment`, so any alignment smaller than requested would be visible.
    public byte* Allocate(long bytes, int alignment)
    {
        AllocBytes = bytes;
        AllocAlign = alignment;
        byte* data = Spans.Of<byte>(pool).Data();
        long address = (long)data;
        long start = (address + 63) / 64 * 64 + (long)next * 256;
        next = next + 1;
        long offset = 0;
        if (alignment < 64) { offset = (long)alignment; }
        return data + (start - address) + offset;
    }

    public byte* Resize(byte* ptr, long oldBytes, long newBytes, int alignment) { return null; }

    public void Deallocate(byte* ptr, long bytes, int alignment)
    {
        FreeBytes = bytes;
        FreeAlign = alignment;
        Frees = Frees + 1;
    }
}

class Program
{
    public static int Main()
    {
        raii ExactAllocator alloc = new ExactAllocator(4096);
        Wide* wide = new (alloc) Wide();
        if ((long)wide % 32 != 0) { return 1; }
        if (ExactAllocator.AllocAlign != 32) { return 2; }
        if (ExactAllocator.AllocBytes != 32) { return 3; }
        {
            owned Box box = new (alloc) Box(7);
            if (box.V != 7) { return 4; }
            if ((long)ExactAllocator.AllocAlign <= 0) { return 5; }
        }
        if (ExactAllocator.Frees != 1) { return 6; }
        if (ExactAllocator.FreeBytes != ExactAllocator.AllocBytes) { return 7; }
        if (ExactAllocator.FreeAlign != ExactAllocator.AllocAlign) { return 8; }
        return 0;
    }
}
)ns", 0);
}

/// EN: C-07. NativeSharp.Core publishes IDisposable: `using` compiles against the library without
///     a local declaration and calls Dispose exactly once on every exit path.
/// ES: C-07. NativeSharp.Core publica IDisposable: `using` compila contra la biblioteca sin una
///     declaracion local y llama Dispose exactamente una vez en cada salida.
void testCoreDisposable(TestContext& tests, const std::filesystem::path& root)
{
    expectRun(tests, root, "C07CoreDisposable", R"ns(
using NativeSharp.Core;

class Handle : IDisposable
{
    public static int Disposed = 0;
    public void Dispose() { Disposed = Disposed + 1; }
}

class Program
{
    public static int Work(bool early)
    {
        using (Handle handle = new Handle())
        {
            if (early) { return 1; }
        }
        return 2;
    }

    public static int Main()
    {
        int first = Work(true);
        int second = Work(false);
        if (first != 1 || second != 2) { return 9; }
        return Handle.Disposed;
    }
}
)ns", 2);
}
}

/// EN: C-01. A `new` that reaches no owner is a compile error: a local without raii/owned
///     (declared with a type or with var, or assigned later), a discarded expression, and a
///     throw operand that is a borrow or a raii owner without move.
/// ES: C-01. Un `new` que no llega a ningun dueno es error de compilacion: un local sin
///     raii/owned (con tipo o con var, o asignado despues), una expresion descartada y un operando
///     de throw que es un prestamo o un owner raii sin move.
void testLostOwnershipRejected(TestContext& tests, const std::filesystem::path& root)
{
    const std::string_view box = "class Box { public int V; }\n";
    const std::string lost = "reaches no owner";
    expectReject(tests, root, "C01TypedLocal", std::string(box) +
        "class Program { public static int Main() { Box b = new Box(); return b.V; } }\n", lost);
    expectReject(tests, root, "C01VarLocal", std::string(box) +
        "class Program { public static int Main() { var b = new Box(); return b.V; } }\n", lost);
    expectReject(tests, root, "C01Assigned", std::string(box) +
        "class Program { public static int Main() { Box b = null; b = new Box(); return 0; } }\n",
        lost);
    expectReject(tests, root, "C01Discarded", std::string(box) +
        "class Program { public static int Main() { new Box(); return 0; } }\n", lost);
    const std::string_view boom = "using NativeSharp.Core;\nclass Boom : Exception { }\n";
    const std::string thrown = "throw hands the exception object";
    expectReject(tests, root, "C01ThrowRaii", std::string(boom) +
        "class Program { public static int Main() { raii Boom b = new Boom(); throw b; } }\n",
        thrown);
    expectReject(tests, root, "C01ThrowBorrow", std::string(boom) +
        "class Program { public static void Fail(Boom given) { throw given; }\n"
        "public static int Main() { return 0; } }\n", thrown);
}

/// EN: C-01. Every owning form compiles and destroys its object exactly once: raii and owned
///     locals, an owned return, an owned parameter, an owned field, a user operator and
///     conversion with an owned return, and an owning `using` (Dispose, then the destructor).
/// ES: C-01. Cada forma con dueno compila y destruye su objeto exactamente una vez: locales raii
///     y owned, un retorno owned, un parametro owned, un campo owned, un operador y una
///     conversion de usuario con retorno owned, y un `using` dueno (Dispose y luego destructor).
void testOwningFormsAccepted(TestContext& tests, const std::filesystem::path& root)
{
    expectRun(tests, root, "C01OwningForms", R"ns(
using NativeSharp.Core;

class Res : IDisposable
{
    public static int Live = 0;
    public static int Disposed = 0;
    public int X;
    public Res(int x) { X = x; Live = Live + 1; }
    ~Res() { Live = Live - 1; }
    public void Dispose() { Disposed = Disposed + 1; }
    public static owned Res operator +(Res left, Res right) { return new Res(left.X + right.X); }
    public static explicit operator owned Res(int value) { return new Res(value); }
}

class Holder
{
    public owned Res Item;
    public Holder(owned Res item) { Item = move(item); }
}

class Program
{
    public static owned Res Make() { return new Res(1); }
    public static int Take(owned Res r) { return r.X; }

    public static int Scope(bool early)
    {
        using (Res r = new Res(3))
        {
            if (early) { return 1; }
        }
        return 2;
    }

    public static int Main()
    {
        int sum = 0;
        {
            raii Res a = new Res(1);
            owned Res b = new Res(2);
            raii Res c = Make();
            raii Holder h = new Holder(new Res(4));
            raii Res added = a + b;
            raii Res cast = (Res)5;
            sum = added.X + cast.X + c.X + h.Item.X;
        }
        sum = sum + Take(new Res(6));
        Scope(true);
        Scope(false);
        if (Res.Disposed != 2) { return 100 + Res.Disposed; }
        if (Res.Live != 0) { return 200 + Res.Live; }
        return sum;
    }
}
)ns", 3 + 5 + 1 + 4 + 6);
}

/// EN: C-01. The catch owns the exception it handles and destroys it once: on normal completion,
///     on `break`/`continue`/`return` out of the body, and when the body throws a new exception.
///     A rethrow (`throw;` or `throw e;`) hands it over, also to a catch nested in the same body.
/// ES: C-01. El catch es dueno de la excepcion que maneja y la destruye una vez: al terminar
///     normal, con `break`/`continue`/`return` fuera del cuerpo y cuando el cuerpo lanza una
///     excepcion nueva. Un rethrow (`throw;` o `throw e;`) la entrega, tambien a un catch anidado.
void testCatchOwnsException(TestContext& tests, const std::filesystem::path& root)
{
    expectRun(tests, root, "C01CatchOwnership", R"ns(
using NativeSharp.Core;

class Boom : Exception
{
    public static int Destroyed = 0;
    public Boom(string message) : base(message) { }
    ~Boom() { Destroyed = Destroyed + 1; }
}

class Program
{
    public static void Fail() throws Boom { throw new Boom("bad"); }

    public static void FailOwned() throws Boom
    {
        raii Boom problem = new Boom("owned");
        throw move(problem);
    }

    public static void Middle() throws Boom
    {
        try { Fail(); }
        catch (Boom b) { throw; }
    }

    public static void Again() throws Boom
    {
        try { Fail(); }
        catch (Boom b) { throw b; }
    }

    public static void Wrap() throws Boom
    {
        try { Fail(); }
        catch (Boom b) { throw new Boom("wrapped"); }
    }

    public static int Inner()
    {
        int seen = 0;
        try { Fail(); }
        catch (Boom b)
        {
            try { throw; }
            catch (Boom c) { seen = 1; }
        }
        return seen;
    }

    public static int Leave()
    {
        for (int i = 0; i < 3; i = i + 1)
        {
            try { Fail(); }
            catch (Boom b) { if (i == 1) break; continue; }
        }
        try { Fail(); }
        catch (Boom b) { return Boom.Destroyed; }
        return 0;
    }

    public static int Main()
    {
        try { FailOwned(); }
        catch (Boom b) { if (Boom.Destroyed != 0) { return 101; } }
        if (Boom.Destroyed != 1) { return 102; }
        try { Middle(); }
        catch (Boom b) { }
        if (Boom.Destroyed != 2) { return 103; }
        try { Again(); }
        catch (Boom b) { }
        if (Boom.Destroyed != 3) { return 104; }
        if (Inner() != 1 || Boom.Destroyed != 4) { return 105; }
        try { Wrap(); }
        catch (Boom b) { if (Boom.Destroyed != 5) { return 106; } }
        if (Boom.Destroyed != 6) { return 107; }
        if (Leave() != 8 || Boom.Destroyed != 9) { return 108; }
        return 0;
    }
}
)ns", 0);
}

/// EN: DEC-10. The caught exception belongs to its catch for the whole handler and is destroyed when
///     the handler ends. Storing it where it outlives the handler (a field, a static, an outer local, a
///     return, also through a local alias) is a compile error; a local of the handler may alias it, and
///     only move(e) takes it out, after which the catch neither destroys it nor rethrows it.
/// ES: DEC-10. La excepcion atrapada pertenece a su catch durante todo el handler y se destruye al
///     terminar el handler. Guardarla donde sobrevive al handler (un campo, un static, un local
///     exterior, un return, tambien a traves de un alias local) es error de compilacion; un local del
///     handler puede ser alias, y solo move(e) la saca, despues de lo cual el catch ni la destruye ni
///     la relanza.
void testCaughtExceptionStaysInCatch(TestContext& tests, const std::filesystem::path& root)
{
    expectReject(tests, root, "DEC10CatchStoredInField", R"ns(
using NativeSharp.Core;
class Boom : Exception
{
    public static int Destroyed;
    public int Code;
    public Boom(int code) : base("boom") { Code = code; }
    ~Boom() { Destroyed = Destroyed + 1; }
}
class Holder
{
    public Boom Saved;
}
class Program
{
    public static int Main()
    {
        raii Holder holder = new Holder();
        try { throw new Boom(7); }
        catch (Boom problem) { holder.Saved = problem; }
        int destroyed = Boom.Destroyed;
        return destroyed * 100 + holder.Saved.Code;
    }
}
)ns", "belongs to its catch until the handler ends");
    expectReject(tests, root, "DEC10CatchStoredInOuterLocal", R"ns(
using NativeSharp.Core;
class Boom : Exception
{
    public static int Destroyed;
    public int Code;
    public Boom(int code) : base("boom") { Code = code; }
    ~Boom() { Destroyed = Destroyed + 1; }
}
class Program
{
    public static int Main()
    {
        Boom outer = null;
        try { throw new Boom(7); }
        catch (Boom problem) { outer = problem; }
        return outer.Code;
    }
}
)ns", "belongs to its catch until the handler ends");
    expectReject(tests, root, "DEC10CatchReturned", R"ns(
using NativeSharp.Core;
class Boom : Exception
{
    public static int Destroyed;
    public int Code;
    public Boom(int code) : base("boom") { Code = code; }
    ~Boom() { Destroyed = Destroyed + 1; }
}
class Program
{
    public static Boom Catch()
    {
        try { throw new Boom(7); }
        catch (Boom problem) { return problem; }
        return null;
    }
    public static int Main() { return Catch().Code; }
}
)ns", "cannot be returned");
    expectReject(tests, root, "DEC10CatchStoredInStatic", R"ns(
using NativeSharp.Core;
class Boom : Exception
{
    public static int Destroyed;
    public int Code;
    public Boom(int code) : base("boom") { Code = code; }
    ~Boom() { Destroyed = Destroyed + 1; }
}
class Program
{
    public static Boom Last;
    public static int Main()
    {
        try { throw new Boom(7); }
        catch (Boom problem) { Last = problem; }
        return Last.Code;
    }
}
)ns", "belongs to its catch until the handler ends");
    expectReject(tests, root, "DEC10CatchAliasStored", R"ns(
using NativeSharp.Core;
class Boom : Exception
{
    public static int Destroyed;
    public int Code;
    public Boom(int code) : base("boom") { Code = code; }
    ~Boom() { Destroyed = Destroyed + 1; }
}
class Holder { public Boom Saved; }
class Program
{
    public static int Main()
    {
        raii Holder holder = new Holder();
        try { throw new Boom(7); }
        catch (Boom problem) { Boom alias = problem; holder.Saved = alias; }
        return holder.Saved.Code;
    }
}
)ns", "the caught exception in 'alias'");
    expectReject(tests, root, "DEC10CatchRethrowAfterMove", R"ns(
using NativeSharp.Core;
class Boom : Exception
{
    public static int Destroyed;
    public int Code;
    public Boom(int code) : base("boom") { Code = code; }
    ~Boom() { Destroyed = Destroyed + 1; }
}
class Program
{
    public static void Run() throws Boom
    {
        try { throw new Boom(7); }
        catch (Boom problem)
        {
            raii Boom keep = move(problem);
            throw;
        }
    }
    public static int Main() { return 0; }
}
)ns", "was moved out of its catch");
    expectReject(tests, root, "DEC10CatchUseAfterMove", R"ns(
using NativeSharp.Core;
class Boom : Exception
{
    public static int Destroyed;
    public int Code;
    public Boom(int code) : base("boom") { Code = code; }
    ~Boom() { Destroyed = Destroyed + 1; }
}
class Program
{
    public static int Main()
    {
        try { throw new Boom(7); }
        catch (Boom problem)
        {
            raii Boom keep = move(problem);
            return problem.Code;
        }
        return 0;
    }
}
)ns", "use of moved-from caught exception 'problem'");
    expectRun(tests, root, "DEC10CatchMovedToOwnedField", R"ns(
using NativeSharp.Core;
class Boom : Exception
{
    public static int Destroyed;
    public int Code;
    public Boom(int code) : base("boom") { Code = code; }
    ~Boom() { Destroyed = Destroyed + 1; }
}
class Holder
{
    public owned Boom Saved;
}
class Program
{
    public static int Main()
    {
        int code = 0;
        int during = 0;
        {
            raii Holder holder = new Holder();
            try { throw new Boom(7); }
            catch (Boom problem) { holder.Saved = move(problem); }
            during = Boom.Destroyed;
            code = holder.Saved.Code;
        }
        return during * 100 + Boom.Destroyed * 10 + code;
    }
}
)ns", 17);
    expectRun(tests, root, "DEC10CatchMovedToLocal", R"ns(
using NativeSharp.Core;
class Boom : Exception
{
    public static int Destroyed;
    public int Code;
    public Boom(int code) : base("boom") { Code = code; }
    ~Boom() { Destroyed = Destroyed + 1; }
}
class Program
{
    public static int Main()
    {
        int inside = 0;
        try { throw new Boom(7); }
        catch (Boom problem)
        {
            {
                raii Boom keep = move(problem);
                inside = keep.Code;
            }
            inside = inside + Boom.Destroyed * 100;
        }
        return Boom.Destroyed * 1000 + inside;
    }
}
)ns", 1107);
    expectRun(tests, root, "DEC10CatchReturnedByMove", R"ns(
using NativeSharp.Core;
class Boom : Exception
{
    public static int Destroyed;
    public int Code;
    public Boom(int code) : base("boom") { Code = code; }
    ~Boom() { Destroyed = Destroyed + 1; }
}
class Program
{
    public static owned Boom Catch()
    {
        try { throw new Boom(7); }
        catch (Boom problem) { return move(problem); }
        return null;
    }
    public static int Main()
    {
        raii Boom b = Catch();
        return Boom.Destroyed * 100 + b.Code;
    }
}
)ns", 7);
    expectRun(tests, root, "DEC10CatchMovedOnOnePath", R"ns(
using NativeSharp.Core;
class Boom : Exception
{
    public static int Destroyed;
    public int Code;
    public Boom(int code) : base("boom") { Code = code; }
    ~Boom() { Destroyed = Destroyed + 1; }
}
class Program
{
    public static int Run(bool take)
    {
        int r = 0;
        try { throw new Boom(7); }
        catch (Boom problem)
        {
            if (take)
            {
                raii Boom keep = move(problem);
                r = keep.Code;
            }
        }
        return r;
    }
    public static int Main()
    {
        int a = Run(true);
        int first = Boom.Destroyed;
        int b = Run(false);
        return first * 1000 + Boom.Destroyed * 100 + a + b;
    }
}
)ns", 1207);
    expectRun(tests, root, "DEC10CatchAliasInHandler", R"ns(
using NativeSharp.Core;
class Boom : Exception
{
    public static int Destroyed;
    public int Code;
    public Boom(int code) : base("boom") { Code = code; }
    ~Boom() { Destroyed = Destroyed + 1; }
}
class Program
{
    public static int Main()
    {
        int r = 0;
        try { throw new Boom(7); }
        catch (Boom problem)
        {
            Boom inner = null;
            inner = problem;
            r = inner.Code;
        }
        return Boom.Destroyed * 100 + r;
    }
}
)ns", 107);
    expectRun(tests, root, "DEC10CatchAliasDeclaredInHandler", R"ns(
using NativeSharp.Core;
class Boom : Exception
{
    public static int Destroyed;
    public int Code;
    public Boom(int code) : base("boom") { Code = code; }
    ~Boom() { Destroyed = Destroyed + 1; }
}
class Program
{
    public static int Main()
    {
        int sum = 0;
        try { throw new Boom(7); }
        catch (Exception e) { Exception again = e; sum = sum + 20; }
        return Boom.Destroyed * 100 + sum;
    }
}
)ns", 120);
    expectRun(tests, root, "DEC10CatchThrownByMove", R"ns(
using NativeSharp.Core;
class Boom : Exception
{
    public static int Destroyed;
    public int Code;
    public Boom(int code) : base("boom") { Code = code; }
    ~Boom() { Destroyed = Destroyed + 1; }
}
class Program
{
    public static void Run() throws Boom
    {
        try { throw new Boom(7); }
        catch (Boom problem) { throw move(problem); }
    }
    public static int Main()
    {
        try { Run(); }
        catch (Boom b) { if (Boom.Destroyed != 0) return 50; }
        return Boom.Destroyed;
    }
}
)ns", 1);
}

/// EN: C-02. A synchronous await polls its future once, so a producer that can suspend is a
///     compile error: directly (`yield;`), transitively (it awaits one that suspends) or through
///     a future of unknown origin (a local that is reassigned).
/// ES: C-02. Un await sincrono hace un solo poll, asi que un productor que puede suspenderse es
///     error de compilacion: directo (`yield;`), transitivo (espera a uno que se suspende) o por
///     un future de origen desconocido (un local reasignado).
void testSynchronousAwaitRejected(TestContext& tests, const std::filesystem::path& root)
{
    const std::string_view producer =
        "    public static async int Producer() { yield; return 5; }\n";
    const std::string suspends = "synchronous await of a producer that can suspend";
    expectReject(tests, root, "C02Direct", "class Program\n{\n" + std::string(producer) +
        "    public static int Main() { return await Producer(); }\n}\n", suspends);
    expectReject(tests, root, "C02Transitive", "class Program\n{\n" + std::string(producer) +
        "    public static async int Middle() { int value = await Producer(); return value + 1; }\n"
        "    public static int Main() { return await Middle(); }\n}\n", suspends);
    expectReject(tests, root, "C02UnknownOrigin", "class Program\n{\n" + std::string(producer) +
        "    public static async int Quick() { return 1; }\n"
        "    public static int Main()\n    {\n        Future<int> later = Quick();\n"
        "        later = Producer();\n        return await later;\n    }\n}\n", suspends);
    // EN: D-04: whether a producer suspends does not depend on which synchronous caller is checked
    //     first. `First` opens B before `Main` opens A; A and B await each other and B awaits P, so
    //     `Main` is rejected too.
    // ES: D-04: que un productor se suspenda no depende de que llamador sincrono se revise primero.
    //     `First` abre B antes de que `Main` abra A; A y B se esperan mutuamente y B espera a P, asi
    //     que `Main` tambien se rechaza.
    expectReject(tests, root, "D04CycleOrder", R"ns(
class Program
{
    public static async int P() { yield; return 5; }
    public static async int A(int n) { if (n == 0) return 1; int v = await B(n - 1); return v + 1; }
    public static async int B(int n) { if (n == 0) return 2; int v = await A(n - 1); int w = await P(); return v + w; }
    public static int First() { return await B(1); }
    public static int Main() { return await A(3); }
}
)ns", "declare 'Main' async");
}

/// EN: C-02. A synchronous await of producers that complete without suspending still works,
///     through a direct call, a chain of awaits and a local initialized by the call.
/// ES: C-02. El await sincrono de productores que terminan sin suspenderse sigue funcionando:
///     por llamada directa, por una cadena de awaits y por un local inicializado con la llamada.
void testSynchronousAwaitAccepted(TestContext& tests, const std::filesystem::path& root)
{
    expectRun(tests, root, "C02SynchronousProducers", R"ns(
class Program
{
    public static async int Inner() { return 5; }

    public static async int Read()
    {
        int value = await Inner();
        return value + 1;
    }

    public static int Main()
    {
        Future<int> direct = Inner();
        int first = await direct;
        int second = await Read();
        return first * 10 + second;
    }
}
)ns", 56);
    // EN: D-04: an unknown origin (a parameter) and a local whose every assignment comes from a
    //     producer that does not suspend are accepted and run.
    // ES: D-04: un origen desconocido (un parametro) y un local cuyas asignaciones vienen todas de un
    //     productor que no se suspende se aceptan y se ejecutan.
    expectRun(tests, root, "D04FutureParam", R"ns(
class Program
{
    public static async int Make() { return 4; }
    public static int Use(Future<int> f) { return await f; }
    public static int Main() { return Use(Make()); }
}
)ns", 4);
    expectRun(tests, root, "D04FutureReassigned", R"ns(
class Program
{
    public static async int Make(int v) { return v; }
    public static int Main()
    {
        Future<int> f = Make(1);
        f = Make(9);
        return await f;
    }
}
)ns", 9);
    // EN: DEC-04: an unknown origin that is still pending after the single poll panics (exit 142,
    //     LlvmTrapReason::Panic); it is not iterated to completion.
    // ES: DEC-04: un origen desconocido que sigue pendiente despues del unico poll entra en panico
    //     (salida 142, LlvmTrapReason::Panic); no se itera hasta completar.
    expectRun(tests, root, "D04PendingPanics", R"ns(
class Program
{
    public static async int Producer() { yield; return 5; }
    public static int Use(Future<int> f) { return await f; }
    public static int Main() { return Use(Producer()); }
}
)ns", 142);
}

/// EN: C-02. `static async int Main()` is an entry point and drives a suspending producer to
///     completion, also through Future<void> and through a child holding a `ref` to a local.
/// ES: C-02. `static async int Main()` es punto de entrada y lleva a su fin a un productor que
///     se suspende, tambien por Future<void> y por un hijo que guarda un `ref` a un local.
void testAsyncMainEntryPoint(TestContext& tests, const std::filesystem::path& root)
{
    expectRun(tests, root, "C02AsyncMain", R"ns(
class Program
{
    public static int Hits;

    public static async int Producer()
    {
        yield;
        return 5;
    }

    public static async void Tick()
    {
        yield;
        Hits = Hits + 1;
    }

    public static async void Bump(ref int value)
    {
        yield;
        value = value + 1;
    }

    public static async int Main()
    {
        int value = await Producer();
        Future<void> tick = Tick();
        await tick;
        int counter = 40;
        Future<void> bump = Bump(ref counter);
        await bump;
        return value * 100 + Hits * 10 + counter - 41;
    }
}
)ns", 510);
}

/// EN: C-02. A resumed poll continues where it suspended: code before the suspension does not run
///     again, two awaits keep separate state and storage, a parameter read inside a loop
///     survives every suspension, and awaits nest and repeat inside loops.
/// ES: C-02. Un poll reanudado sigue donde se suspendio: el codigo previo no se repite, dos await
///     guardan estado y almacenamiento propios, un parametro leido en un ciclo sobrevive cada
///     suspension, y los await se anidan y se repiten dentro de ciclos.
void testCoroutineResume(TestContext& tests, const std::filesystem::path& root)
{
    expectRun(tests, root, "C02ResumeTwoAwaits", R"ns(
class Program
{
    public static int Hits;
    public static async int A()
    {
        yield;
        return 1;
    }
    public static async int B()
    {
        yield;
        yield;
        return 2;
    }
    public static async int Main()
    {
        Hits = Hits + 1;
        int v = await A() * 10 + await B();
        return Hits * 100 + v;
    }
}
)ns", 112);
    expectRun(tests, root, "C02ResumeParameterLoop", R"ns(
class Program
{
    public static async int Rep(int n)
    {
        int t = 0;
        while (t < n)
        {
            yield;
            t = t + 1;
        }
        return t * 10 + n;
    }
    public static async int Main()
    {
        return await Rep(3);
    }
}
)ns", 33);
    expectRun(tests, root, "C02ResumeNested", R"ns(
class Program
{
    public static async int Leaf(int x)
    {
        yield;
        return x * 2;
    }
    public static async int Mid(int x)
    {
        int a = await Leaf(x);
        int b = await Leaf(a);
        return a + b;
    }
    public static async int Main()
    {
        int total = 0;
        for (int i = 1; i <= 2; i = i + 1) { total = total + await Mid(i); }
        return total;
    }
}
)ns", 18);
    expectRun(tests, root, "C02ResumeLoopAwait", R"ns(
class Program
{
    public static async int One(int i)
    {
        yield;
        return i + 1;
    }
    public static async int Sum()
    {
        int s = 0;
        for (int i = 0; i < 3; i = i + 1)
        {
            s = s + await One(i);
        }
        return s;
    }
    public static async int Main()
    {
        return await Sum();
    }
}
)ns", 6);
}

/// EN: Freeze 3.0 D-01. A resumed poll re-enters the statement around the await that suspended;
///     every operand evaluated before the await in that statement (a call, an index, a condition,
///     a struct value, an assignment target) runs once, and an await in a `for` or `do` condition
///     resumes in the condition, not at the initializer or the body.
/// ES: Congelamiento 3.0 D-01. Un poll reanudado vuelve a entrar a la sentencia que rodea al await
///     que suspendio; todo operando evaluado antes del await en esa sentencia (una llamada, un
///     indice, una condicion, un valor struct, un destino de asignacion) corre una vez, y un await en
///     la condicion de un `for` o un `do` reanuda en la condicion, no en el inicializador ni el cuerpo.
void testAwaitReplayRunsOperandsOnce(TestContext& tests, const std::filesystem::path& root)
{
    expectRun(tests, root, "D01Prefix", R"ns(
class Program
{
    public static int Hits;
    public static int Count() { Hits = Hits + 1; return 1; }
    public static async int Slow()
    {
        yield;
        return 5;
    }
    public static async int Main()
    {
        int x = Count() + await Slow();
        return Hits * 100 + x;
    }
}
)ns", 106);
    expectRun(tests, root, "D01TwoAwaits", R"ns(
class Program
{
    public static int Hits;
    public static int Count() { Hits = Hits + 1; return 1; }
    public static async int A()
    {
        yield;
        return 1;
    }
    public static async int B()
    {
        yield;
        yield;
        return 2;
    }
    public static async int Main()
    {
        int v = await A() * 10 + Count() + await B();
        return Hits * 100 + v;
    }
}
)ns", 113);
    expectRun(tests, root, "D01Condition", R"ns(
class Program
{
    public static int Hits;
    public static int Count() { Hits = Hits + 1; return 1; }
    public static async int Slow()
    {
        yield;
        return 5;
    }
    public static async int Main()
    {
        int r = 0;
        if (Count() == 1 && await Slow() == 5) { r = 7; }
        return Hits * 100 + r;
    }
}
)ns", 107);
    expectRun(tests, root, "D01StoreIndex", R"ns(
class Program
{
    public static int Hits;
    public static int Next() { Hits = Hits + 1; return Hits - 1; }
    public static async int Slow()
    {
        yield;
        return 5;
    }
    public static async int Main()
    {
        int[] values = new int[4];
        values[Next()] = await Slow();
        int r = Hits * 100 + values[0] * 10 + values[1];
        delete[] values;
        return r;
    }
}
)ns", 150);
    expectRun(tests, root, "D01Loop", R"ns(
class Program
{
    public static int Hits;
    public static int Count() { Hits = Hits + 1; return 1; }
    public static async int Slow()
    {
        yield;
        return 2;
    }
    public static async int Main()
    {
        int total = 0;
        for (int i = 0; i < 3; i = i + 1)
        {
            total = total + Count() * await Slow();
        }
        return Hits * 100 + total;
    }
}
)ns", 306);
    expectRun(tests, root, "D01ForCondition", R"ns(
class Program
{
    public static int Hits;
    public static int Count() { Hits = Hits + 1; return 1; }
    public static async bool Less(int i)
    {
        yield;
        return i < 3;
    }
    public static async int Main()
    {
        int total = 0;
        for (int i = 0; await Less(i); i = i + 1)
        {
            total = total + Count();
        }
        return Hits * 10 + total;
    }
}
)ns", 33);
    expectRun(tests, root, "D01DoWhileCondition", R"ns(
class Program
{
    public static int Hits;
    public static int Count() { Hits = Hits + 1; return 1; }
    public static async bool Less(int i)
    {
        yield;
        return i < 3;
    }
    public static async int Main()
    {
        int n = 0;
        do
        {
            n = n + Count();
        }
        while (await Less(n));
        return Hits * 10 + n;
    }
}
)ns", 33);
    expectRun(tests, root, "D01Arguments", R"ns(
class Program
{
    public static int Hits;
    public static int Count() { Hits = Hits + 1; return 1; }
    public static int Add3(int a, int b, int c) { return a + b + c; }
    public static async int Slow()
    {
        yield;
        return 5;
    }
    public static async int Main()
    {
        int v = Add3(Count(), await Slow(), Count());
        return Hits * 100 + v;
    }
}
)ns", 207);
    expectRun(tests, root, "D01Conditional", R"ns(
class Program
{
    public static int Hits;
    public static int Count() { Hits = Hits + 1; return 1; }
    public static bool Flag() { Hits = Hits + 1; return true; }
    public static async int Slow()
    {
        yield;
        return 5;
    }
    public static async int Main()
    {
        int v = Flag() ? Count() + await Slow() : 0;
        return Hits * 100 + v;
    }
}
)ns", 206);
    expectRun(tests, root, "D01StructOperand", R"ns(
struct Pair
{
    public int A;
    public int B;
}
class Program
{
    public static int Hits;
    public static Pair Make()
    {
        Hits = Hits + 1;
        Pair p;
        p.A = 1;
        p.B = 2;
        return p;
    }
    public static int Sum(Pair p, int x) { return p.A + p.B + x; }
    public static async int Slow()
    {
        yield;
        return 5;
    }
    public static async int Main()
    {
        int v = Sum(Make(), await Slow());
        return Hits * 100 + v;
    }
}
)ns", 108);
    expectRun(tests, root, "D01FieldTarget", R"ns(
class Holder
{
    public int X;
}
class Program
{
    public static int Hits;
    public static Holder Pick(Holder h) { Hits = Hits + 1; return h; }
    public static async int Slow()
    {
        yield;
        return 7;
    }
    public static async int Main()
    {
        raii Holder h = new Holder();
        Pick(h).X = await Slow();
        return Hits * 100 + h.X;
    }
}
)ns", 107);
}

/// EN: C-02. A local of a coroutine lives in its frame: a raii local is destroyed once after the
///     resume, and a borrowed view of it stays valid across the suspension.
/// ES: C-02. Un local de una corrutina vive en su frame: un local raii se destruye una vez tras
///     la reanudacion, y una vista prestada de el sigue valida a traves de la suspension.
void testCoroutineFrameLocals(TestContext& tests, const std::filesystem::path& root)
{
    expectRun(tests, root, "C02FrameRaii", R"ns(
class Box
{
    public static int Drops;
    public Box() { }
    ~Box() { Drops = Drops + 1; }
}
class Program
{
    public static async int Read()
    {
        raii Box box = new Box();
        yield;
        return 42;
    }

    public static async int Main()
    {
        int result = await Read();
        return Box.Drops * 100 + result;
    }
}
)ns", 142);
    expectRun(tests, root, "C02FrameBorrow", R"ns(
class Box { public int Value; public Box() { Value = 10; } }
class Program
{
    public static async int Read()
    {
        raii Box box = new Box();
        borrowed Box view = box;
        yield;
        return view.Value;
    }

    public static async int Main()
    {
        return await Read();
    }
}
)ns", 10);
}

/// EN: C-02. A generator resumes after its yield: the prefix runs once and a yield inside a loop
///     ends the iteration.
/// ES: C-02. Un generador reanuda tras su yield: el prefijo corre una vez y un yield dentro de un
///     ciclo termina la iteracion.
void testGeneratorResume(TestContext& tests, const std::filesystem::path& root)
{
    expectRun(tests, root, "C02GeneratorPrefix", R"ns(
class Program
{
    public static int Log;
    public static Iterator<int> Gen()
    {
        Log = Log + 1;
        yield return 1;
        Log = Log + 10;
        yield return 2;
        Log = Log + 100;
    }
    public static int Main()
    {
        int sum = 0;
        foreach (var x in Gen()) { sum = sum * 10 + x; }
        return sum * 1000 + Log;
    }
}
)ns", 12111);
    expectRun(tests, root, "C02GeneratorLoop", R"ns(
class Program
{
    public static int Log;
    public static Iterator<int> Gen()
    {
        Log = Log + 1;
        for (int i = 1; i < 4; i = i + 1)
        {
            yield return i;
        }
    }
    public static int Main()
    {
        int sum = 0;
        foreach (var x in Gen()) { sum = sum * 10 + x; }
        return sum * 10 + Log;
    }
}
)ns", 1231);
}

/// EN: C-02. An abandoned generator runs the finally blocks and destructions active at its yield
///     exactly once; one never started runs none; a natural end or `yield break` runs them once.
/// ES: C-02. Un generador abandonado corre una sola vez los finally y destrucciones activos en su
///     yield; uno que nunca empezo no corre ninguno; un fin natural o `yield break` los corre una
///     vez.
void testGeneratorEarlyStop(TestContext& tests, const std::filesystem::path& root)
{
    expectRun(tests, root, "C02GeneratorBreak", R"ns(
class Program
{
    public static int Fin;
    public static Iterator<int> Gen()
    {
        try
        {
            for (int i = 1; i < 10; i = i + 1) { yield return i; }
        }
        finally { Fin = Fin + 1; }
    }
    public static int Main()
    {
        int sum = 0;
        foreach (var x in Gen()) { if (x == 4) break; sum = sum + x; }
        return sum * 10 + Fin;
    }
}
)ns", 61);
    expectRun(tests, root, "C02GeneratorNotStarted", R"ns(
class Program
{
    public static int Fin;
    public static Iterator<int> Gen()
    {
        try { yield return 1; }
        finally { Fin = Fin + 1; }
    }
    public static int Main()
    {
        Iterator<int> it = Gen();
        it.Drop();
        return Fin + 5;
    }
}
)ns", 5);
    expectRun(tests, root, "C02GeneratorNatural", R"ns(
class Program
{
    public static int Fin;
    public static Iterator<int> Gen()
    {
        try
        {
            for (int i = 1; i < 4; i = i + 1) { yield return i; }
        }
        finally { Fin = Fin + 1; }
    }
    public static int Main()
    {
        int sum = 0;
        foreach (var x in Gen()) { sum = sum + x; }
        return sum * 10 + Fin;
    }
}
)ns", 61);
    expectRun(tests, root, "C02GeneratorRaii", R"ns(
class Res
{
    public static int Drops;
    public Res() { }
    ~Res() { Drops = Drops + 1; }
}
class Program
{
    public static Iterator<int> Gen()
    {
        raii Res r = new Res();
        yield return 1;
        yield return 2;
    }
    public static int Main()
    {
        int seen = 0;
        foreach (var x in Gen()) { seen = seen + x; break; }
        int after = Res.Drops;
        foreach (var y in Gen()) { seen = seen + y; }
        return seen * 100 + after * 10 + Res.Drops;
    }
}
)ns", 412);
    expectRun(tests, root, "C02GeneratorYieldBreak", R"ns(
class Program
{
    public static int Fin;
    public static Iterator<int> Gen()
    {
        try
        {
            yield return 1;
            yield break;
        }
        finally { Fin = Fin + 1; }
    }
    public static int Main()
    {
        int sum = 0;
        foreach (var x in Gen()) { sum = sum + x; }
        return sum * 10 + Fin;
    }
}
)ns", 11);
}

/// EN: Runs the driver on `arguments`; `text` gets its output and error streams.
/// ES: Corre el driver con `arguments`; `text` recibe su salida y su error.
ExitCode runDriver(std::span<const std::string_view> arguments, std::string& text)
{
    Driver driver;
    std::ostringstream output;
    std::ostringstream error;
    const ExitCode code = driver.run(arguments, output, error);
    text = output.str() + error.str();
    return code;
}

/// EN: C-05. A source file compiles as its implicit project: the same source produces the same
///     MIR and LLVM as an explicit project with the same name, and both programs run alike. The
///     source mixes the 3.x surface the 1.x file pipeline lacked: a variant `match`, a lambda
///     with a capture, interpolation and an `async Main` that awaits a suspending call (whose
///     captured local must survive the suspension).
/// ES: C-05. Un archivo fuente compila como su proyecto implicito: el mismo fuente produce el
///     mismo MIR y LLVM que un proyecto explicito con el mismo nombre, y ambos programas corren
///     igual. El fuente mezcla la superficie 3.x que el pipeline 1.x de archivo no tenia: un
///     `match` de variant, una lambda con captura, interpolacion y un `async Main` que espera una
///     llamada que se suspende (cuyo local capturado debe sobrevivir a la suspension).
void testFileModeMatchesProject(TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path directory = root / "C05FileMode";
    const std::filesystem::path source = directory / "Main.ns";
    const std::filesystem::path project = writeProject(directory, "Main");
    writeFile(source, R"ns(
using NativeSharp.Core;

delegate int Transform(int value);

variant Shape { Circle(int), Square(int) }

class Program
{
    private static int Area(Shape shape)
    {
        match (shape)
        {
            Circle(var r) => { return 3 * r * r; },
            Square(var s) => { return s * s; }
        }
    }

    private static async int Twice(int value)
    {
        yield;
        return value * 2;
    }

    public static async int Main()
    {
        int bias = 1;
        raii Transform addBias = [copy bias] (int value) => value + bias;
        raii string text = $"area={Area(Shape.Square(3))}";
        int doubled = await Twice(addBias(text.Length));
        return doubled + addBias(0) - bias;
    }
}
)ns");
    const std::string sourceText = source.string();
    const std::string projectText = project.string();
    const std::array<std::pair<std::string_view, std::string_view>, 2> dumps = {{
        {"--dump-mir", "--dump-project-oop-mir"}, {"--dump-llvm", "--dump-project-oop-llvm"}}};
    for (const auto& [fileAction, projectAction] : dumps)
    {
        std::string fileOutput;
        std::string projectOutput;
        const std::array<std::string_view, 2> fileArguments = {fileAction, sourceText};
        const std::array<std::string_view, 2> projectArguments = {projectAction, projectText};
        const ExitCode fileCode = runDriver(fileArguments, fileOutput);
        const ExitCode projectCode = runDriver(projectArguments, projectOutput);
        tests.expect(fileCode == ExitCode::Success && projectCode == ExitCode::Success &&
                fileOutput == projectOutput,
            std::string(fileAction) + " of the file equals " + std::string(projectAction) +
                " of the project: " + fileOutput.substr(0, 400));
    }
    const std::array<std::string_view, 2> levels = {"-O0", "-O2"};
    for (const std::string_view level : levels)
    {
        const std::filesystem::path executable =
            directory / ("file" + std::string(level.substr(1)) + ".exe");
        const std::string executableText = executable.string();
        const std::array<std::string_view, 5> arguments = {
            level, "--emit-exe", sourceText, "-o", executableText};
        std::string diagnostic;
        const ExitCode code = runDriver(arguments, diagnostic);
        tests.expect(code == ExitCode::Success,
            "C05FileMode [" + std::string(level) + "]: the file compiles: " + diagnostic);
        if (code != ExitCode::Success) continue;
        const std::string command = "\"\"" + executableText + "\"\"";
        const int observed = std::system(command.c_str());
        tests.expect(observed == 14,
            "C05FileMode [" + std::string(level) + "]: exits 14, observed " + std::to_string(observed));
    }
}

/// EN: C-05 (found while porting the file-mode cases). A local a lambda captures by reference in a
///     coroutine keeps one cell across suspensions: the lambda made before an `await` (or a
///     `yield return`) and the code after it share the variable.
/// ES: C-05 (hallazgo al portar los casos de modo archivo). Un local que una lambda captura por
///     referencia en una corrutina conserva una sola celda entre suspensiones: la lambda creada
///     antes de un `await` (o un `yield return`) y el codigo posterior comparten la variable.
void testCaptureSurvivesSuspension(TestContext& tests, const std::filesystem::path& root)
{
    expectRun(tests, root, "C05CaptureAcrossAwait", R"ns(
delegate int Transform(int value);

class Program
{
    private static async int Twice(int value)
    {
        yield;
        return value * 2;
    }

    public static async int Main()
    {
        int counter = 1;
        raii Transform bump = [ref mut counter] (int value) =>
        {
            counter = counter + value;
            return counter;
        };
        int before = bump(1);
        int doubled = await Twice(before);
        int after = bump(10);
        return after + counter + doubled;
    }
}
)ns", 28);
    expectRun(tests, root, "C05CaptureAcrossYield", R"ns(
delegate int Transform(int value);

class Program
{
    public static Iterator<int> Gen()
    {
        int counter = 1;
        raii Transform bump = [ref mut counter] (int value) =>
        {
            counter = counter + value;
            return counter;
        };
        yield return bump(1);
        yield return bump(10);
        yield return counter;
    }

    public static int Main()
    {
        int sum = 0;
        foreach (var x in Gen()) { sum = sum * 100 + x; }
        return sum % 251;
    }
}
)ns", 128);
}

/// EN: C-05 (found while porting the file-mode cases). A method that can fall off its end without
///     a value is rejected; an exhaustive `match` whose arms all return or panic, an `async void`
///     method and an `async int` method that returns are accepted.
/// ES: C-05 (hallazgo al portar los casos de modo archivo). Un metodo que puede llegar a su final
///     sin valor se rechaza; un `match` exhaustivo cuyos brazos retornan o hacen panic, un metodo
///     `async void` y un metodo `async int` que retorna se aceptan.
void testMissingReturn(TestContext& tests, const std::filesystem::path& root)
{
    expectReject(tests, root, "C05MissingReturn", R"ns(
class Program
{
    private static int Pick(bool flag)
    {
        if (flag)
        {
            return 1;
        }
    }

    public static int Main()
    {
        return Pick(true);
    }
}
)ns", "not all reachable control paths return a value from function 'Pick'");
    expectRun(tests, root, "C05EveryPathReturns", R"ns(
variant Decision { Value(int), Stop }

class Program
{
    public static int Hits;

    private static int Pick(Decision decision)
    {
        match (decision)
        {
            Value(var value) => { return value; },
            Stop => { panic("stopped"); }
        }
    }

    private static int Sign(int value)
    {
        match (value)
        {
            0 => { return 0; },
            _ => { return 1; }
        }
    }

    private static async void Bump()
    {
        yield;
        Hits = Hits + 1;
    }

    private static async int Later()
    {
        yield;
        return 1;
    }

    public static async int Main()
    {
        await Bump();
        int later = await Later();
        return Pick(Decision.Value(39)) + Sign(5) + Hits + later;
    }
}
)ns", 42);
}

/// EN: C-05 (found while porting the file-mode cases). An `extern C` import outside a class and a
///     variable outside every namespace were 1.x file-mode forms; 3.x rejects both by name instead
///     of failing later with an unknown identifier.
/// ES: C-05 (hallazgo al portar los casos de modo archivo). Un import `extern C` fuera de una clase
///     y una variable fuera de todo namespace eran formas del modo archivo 1.x; 3.x rechaza ambas
///     por nombre en vez de fallar despues con un identificador desconocido.
void testGlobalDeclarationsRejected(TestContext& tests, const std::filesystem::path& root)
{
    expectReject(tests, root, "C05GlobalExtern", R"ns(
extern C int MissingNativeSymbol();

class Program
{
    public static int Main()
    {
        return 0;
    }
}
)ns", "an 'extern C' import outside a class is not supported in NativeSharp 3.0");
    expectReject(tests, root, "C05GlobalVariable", R"ns(
int Counter;

class Program
{
    public static int Main()
    {
        return 0;
    }
}
)ns", "a variable outside every namespace is not supported in NativeSharp 3.0");
}

/// EN: C-11. A struct with a destructor or owned fields, used as a local, is destroyed exactly
///     once on every way out of its scope (in reverse declaration order), and not at all after a
///     move.
/// ES: C-11. Un struct con destructor o campos owned, usado como local, se destruye exactamente
///     una vez en cada salida de su scope (en orden inverso de declaracion), y nunca despues de
///     un move.
constexpr std::string_view kGuardPrelude = R"ns(
using NativeSharp.Core;
class Counter { public static int Drops; }
struct Guard
{
    public int Id;
    public int GetId() { return Id; }
    ~Guard() { Counter.Drops = Counter.Drops * 10 + Id; }
}
)ns";

void testValueLocalLifetimes(TestContext& tests, const std::filesystem::path& root)
{
    expectRun(tests, root, "C11ScopeOrder", std::string(kGuardPrelude) + R"ns(
class Program
{
    private static int Scope()
    {
        Guard a;
        a.Id = 1;
        {
            Guard b;
            b.Id = 2;
        }
        return 7;
    }
    public static int Main()
    {
        int r = Scope();
        return Counter.Drops * 10 + r;
    }
}
)ns", 217);
    expectRun(tests, root, "C11EarlyReturn", std::string(kGuardPrelude) + R"ns(
class Program
{
    private static int Pick(bool early)
    {
        Guard g;
        g.Id = 6;
        if (early) { return 1; }
        Guard h;
        h.Id = 7;
        return 2;
    }
    public static int Main()
    {
        int a = Pick(true);
        int b = Pick(false);
        return Counter.Drops * 10 + a + b;
    }
}
)ns", 6763);
    expectRun(tests, root, "C11Exception", std::string(kGuardPrelude) + R"ns(
class Boom : Exception { public Boom() { } }
class Program
{
    private static void Risky() throws Boom
    {
        Guard g;
        g.Id = 5;
        throw new Boom();
    }
    public static int Main()
    {
        try { Risky(); }
        catch (Boom problem) { return Counter.Drops; }
        return 99;
    }
}
)ns", 5);
    expectRun(tests, root, "C11ConditionalMove", R"ns(
class Widget
{
    public static int Drops;
    public int Id;
    public Widget(int id) { Id = id; }
    ~Widget() { Drops = Drops * 10 + Id; }
}
struct Holder
{
    public owned Widget Item;
}
class Program
{
    private static void Take(Holder h) { }
    private static void Run(bool give, int id)
    {
        Holder x;
        x.Item = new Widget(id);
        if (give) { Take(move(x)); }
    }
    public static int Main()
    {
        Run(true, 5);
        Run(false, 6);
        return Widget.Drops;
    }
}
)ns", 56);
}

/// EN: C-11 (found while probing). A struct with a destructor used to be copied silently, so a
///     returned local was destroyed in the callee and again in the caller. It is move-only now:
///     `return move(g)` hands it over once, assignment destroys the old value once, and a plain
///     copy is rejected.
/// ES: C-11 (hallazgo de la sonda). Un struct con destructor se copiaba en silencio, asi que un
///     local retornado se destruia en el llamado y otra vez en el llamador. Ahora es move-only:
///     `return move(g)` lo entrega una vez, la asignacion destruye el valor anterior una vez, y
///     una copia simple se rechaza.
void testValueMoves(TestContext& tests, const std::filesystem::path& root)
{
    expectRun(tests, root, "C11ReturnAndAssign", std::string(kGuardPrelude) + R"ns(
class Program
{
    private static Guard Make(int id)
    {
        Guard g;
        g.Id = id;
        return move(g);
    }
    private static int Run()
    {
        Guard x = Make(1);
        x = Make(2);
        return Counter.Drops * 10 + x.Id;
    }
    public static int Main()
    {
        int seen = Run();
        return seen * 100 + Counter.Drops;
    }
}
)ns", 1212);
    expectReject(tests, root, "C11CopyRejected", std::string(kGuardPrelude) + R"ns(
class Program
{
    public static int Main()
    {
        Guard x;
        x.Id = 3;
        Guard y = x;
        return y.Id;
    }
}
)ns", "cannot be copied into local 'y'; use move(source)");
}

/// EN: C-11. A struct enumerator with a destructor is accepted by `foreach` and destroyed once on
///     the natural end, on `break` and on `return`.
/// ES: C-11. Un enumerador struct con destructor se acepta en `foreach` y se destruye una vez al
///     terminar, con `break` y con `return`.
void testStructEnumerator(TestContext& tests, const std::filesystem::path& root)
{
    expectRun(tests, root, "C11StructEnumerator", R"ns(
class Counter { public static int Drops; }
struct RangeEnumerator
{
    public int Value;
    public int Limit;
    public int Current { get { return Value; } }
    public bool MoveNext()
    {
        Value = Value + 1;
        return Value <= Limit;
    }
    ~RangeEnumerator() { Counter.Drops = Counter.Drops + 1; }
}
struct Range
{
    public int Limit;
    public RangeEnumerator GetEnumerator()
    {
        RangeEnumerator e;
        e.Value = 0;
        e.Limit = Limit;
        return move(e);
    }
}
class Program
{
    private static int FirstOver(Range r, int bound)
    {
        foreach (var x in r)
        {
            if (x > bound) { return x; }
        }
        return 0;
    }
    public static int Main()
    {
        Range r;
        r.Limit = 5;
        int all = 0;
        foreach (var x in r) { all = all + x; }
        int some = 0;
        foreach (var x in r)
        {
            if (x == 3) { break; }
            some = some + x;
        }
        int first = FirstOver(r, 1);
        return all * 1000 + some * 100 + first * 10 + Counter.Drops;
    }
}
)ns", 15323);
}

/// EN: C-11 (found while probing). A struct field with a destructor is destroyed with its object,
///     and the result of a call that hands over something to destroy (a struct with a destructor
///     or an owned object) is destroyed when it is discarded or only used as a receiver.
/// ES: C-11 (hallazgo de la sonda). Un campo struct con destructor se destruye con su objeto, y el
///     resultado de una llamada que entrega algo que destruir (un struct con destructor o un
///     objeto owned) se destruye cuando se descarta o solo se usa como receptor.
void testValueFieldsAndTemporaries(TestContext& tests, const std::filesystem::path& root)
{
    expectRun(tests, root, "C11ObjectValueField", std::string(kGuardPrelude) + R"ns(
class Holder
{
    public Guard Item;
    public Holder(int id) { Item.Id = id; }
}
class Program
{
    public static int Main()
    {
        {
            raii Holder h = new Holder(8);
        }
        return Counter.Drops;
    }
}
)ns", 8);
    expectRun(tests, root, "C11Temporaries", std::string(kGuardPrelude) + R"ns(
class Widget
{
    public int Id;
    public Widget(int id) { Id = id; }
    public int GetId() { return Id; }
    ~Widget() { Counter.Drops = Counter.Drops * 10 + Id; }
}
class Program
{
    private static Guard Make(int id)
    {
        Guard g;
        g.Id = id;
        return move(g);
    }
    private static owned Widget MakeWidget(int id) { return new Widget(id); }
    public static int Main()
    {
        int a = Make(1).GetId();
        int b = Make(2).Id;
        Make(3);
        int c = MakeWidget(4).GetId();
        int d = MakeWidget(5).Id;
        MakeWidget(6);
        return Counter.Drops + a + b + c + d - 12;
    }
}
)ns", 123456);
}

void testValueArrayElements(TestContext& tests, const std::filesystem::path& root)
{
    expectRun(tests, root, "C12RaiiStruct", std::string(kGuardPrelude) + R"ns(
class Program
{
    public static int Main()
    {
        {
            raii Guard[] arr = new Guard[2];
            arr[0].Id = 1;
            arr[1].Id = 2;
        }
        return Counter.Drops;
    }
}
)ns", 21);
    expectRun(tests, root, "C12Delete", std::string(kGuardPrelude) + R"ns(
class Program
{
    public static int Main()
    {
        Guard[] arr = new Guard[2];
        arr[0].Id = 1;
        arr[1].Id = 2;
        delete[] arr;
        return Counter.Drops;
    }
}
)ns", 21);
    expectRun(tests, root, "C12OwnedField", std::string(kGuardPrelude) + R"ns(
class Widget
{
    public int Id;
    public Widget(int id) { Id = id; }
    ~Widget() { Counter.Drops = Counter.Drops * 10 + Id; }
}
struct Box
{
    public owned Widget Item;
}
class Program
{
    public static int Main()
    {
        {
            raii Box[] arr = new Box[2];
            arr[0].Item = new Widget(1);
            arr[1].Item = new Widget(2);
        }
        return Counter.Drops;
    }
}
)ns", 21);
    // EN: Freeze 3.0 DEC-11: the first stores into the never-written slots and the moved-out slot
    //     destroy nothing; this was 10302 while those zero values were destroyed too.
    // ES: Congelamiento 3.0 DEC-11: los primeros stores en slots nunca escritos y el slot movido no
    //     destruyen nada; daba 10302 mientras esos valores cero tambien se destruian.
    expectRun(tests, root, "C12AssignAndMove", std::string(kGuardPrelude) + R"ns(
class Program
{
    private static Guard Make(int id)
    {
        Guard g;
        g.Id = id;
        return move(g);
    }
    public static int Main()
    {
        {
            raii Guard[] arr = new Guard[2];
            arr[0] = Make(1);
            arr[0] = Make(2);
            arr[1] = Make(4);
            Guard taken = move(arr[1]);
            taken.Id = 3;
        }
        return Counter.Drops;
    }
}
)ns", 132);
}

/// EN: Freeze 3.0 DEC-11. An array of structs with a destructor keeps one live bit per element. The
///     slots of `new T[n]` start dead; the first whole write or field write makes one alive (a field
///     write zeroes the other fields first); move and delete make it dead; only live elements are
///     destroyed; reading or moving a dead element traps (exit 146).
/// ES: Congelamiento 3.0 DEC-11. Un arreglo de structs con destructor guarda un bit de vida por
///     elemento. Los slots de `new T[n]` nacen muertos; la primera escritura completa o de campo
///     revive uno (la de campo pone en cero los demas campos antes); move y delete lo matan; solo se
///     destruyen los vivos; leer o mover un elemento muerto atrapa (exit 146).
void testArrayElementLiveness(TestContext& tests, const std::filesystem::path& root)
{
    expectRun(tests, root, "DEC11DeadSlotDrop", R"ns(
class Counter { public static int Drops; }
struct Guard
{
    public int Id;
    ~Guard() { Counter.Drops = Counter.Drops * 10 + Id + 1; }
}
class Program
{
    private static void Run()
    {
        raii Guard[] items = new Guard[2];
        items[0].Id = 1;
        items[1].Id = 2;
        delete items[0];
    }
    public static int Main()
    {
        Run();
        return Counter.Drops;
    }
}
)ns", 23);
    expectRun(tests, root, "DEC11Rectangular", R"ns(
class Counter { public static int Drops; }
struct Guard
{
    public int Id;
    ~Guard() { Counter.Drops = Counter.Drops + 1; }
}
class Program
{
    private static void Run()
    {
        raii Guard[,] grid = new Guard[2, 3];
        grid[1, 2].Id = 5;
    }
    public static int Main()
    {
        Run();
        return Counter.Drops;
    }
}
)ns", 1);
    expectRun(tests, root, "DEC11WholeStore", R"ns(
class Counter { public static int Drops; }
struct Guard
{
    public int Id;
    ~Guard() { Counter.Drops = Counter.Drops * 10 + Id + 1; }
}
class Program
{
    private static void Run()
    {
        raii Guard[] a = new Guard[3];
        a[0].Id = 1;
        Guard g;
        g.Id = 7;
        a[0] = move(g);
    }
    public static int Main()
    {
        Run();
        return Counter.Drops;
    }
}
)ns", 28);
    expectRun(tests, root, "DEC11Move", R"ns(
class Counter { public static int Drops; }
struct Guard
{
    public int Id;
    ~Guard() { Counter.Drops = Counter.Drops * 10 + Id + 1; }
}
class Program
{
    private static void Run()
    {
        raii Guard[] a = new Guard[2];
        a[0].Id = 1;
        a[1].Id = 2;
        Guard g = move(a[1]);
    }
    public static int Main()
    {
        Run();
        return Counter.Drops;
    }
}
)ns", 32);
    expectRun(tests, root, "DEC11Revive", R"ns(
class Counter { public static int Drops; }
struct Guard
{
    public int Id;
    ~Guard() { Counter.Drops = Counter.Drops * 10 + Id + 1; }
}
class Program
{
    private static void Run()
    {
        raii Guard[] a = new Guard[2];
        a[0].Id = 1;
        a[1].Id = 2;
        delete a[0];
        Guard g;
        g.Id = 5;
        a[0] = move(g);
    }
    public static int Main()
    {
        Run();
        return Counter.Drops;
    }
}
)ns", 236);
    expectRun(tests, root, "DEC11OwnedDelete", R"ns(
class Counter { public static int Drops; }
struct Guard
{
    public int Id;
    ~Guard() { Counter.Drops = Counter.Drops * 10 + Id + 1; }
}
class Program
{
    private static void Run()
    {
        owned Guard[] a = new Guard[2];
        a[0].Id = 1;
        a[1].Id = 2;
        delete a[1];
        delete[] a;
    }
    public static int Main()
    {
        Run();
        return Counter.Drops;
    }
}
)ns", 32);
    expectRun(tests, root, "DEC11OwnedField", R"ns(
class Counter { public static int Drops; }
struct Guard
{
    public int Id;
    ~Guard() { Counter.Drops = Counter.Drops * 10 + Id + 1; }
}
class Holder
{
    public owned Guard[] Items;
    public Holder() { Items = new Guard[2]; }
}
class Program
{
    private static void Run()
    {
        raii Holder h = new Holder();
        h.Items[0].Id = 1;
        h.Items[1].Id = 2;
        Guard g = move(h.Items[0]);
    }
    public static int Main()
    {
        Run();
        return Counter.Drops;
    }
}
)ns", 23);
    expectRun(tests, root, "DEC11DeadRead", R"ns(
class Counter { public static int Drops; }
struct Guard
{
    public int Id;
    public int GetId() { return Id; }
    ~Guard() { Counter.Drops = Counter.Drops * 10 + Id + 1; }
}
class Program
{
    public static int Main()
    {
        raii Guard[] a = new Guard[2];
        a[0].Id = 4;
        return a[1].Id;
    }
}
)ns", 146);
    expectRun(tests, root, "DEC11MoveDead", R"ns(
class Counter { public static int Drops; }
struct Guard
{
    public int Id;
    public int GetId() { return Id; }
    ~Guard() { Counter.Drops = Counter.Drops * 10 + Id + 1; }
}
class Program
{
    public static int Main()
    {
        raii Guard[] a = new Guard[2];
        Guard g = move(a[1]);
        return 7;
    }
}
)ns", 146);
    expectRun(tests, root, "DEC11Initializer", R"ns(
class Counter { public static int Drops; }
struct Guard
{
    public int Id;
    public int GetId() { return Id; }
    ~Guard() { Counter.Drops = Counter.Drops * 10 + Id + 1; }
}
class Program
{
    private static Guard Make(int id)
    {
        Guard g;
        g.Id = id;
        return move(g);
    }
    private static void Run()
    {
        raii Guard[] a = new Guard[] { Make(1), Make(2) };
    }
    public static int Main()
    {
        Run();
        return Counter.Drops;
    }
}
)ns", 32);
    expectRun(tests, root, "DEC11DoubleDelete", R"ns(
class Counter { public static int Drops; }
struct Guard
{
    public int Id;
    public int GetId() { return Id; }
    ~Guard() { Counter.Drops = Counter.Drops * 10 + Id + 1; }
}
class Program
{
    private static void Run()
    {
        raii Guard[] a = new Guard[2];
        a[0].Id = 1;
        delete a[0];
        delete a[0];
        delete a[1];
    }
    public static int Main()
    {
        Run();
        return Counter.Drops;
    }
}
)ns", 2);
    expectRun(tests, root, "DEC11DeadReceiver", R"ns(
class Counter { public static int Drops; }
struct Guard
{
    public int Id;
    public int GetId() { return Id; }
    ~Guard() { Counter.Drops = Counter.Drops * 10 + Id + 1; }
}
class Program
{
    public static int Main()
    {
        raii Guard[] a = new Guard[2];
        a[0].Id = 5;
        return a[0].GetId() * 10 + a[1].GetId();
    }
}
)ns", 146);
    expectRun(tests, root, "DEC11ListReverse", R"ns(
using NativeSharp.Core;
using NativeSharp.Core.Collections.Generic;
class Counter { public static int Drops; public static int Sum; }
struct Guard
{
    public int Id;
    ~Guard() { Counter.Drops = Counter.Drops + 1; Counter.Sum = Counter.Sum + Id; }
}
class Program
{
    private static Guard Make(int id)
    {
        Guard g;
        g.Id = id;
        return move(g);
    }
    public static int Main()
    {
        {
            raii List<Guard> list = new List<Guard>();
            list.Add(Make(3));
            list.Add(Make(1));
            list.Add(Make(2));
            list.Reverse();
        }
        return Counter.Drops * 100 + Counter.Sum;
    }
}
)ns", 306, true);
    expectRun(tests, root, "DEC11ListAdd", R"ns(
using NativeSharp.Core;
using NativeSharp.Core.Collections.Generic;
class Counter { public static int Drops; }
struct Guard
{
    public int Id;
    ~Guard() { Counter.Drops = Counter.Drops * 10 + Id + 1; }
}
class Program
{
    private static Guard Make(int id)
    {
        Guard g;
        g.Id = id;
        return move(g);
    }
    public static int Main()
    {
        {
            raii List<Guard> list = new List<Guard>();
            list.Add(Make(3));
            list.Add(Make(1));
        }
        return Counter.Drops;
    }
}
)ns", 42, true);
    expectRun(tests, root, "DEC11AwaitFieldStore", R"ns(
class Counter { public static int Drops; public static int Hits; }
struct Guard
{
    public int Id;
    ~Guard() { Counter.Drops = Counter.Drops * 10 + Id + 1; }
}
class Program
{
    public static int Next() { Counter.Hits = Counter.Hits + 1; return 1; }
    public static async int Slow()
    {
        yield;
        return 5;
    }
    public static async int Run()
    {
        raii Guard[] a = new Guard[2];
        a[Next()].Id = await Slow();
        return Counter.Hits * 10 + a[1].Id;
    }
    public static async int Main()
    {
        int r = await Run();
        return r * 100 + Counter.Drops;
    }
}
)ns", 1506);
    expectRun(tests, root, "DEC11AwaitWholeStore", R"ns(
class Counter { public static int Drops; public static int Hits; }
struct Guard
{
    public int Id;
    ~Guard() { Counter.Drops = Counter.Drops * 10 + Id + 1; }
}
class Program
{
    public static int Next() { Counter.Hits = Counter.Hits + 1; return 1; }
    public static async Guard Make()
    {
        yield;
        Guard g;
        g.Id = 5;
        return move(g);
    }
    public static async int Run()
    {
        raii Guard[] a = new Guard[2];
        a[Next()] = await Make();
        return Counter.Hits * 10 + a[1].Id;
    }
    public static async int Main()
    {
        int r = await Run();
        return r * 100 + Counter.Drops;
    }
}
)ns", 1506);
}

// EN: List<T> of a struct with a destructor drops each live element exactly once, also across
//     growth and RemoveAt; a removed slot is left dead (Freeze 3.0 DEC-11), so shifting the next
//     element into it destroys nothing: C12ListRemove is 2213 (it was 22013 while the dead slot's
//     zero value was destroyed too).
// ES: List<T> de un struct con destructor destruye cada elemento vivo una sola vez, tambien al
//     crecer y en RemoveAt; un slot removido queda muerto (Congelamiento 3.0 DEC-11), asi mover el
//     siguiente elemento a el no destruye nada: C12ListRemove da 2213 (daba 22013 mientras el valor
//     cero del slot muerto tambien se destruia).
void testValueListElements(TestContext& tests, const std::filesystem::path& root)
{
    const std::string prelude = "using NativeSharp.Core.Collections.Generic;" +
        std::string(kGuardPrelude) +
        "class Factory { public static Guard Make(int id) { Guard g; g.Id = id; return move(g); } }\n";
    expectRun(tests, root, "C12ListGrow", prelude + R"ns(
class Program
{
    public static int Main()
    {
        {
            raii List<Guard> list = new List<Guard>();
            int i = 1;
            while (i <= 5) { list.Add(Factory.Make(i)); i = i + 1; }
        }
        return Counter.Drops;
    }
}
)ns", 12345, true);
    expectRun(tests, root, "C12ListRemove", prelude + R"ns(
class Program
{
    public static int Main()
    {
        int afterRemove = 0;
        {
            raii List<Guard> list = new List<Guard>();
            list.Add(Factory.Make(1));
            list.Add(Factory.Make(2));
            list.Add(Factory.Make(3));
            list.RemoveAt(1);
            afterRemove = Counter.Drops;
        }
        return afterRemove * 1000 + Counter.Drops;
    }
}
)ns", 2213, true);
    // EN: LinkedList<T> keeps each value in an `owned T[]` slot of one element; the removed value
    //     belongs to the caller and every remaining value is dropped once by Clear().
    //     Since P-1, RemoveFirst declares `throws InvalidOperationException`, so the call sits in a
    //     minimal try/catch (authorized, P9); the removed value now ends with the try block, so
    //     `before` is 1 and the result is 1123 (it was 123 before the call had to be handled).
    // ES: LinkedList<T> guarda cada valor en un slot `owned T[]` de un elemento; el valor removido
    //     es del llamador y cada valor restante se destruye una vez en Clear().
    //     Desde P-1, RemoveFirst declara `throws InvalidOperationException`, asi que la llamada va
    //     en un try/catch minimo (autorizado, P9); el valor removido termina con el bloque try, asi
    //     `before` vale 1 y el resultado es 1123 (era 123 antes de tener que manejar la llamada).
    expectRun(tests, root, "C12LinkedList", prelude + R"ns(
class Program
{
    public static int Main()
    {
        int before = 0;
        {
            raii LinkedList<Guard> list = new LinkedList<Guard>();
            list.AddLast(Factory.Make(1));
            list.AddLast(Factory.Make(2));
            list.AddLast(Factory.Make(3));
            try { Guard first = list.RemoveFirst(); } catch (InvalidOperationException e) { return 99; }
            before = Counter.Drops;
        }
        return before * 1000 + Counter.Drops;
    }
}
)ns", 1123, true);
}

/// EN: P1 (DEC-12). A member of a generic class or struct may constrain its type's parameter
///     (`public T Copy() where T : copy`). The constraint is checked when a call resolves against
///     a concrete argument: a copyable argument is accepted, a move-only one is rejected at the
///     call, and a specialization that never calls the member still compiles. The constraint
///     survives NSMETA, so a consumer of the precompiled library gets the same verdict. A member
///     clause naming a parameter its type does not declare stays an error.
/// ES: P1 (DEC-12). Un miembro de una clase o struct generica puede restringir el parametro de su
///     tipo (`public T Copy() where T : copy`). La restriccion se comprueba al resolver una llamada
///     con un argumento concreto: un argumento copiable se acepta, uno move-only se rechaza en la
///     llamada, y una especializacion que nunca llama al miembro compila. La restriccion
///     sobrevive en NSMETA, asi que el consumidor de la biblioteca precompilada recibe el mismo
///     veredicto. Una clausula de miembro que nombra un parametro que su tipo no declara sigue
///     siendo error.
constexpr std::string_view kCopyBoxPrelude = R"ns(
class Box<T>
{
    private T only;
    public void Put(T value) { only = move(value); }
    public T Copy() where T : copy { return only; }
    public int Count() { return 1; }
}
)ns";

/// EN: P1 through NSMETA: the library is compiled first and the consumers only see its metadata.
/// ES: P1 a traves de NSMETA: la biblioteca se compila primero y los consumidores solo ven su
///     metadata.
void testMemberCopyConstraintLibrary(TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path producer = root / "P1Library" / "Api";
    writeFile(producer / "CopyApi.nsproj",
        "<Project><PropertyGroup><Name>CopyApi</Name><Language>3</Language>"
        "<OutputType>StaticLibrary</OutputType></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
    writeFile(producer / "Main.ns", "namespace CopyApi\n{\npublic " +
        std::string(kCopyBoxPrelude).substr(1) + "}\n");
    const std::string projectText = (producer / "CopyApi.nsproj").string();
    const std::string libraryText = (producer / "CopyApi.lib").string();
    const std::array<std::string_view, 5> arguments = {
        "--emit-project-oop-lib", projectText, "-O0", "-o", libraryText};
    std::string diagnostic;
    const ExitCode built = runDriver(arguments, diagnostic);
    tests.expect(built == ExitCode::Success, "P1Library: the library compiles: " + diagnostic);
    if (built != ExitCode::Success) return;
    const std::filesystem::path core =
        std::filesystem::path(NSC_SOURCE_DIR) / "lib/NativeSharp.Core/NativeSharp.Core.nsproj";
    const std::string reference = "<ProjectReference Include=\"" + core.generic_string() + "\" />" +
        "<LibraryReference Include=\"" + std::filesystem::path(libraryText).generic_string() + "\" />";
    const std::array<std::pair<std::string_view, std::string_view>, 2> consumers = {{
        {"Accepted", "raii Box<int> b = new Box<int>(); b.Put(12); return b.Copy() + b.Count();"},
        {"Rejected", "raii Box<Guard> b = new Box<Guard>(); Guard g; g.Id = 1; b.Put(move(g)); "
            "Guard h = b.Copy(); return h.Id;"}}};
    for (const auto& [name, body] : consumers)
    {
        const std::filesystem::path directory = root / "P1Library" / std::string(name);
        const std::filesystem::path project = directory / "Consumer.nsproj";
        writeFile(project,
            "<Project><PropertyGroup><Name>Consumer</Name></PropertyGroup><ItemGroup>"
            "<Compile Include=\"**/*.ns\" />" + reference + "</ItemGroup></Project>");
        writeFile(directory / "Main.ns", "using CopyApi;" + std::string(kGuardPrelude) +
            "class Program { public static int Main() { " + std::string(body) + " } }\n");
        const std::filesystem::path executable = directory / "Consumer.exe";
        const ExitCode code = compile(project, executable, "-O0", diagnostic);
        if (name == "Rejected")
        {
            tests.expect(code != ExitCode::Success && diagnostic.find(
                "of 'Box.Copy' requires copyable storage ('where T : copy')") != std::string::npos,
                "P1Library: a move-only argument is rejected through NSMETA: " + diagnostic);
            continue;
        }
        tests.expect(code == ExitCode::Success, "P1Library: the consumer compiles: " + diagnostic);
        if (code != ExitCode::Success) continue;
        const int observed = std::system(("\"\"" + executable.string() + "\"\"").c_str());
        tests.expect(observed == 13, "P1Library: exits 13, observed " + std::to_string(observed));
    }
}

void testMemberCopyConstraint(TestContext& tests, const std::filesystem::path& root)
{
    const std::string prelude = std::string(kGuardPrelude) + std::string(kCopyBoxPrelude);
    expectRun(tests, root, "P1MemberCopyAccepted", prelude + R"ns(
struct Cell<T> { public T Item; public T Copy() where T : copy { return Item; } }
class Program
{
    public static int Main()
    {
        raii Box<int> numbers = new Box<int>();
        numbers.Put(4);
        Cell<int> cell;
        cell.Item = 30;
        return numbers.Copy() + cell.Copy();
    }
}
)ns", 34);
    expectRun(tests, root, "P1MemberUnused", prelude + R"ns(
class Program
{
    public static int Main()
    {
        raii Box<Guard> guards = new Box<Guard>();
        Guard g;
        g.Id = 4;
        guards.Put(move(g));
        return guards.Count();
    }
}
)ns", 1);
    expectReject(tests, root, "P1MemberCopyRejected", prelude + R"ns(
class Program
{
    public static int Main()
    {
        raii Box<Guard> guards = new Box<Guard>();
        Guard g;
        g.Id = 1;
        guards.Put(move(g));
        Guard h = guards.Copy();
        return h.Id;
    }
}
)ns", "generic parameter 'T' of 'Box.Copy' requires copyable storage ('where T : copy')");
    expectReject(tests, root, "P1MemberUnknownParameter", R"ns(
class Box<T> { public int Value() where U : copy { return 1; } }
class Program { public static int Main() { return 0; } }
)ns", "'U' is not a generic or lifetime parameter of this declaration");
    expectReject(tests, root, "P1MemberNonGeneric", R"ns(
class Plain { public int Value() where T : copy { return 1; } }
class Program { public static int Main() { return 0; } }
)ns", "'T' is not a generic or lifetime parameter of this declaration");
    testMemberCopyConstraintLibrary(tests, root);
}

/// EN: P1 on constructors. A constructor of a generic type may constrain the type's parameter; a
///     construction that selects it with a failing argument is rejected at the `new`, and an
///     overload whose clause holds is still chosen by its parameter types.
/// ES: P1 en constructores. Un constructor de un tipo generico puede restringir el parametro del
///     tipo; una construccion que lo elige con un argumento que no cumple se rechaza en el `new`, y
///     una sobrecarga cuya clausula se cumple se sigue eligiendo por sus tipos de parametro.
constexpr std::string_view kCopyHolderPrelude = R"ns(
delegate int Cmp<T>(T a, T b);
delegate int InCmp<T>(in T a, in T b);
class Holder<T>
{
    private int kind;
    public Holder(Cmp<T> c) where T : copy { kind = 1; }
    public Holder(InCmp<T> c) { kind = 2; }
    public Holder() { kind = 3; }
    public int Kind() { return kind; }
}
struct Cell<T>
{
    public int Kind;
    public Cell(int k) where T : copy { Kind = k; }
}
)ns";

void testConstructorCopyConstraint(TestContext& tests, const std::filesystem::path& root)
{
    const std::string prelude = std::string(kGuardPrelude) + std::string(kCopyHolderPrelude);
    expectRun(tests, root, "P1ConstructorAccepted", prelude + R"ns(
class Program
{
    public static int Main()
    {
        Cmp<int> c = (int p, int q) => p - q;
        InCmp<Guard> d = (in Guard p, in Guard q) => p.Id - q.Id;
        raii Holder<int> a = new Holder<int>(c);
        raii Holder<Guard> b = new Holder<Guard>(d);
        raii Holder<Guard> e = new Holder<Guard>();
        Cell<int> cell = new Cell<int>(4);
        return a.Kind() * 1000 + b.Kind() * 100 + e.Kind() * 10 + cell.Kind;
    }
}
)ns", 1234);
    expectReject(tests, root, "P1ConstructorRejected", prelude + R"ns(
class Program
{
    public static int Main()
    {
        Cmp<Guard> c = (Guard p, Guard q) => p.Id - q.Id;
        raii Holder<Guard> a = new Holder<Guard>(c);
        return a.Kind();
    }
}
)ns", "generic parameter 'T' of 'Holder constructor' requires copyable storage ('where T : copy')");
    expectReject(tests, root, "P1StructConstructorRejected", prelude + R"ns(
class Program { public static int Main() { Cell<Guard> c = new Cell<Guard>(5); return c.Kind; } }
)ns", "generic parameter 'T' of 'Cell constructor' requires copyable storage ('where T : copy')");
    expectReject(tests, root, "P1ConstructorNonGeneric", R"ns(
class Plain { public Plain() where T : copy { } }
class Program { public static int Main() { return 0; } }
)ns", "'T' is not a generic or lifetime parameter of this declaration");
}

/// EN: P2 (DEC-12). Every collection API that hands an element to a by-value delegate
///     (Comparison, Predicate, Action, HashFunc, KeyValueAction) requires `T : copy`; the
///     InComparison/InPredicate/InAction overloads lend the element instead and serve a move-only
///     T. Sort, Reverse and RemoveIf relocate elements by swapping, so a List<T> or a span of a
///     struct with a destructor is sorted, reversed and filtered without a single extra
///     destruction (the destructor counts calls, so a destroyed empty value would show).
/// ES: P2 (DEC-12). Toda API de coleccion que pasa un elemento a un delegado por valor
///     (Comparison, Predicate, Action, HashFunc, KeyValueAction) exige `T : copy`; las sobrecargas
///     con InComparison/InPredicate/InAction prestan el elemento y sirven para un T move-only. Sort,
///     Reverse y RemoveIf reubican elementos intercambiandolos, asi que una List<T> o un span de un
///     struct con destructor se ordena, invierte y filtra sin una sola destruccion de mas (el
///     destructor cuenta llamadas, asi que destruir un valor vacio se notaria).
void testInDelegateOverloads(TestContext& tests, const std::filesystem::path& root)
{
    expectRun(tests, root, "P2SwapWithoutExtraDrops", R"ns(
using NativeSharp.Core;
using NativeSharp.Core.Collections.Generic;
class Counter { public static int Drops; public static int Sum; }
struct Guard
{
    public int Id;
    ~Guard() { Counter.Drops = Counter.Drops + 1; Counter.Sum = Counter.Sum + Id; }
    public static bool operator <(in Guard a, in Guard b) { return a.Id < b.Id; }
    public static bool operator >(in Guard a, in Guard b) { return a.Id > b.Id; }
}
class Make { public static Guard G(int id) { Guard g; g.Id = id; return move(g); } }
class Program
{
    private static int RunList()
    {
        raii List<Guard> list = new List<Guard>();
        list.Add(Make.G(3));
        list.Add(Make.G(1));
        list.Add(Make.G(2));
        list.Sort((in Guard a, in Guard b) => b.Id - a.Id);
        list.Sort();
        list.Reverse();
        int kept = list.RemoveAll((in Guard g) => g.Id == 2);
        return Counter.Drops * 10 + kept;
    }
    private static int RunArray()
    {
        raii Guard[] a = new Guard[4];
        a[0].Id = 3; a[1].Id = 1; a[2].Id = 4; a[3].Id = 2;
        Span<Guard> s = Spans.OfRange<Guard>(a, 0, 4);
        Algorithm.Sort<Guard>(s);
        Algorithm.Sort<Guard>(s, (in Guard x, in Guard y) => y.Id - x.Id);
        Algorithm.StableSort<Guard>(s, (in Guard x, in Guard y) => x.Id - y.Id);
        Algorithm.Reverse<Guard>(s);
        int kept = Algorithm.RemoveIf<Guard>(s, (in Guard x) => x.Id % 2 == 0);
        return Counter.Drops * 10 + kept;
    }
    public static int Main()
    {
        int list = RunList();
        int afterList = Counter.Drops;
        int sumList = Counter.Sum;
        Counter.Drops = 0;
        Counter.Sum = 0;
        int array = RunArray();
        return list * 1000000 + afterList * 100000 + sumList * 1000 + array * 10 + Counter.Drops;
    }
}
)ns", 11306024, true);
    expectRun(tests, root, "P2ListInQueries", R"ns(
using NativeSharp.Core;
using NativeSharp.Core.Collections.Generic;
class Counter { public static int Drops; }
struct Guard
{
    public int Id;
    ~Guard() { Counter.Drops = Counter.Drops * 10 + Id; }
    public static bool operator <(in Guard a, in Guard b) { return a.Id < b.Id; }
    public static bool operator >(in Guard a, in Guard b) { return a.Id > b.Id; }
}
class Make { public static Guard G(int id) { Guard g; g.Id = id; return move(g); } }
class Program
{
    public static int Main()
    {
        int r = 0;
        int afterRemove = 0;
        {
            raii List<Guard> list = new List<Guard>();
            list.Add(Make.G(1));
            list.Add(Make.G(2));
            list.Add(Make.G(3));
            list.Add(Make.G(4));
            InPredicate<Guard> even = (in Guard g) => g.Id % 2 == 0;
            r = list.FindIndex(even) * 1000 + list.FindLastIndex(even) * 100;
            if (list.Exists(even)) { r = r + 10; }
            if (!list.TrueForAll(even)) { r = r + 1; }
            r = r * 10 + list.RemoveAll(even);
            afterRemove = Counter.Drops;
        }
        return r * 10000 + afterRemove * 100 + Counter.Drops;
    }
}
)ns", 131124813, true);
    expectRun(tests, root, "P2PriorityQueueInComparison", R"ns(
using NativeSharp.Core;
using NativeSharp.Core.Collections.Generic;
class Counter { public static int Drops; }
struct Guard
{
    public int Id;
    ~Guard() { Counter.Drops = Counter.Drops * 10 + Id; }
    public static bool operator <(in Guard a, in Guard b) { return a.Id < b.Id; }
    public static bool operator >(in Guard a, in Guard b) { return a.Id > b.Id; }
}
class Make { public static Guard G(int id) { Guard g; g.Id = id; return move(g); } }
class Program
{
    public static int Main()
    {
        int r = 0;
        {
            InComparison<Guard> byId = (in Guard a, in Guard b) => a.Id - b.Id;
            raii PriorityQueue<int, Guard> queue = new PriorityQueue<int, Guard>(byId);
            queue.Enqueue(30, Make.G(3));
            queue.Enqueue(10, Make.G(1));
            queue.Enqueue(20, Make.G(2));
            try
            {
                r = queue.Dequeue();
                r = r * 100 + queue.Dequeue();
            }
            catch (InvalidOperationException e) { return 99; }
        }
        return r * 1000 + Counter.Drops;
    }
}
)ns", 1020123, true);
    expectRun(tests, root, "P2SetsAndDictionaryIn", R"ns(
using NativeSharp.Core;
using NativeSharp.Core.Collections.Generic;
class Program
{
    public static int Main()
    {
        raii HashSet<int> hs = new HashSet<int>();
        hs.Add(1); hs.Add(2); hs.Add(3); hs.Add(4);
        int r = hs.RemoveWhere((in int x) => x % 2 == 0);
        int sum = 0;
        hs.ForEach((in int x) => { sum = sum + x; });
        InComparison<int> desc = (in int a, in int b) => b - a;
        raii SortedSet<int> ss = new SortedSet<int>(desc);
        ss.Add(1); ss.Add(3); ss.Add(2);
        int order = 0;
        ss.ForEach((in int x) => { order = order * 10 + x; });
        int removed = ss.RemoveWhere((in int x) => x == 3);
        Comparison<int> asc = (int a, int b) => a - b;
        raii SortedDictionary<int, int> sd = new SortedDictionary<int, int>(asc);
        try { sd.Add(2, 20); sd.Add(1, 10); } catch (ArgumentException e) { return 98; }
        raii Dictionary<int, int> d = new Dictionary<int, int>();
        try { d.Add(1, 1); d.Add(2, 2); d.Add(3, 3); } catch (ArgumentException e) { return 97; }
        int dr = d.RemoveKeysWhere((in int k) => k != 2);
        return r * 1000000 + sum * 100000 + order * 100 + removed * 10 + dr * 3 + d.Count() + sd.Count() * 0;
    }
}
)ns", 2432117, true);
    expectReject(tests, root, "P2ByValueComparisonRejected", R"ns(
using NativeSharp.Core;
using NativeSharp.Core.Collections.Generic;
class Counter { public static int Drops; }
struct Guard
{
    public int Id;
    ~Guard() { Counter.Drops = Counter.Drops * 10 + Id; }
    public static bool operator <(in Guard a, in Guard b) { return a.Id < b.Id; }
    public static bool operator >(in Guard a, in Guard b) { return a.Id > b.Id; }
}
class Make { public static Guard G(int id) { Guard g; g.Id = id; return move(g); } }
class Program
{
    public static int Main()
    {
        raii List<Guard> list = new List<Guard>();
        list.Add(Make.G(2));
        list.Sort((Guard a, Guard b) => a.Id - b.Id);
        return 0;
    }
}
)ns", "generic parameter 'T' of 'List.Sort' requires copyable storage ('where T : copy')", true);
    expectReject(tests, root, "P2ByValueConstructorRejected", R"ns(
using NativeSharp.Core;
using NativeSharp.Core.Collections.Generic;
class Counter { public static int Drops; }
struct Guard
{
    public int Id;
    ~Guard() { Counter.Drops = Counter.Drops * 10 + Id; }
    public static bool operator <(in Guard a, in Guard b) { return a.Id < b.Id; }
    public static bool operator >(in Guard a, in Guard b) { return a.Id > b.Id; }
}
class Make { public static Guard G(int id) { Guard g; g.Id = id; return move(g); } }
class Program
{
    public static int Main()
    {
        Comparison<Guard> byId = (Guard a, Guard b) => a.Id - b.Id;
        raii PriorityQueue<int, Guard> queue = new PriorityQueue<int, Guard>(byId);
        return queue.Count();
    }
}
)ns",
        "generic parameter 'TPriority' of 'PriorityQueue constructor' requires copyable storage", true);
}

/// EN: P3 (DEC-12). `ref readonly T` on a method, property or indexer returns an alias that may be
///     read, passed to readonly members and bound by `foreach (ref ...)`, but not written, moved,
///     bound to a mutable `ref` local, used by a mutating struct member or iterated with `ref mut`.
///     An implementation or override must keep the mode.
/// ES: P3 (DEC-12). `ref readonly T` en un metodo, propiedad o indexador devuelve un alias que se
///     puede leer, pasar a miembros readonly y enlazar con `foreach (ref ...)`, pero no escribir,
///     mover, enlazar a un local `ref` mutable, usar con un miembro mutante de struct ni recorrer con
///     `ref mut`. Una implementacion o un override debe conservar el modo.
void testReadonlyReferenceReturns(TestContext& tests, const std::filesystem::path& root)
{
    expectRun(tests, root, "P3RefReadonlyReads", R"ns(
class Cell
{
    private int v;
    public Cell() { v = 7; }
    public ref readonly int Get() { return ref v; }
    public ref int GetMut() { return ref v; }
    public ref readonly int this[int i] { get { return ref v; } }
    public ref readonly int Value { get { return ref v; } }
}
class Program
{
    public static int Main()
    {
        raii Cell c = new Cell();
        int a = c.Get();
        c.GetMut() = 9;
        return a * 1000 + c.Get() * 100 + c[0] * 10 + c.Value - 9;
    }
}
)ns", 7990);
    expectRun(tests, root, "P3RefReadonlyInterface", R"ns(
interface IGet { ref readonly int Get(); }
class Cell : IGet
{
    private int v;
    public Cell() { v = 3; }
    public ref readonly int Get() { return ref v; }
}
class Program
{
    public static int Main()
    {
        raii Cell c = new Cell();
        borrowed IGet g = c;
        return g.Get();
    }
}
)ns", 3);
    expectRun(tests, root, "P3RefReadonlyForeach", R"ns(
class Bag
{
    public owned BagEnumerator GetEnumerator() { return new BagEnumerator(); }
}
class BagEnumerator
{
    private int current;
    public bool MoveNext() { current = current + 1; return current <= 3; }
    public ref readonly int Current { get { return ref current; } }
}
class Program
{
    public static int Main()
    {
        raii Bag b = new Bag();
        int sum = 0;
        foreach (ref int x in b) { sum = sum * 10 + x; }
        foreach (int y in b) { sum = sum * 10 + y; }
        return sum;
    }
}
)ns", 123123);
    expectReject(tests, root, "P3RefReadonlyAssign", R"ns(
class Cell
{
    private int v;
    public ref readonly int Get() { return ref v; }
}
class Program
{
    public static int Main()
    {
        raii Cell c = new Cell();
        c.Get() = 5;
        return 0;
    }
}
)ns", "left side of assignment is not assignable");
    expectReject(tests, root, "P3RefReadonlyRefLocal", R"ns(
class Cell
{
    private int v;
    public ref readonly int Get() { return ref v; }
}
class Program
{
    public static int Main()
    {
        raii Cell c = new Cell();
        ref int x = ref c.Get();
        return x;
    }
}
)ns", "reference-local initializer must be a mutable addressable storage location");
    expectReject(tests, root, "P3RefReadonlyMutatingCall", R"ns(
struct S
{
    public int A;
    public void Bump() { A = A + 1; }
}
class Holder
{
    private S s;
    public ref readonly S Get() { return ref s; }
}
class Program
{
    public static int Main()
    {
        raii Holder h = new Holder();
        h.Get().Bump();
        return 0;
    }
}
)ns", "readonly receiver cannot invoke mutating struct member 'Bump'");
    expectReject(tests, root, "P3RefReadonlyFieldWrite", R"ns(
struct S { public int A; }
class Holder
{
    private S s;
    public ref readonly S Get() { return ref s; }
}
class Program
{
    public static int Main()
    {
        raii Holder h = new Holder();
        h.Get().A = 3;
        return 0;
    }
}
)ns", "left side of assignment is not assignable");
    expectReject(tests, root, "P3RefReadonlyPropertyWrite", R"ns(
class Table
{
    private int v;
    public ref readonly int First { get { return ref v; } }
}
class Program
{
    public static int Main()
    {
        raii Table t = new Table();
        t.First = 1;
        return 0;
    }
}
)ns", "left side of assignment is not assignable");
    expectReject(tests, root, "P3RefReadonlyInterfaceMode", R"ns(
interface IGet { ref readonly int Get(); }
class Cell : IGet
{
    private int v;
    public ref int Get() { return ref v; }
}
class Program { public static int Main() { return 0; } }
)ns", "does not implement interface method 'Get()'");
    expectReject(tests, root, "P3RefReadonlyOnField", R"ns(
struct S { public ref readonly int F; }
class Program { public static int Main() { return 0; } }
)ns", "'ref readonly' is valid only on the return of a method, property or indexer");
    expectReject(tests, root, "P3RefReadonlyForeachRefMut", R"ns(
class Bag
{
    public owned BagEnumerator GetEnumerator() { return new BagEnumerator(); }
}
class BagEnumerator
{
    private int current;
    public bool MoveNext() { current = current + 1; return current <= 3; }
    public ref readonly int Current { get { return ref current; } }
}
class Program
{
    public static int Main()
    {
        raii Bag b = new Bag();
        int sum = 0;
        foreach (ref mut int x in b) { x = 5; }
        return sum;
    }
}
)ns", "reference-local initializer must be a mutable addressable storage location");
}

/// EN: P3. An `in` parameter accepts a readonly place: another `in` parameter, a `ref readonly`
///     result.
/// ES: P3. Un parametro `in` acepta un lugar readonly: otro parametro `in`, un resultado
///     `ref readonly`.
void testInAcceptsReadonlyPlace(TestContext& tests, const std::filesystem::path& root)
{
    expectRun(tests, root, "P3InForward", R"ns(
class Program
{
    private static int B(in int x) { return x + 1; }
    private static int A(in int y) { return B(in y); }
    public static int Main()
    {
        int v = 4;
        return A(in v);
    }
}
)ns", 5);
    expectRun(tests, root, "P3InFromRefReadonly", R"ns(
class Cell
{
    private int v;
    public Cell() { v = 7; }
    public ref readonly int Get() { return ref v; }
    public ref int GetMut() { return ref v; }
}
class Program
{
    private static int Use(in int x) { return x * 2; }
    public static int Main()
    {
        raii Cell c = new Cell();
        int a = c.Get();
        c.GetMut() = 9;
        return a * 1000 + c.Get() * 10 + Use(in c.Get()) % 10;
    }
}
)ns", 7098);
}

/// EN: P3. `return ref field[i]` of an array or pointer field returns with the receiver lifetime.
/// ES: P3. `return ref campo[i]` de un arreglo o puntero campo se devuelve con la vida del receptor.
void testRefToFieldElement(TestContext& tests, const std::filesystem::path& root)
{
    expectRun(tests, root, "P3RefFieldElement", R"ns(
class Table
{
    private owned int[] data;
    public Table() { data = new int[] { 4, 5, 6 }; }
    public ref readonly int this[int i] { get { return ref data[i]; } }
    public ref readonly int First { get { return ref data[0]; } }
}
class Program
{
    public static int Main()
    {
        raii Table t = new Table();
        return t[2] * 100 + t[1] * 10 + t.First;
    }
}
)ns", 654);
    expectRun(tests, root, "P3RefFieldElementCurrent", R"ns(
class Bag
{
    private owned int[] data;
    public Bag() { data = new int[] { 1, 2, 3 }; }
    public int Count() { return data.Length; }
    public ref readonly int At(int i) { return ref data[i]; }
    public owned BagEnumerator GetEnumerator() { return new BagEnumerator(this); }
}
class BagEnumerator
{
    private owned int[] data;
    private int index;
    public BagEnumerator(Bag b) { data = new int[] { b.At(0), b.At(1), b.At(2) }; index = -1; }
    public bool MoveNext() { index = index + 1; return index < data.Length; }
    public ref readonly int Current { get { return ref data[index]; } }
}
class Program
{
    public static int Main()
    {
        raii Bag b = new Bag();
        int sum = 0;
        foreach (ref int x in b) { sum = sum * 10 + x; }
        return sum;
    }
}
)ns", 123);
}

/// EN: P3. A struct returned by `ref` and used as a receiver or field target reaches the storage.
/// ES: P3. Un struct devuelto por `ref` y usado como receptor o destino de campo llega al
///     almacenamiento.
void testRefReturnReceiver(TestContext& tests, const std::filesystem::path& root)
{
    expectRun(tests, root, "P3RefReceiverCall", R"ns(
struct S
{
    public int A;
    public void Bump() { A = A + 1; }
}
class Holder
{
    private S s;
    private int v;
    public ref S GetMut() { return ref s; }
    public ref int V() { return ref v; }
}
class Program { public static int Main() { raii Holder h = new Holder(); h.GetMut().Bump(); return h.GetMut().A; } }
)ns", 1);
    expectRun(tests, root, "P3RefReceiverField", R"ns(
struct S
{
    public int A;
    public void Bump() { A = A + 1; }
}
class Holder
{
    private S s;
    private int v;
    public ref S GetMut() { return ref s; }
    public ref int V() { return ref v; }
}
class Program { public static int Main() { raii Holder h = new Holder(); h.GetMut().A = 5; return h.GetMut().A; } }
)ns", 5);
    expectRun(tests, root, "P3RefReceiverMixed", R"ns(
struct S
{
    public int A;
    public void Bump() { A = A + 1; }
    public readonly int Read() { return A; }
}
class Holder
{
    private S s;
    public ref readonly S Get() { return ref s; }
    public ref S GetMut() { return ref s; }
}
class Program
{
    public static int Main()
    {
        raii Holder h = new Holder();
        h.GetMut().Bump();
        h.GetMut().Bump();
        return h.Get().Read() * 10 + h.Get().A;
    }
}
)ns", 22);
}

/// EN: P3. A precompiled library with public `ref` and `ref readonly` members is consumable; the
///     readonly mode survives NSMETA.
/// ES: P3. Una biblioteca precompilada con miembros publicos `ref` y `ref readonly` se puede
///     consumir; el modo readonly sobrevive en NSMETA.
void testRefReturnLibrary(TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path producer = root / "P3RefLibrary" / "Api";
    writeFile(producer / "RefApi.nsproj",
        "<Project><PropertyGroup><Name>RefApi</Name><Language>3</Language>"
        "<OutputType>StaticLibrary</OutputType></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
    writeFile(producer / "Main.ns", R"ns(
namespace RApi
{
    public class Cell
    {
        private int v;
        public Cell() { v = 6; }
        public ref readonly int Get() { return ref v; }
        public ref readonly int this[int i] { get { return ref v; } }
    }
}
)ns");
    const std::string projectText = (producer / "RefApi.nsproj").string();
    const std::string libraryText = (producer / "RefApi.lib").string();
    const std::array<std::string_view, 5> arguments = {
        "--emit-project-oop-lib", projectText, "-O0", "-o", libraryText};
    std::string diagnostic;
    const ExitCode built = runDriver(arguments, diagnostic);
    tests.expect(built == ExitCode::Success, "P3RefLibrary: the library compiles: " + diagnostic);
    if (built != ExitCode::Success) return;
    const std::string reference = "<LibraryReference Include=\"" +
        std::filesystem::path(libraryText).generic_string() + "\" />";
    const std::array<std::pair<std::string_view, std::string_view>, 2> consumers = {{
        {"Use", R"ns(
using RApi;
class Program
{
    public static int Main()
    {
        raii Cell c = new Cell();
        return c.Get() * 10 + c[0];
    }
}
)ns"},
        {"Bad", R"ns(
using RApi;
class Program
{
    public static int Main()
    {
        raii Cell c = new Cell();
        c.Get() = 1;
        c[0] = 2;
        return 0;
    }
}
)ns"}}};
    for (const auto& [name, source] : consumers)
    {
        const std::filesystem::path directory = root / "P3RefLibrary" / std::string(name);
        const std::filesystem::path project = directory / "Consumer.nsproj";
        writeFile(project,
            "<Project><PropertyGroup><Name>Consumer</Name><Language>3</Language></PropertyGroup>"
            "<ItemGroup><Compile Include=\"**/*.ns\" />" + reference + "</ItemGroup></Project>");
        writeFile(directory / "Main.ns", source);
        const std::filesystem::path executable = directory / "Consumer.exe";
        const ExitCode code = compile(project, executable, "-O0", diagnostic);
        if (name == "Bad")
        {
            tests.expect(code != ExitCode::Success &&
                    diagnostic.find("left side of assignment is not assignable") != std::string::npos,
                "P3RefLibrary: writes through precompiled ref readonly are rejected: " + diagnostic);
            continue;
        }
        tests.expect(code == ExitCode::Success, "P3RefLibrary: the consumer compiles: " + diagnostic);
        if (code != ExitCode::Success) continue;
        const int observed = std::system(("\"\"" + executable.string() + "\"\"").c_str());
        tests.expect(observed == 66, "P3RefLibrary: exits 66, observed " + std::to_string(observed));
    }
}

/// EN: P3. A generic `copy` struct is Copy only when its type arguments are: `Pair<int, int>` copies,
///     `Pair<Guard, int>` is move-only (and a copy of it is rejected), and a SortedList keyed by a
///     move-only struct can be instantiated. A non-generic `copy` struct with a non-Copy field is
///     still an error.
/// ES: P3. Un struct `copy` generico es Copy solo si sus argumentos de tipo lo son: `Pair<int, int>`
///     se copia, `Pair<Guard, int>` es move-only (y copiarlo se rechaza), y una SortedList con clave
///     struct move-only se puede instanciar. Un struct `copy` no generico con un campo no Copy sigue
///     siendo error.
void testConditionalGenericCopy(TestContext& tests, const std::filesystem::path& root)
{
    expectRun(tests, root, "P3GenericCopyStays", R"ns(
copy struct Pair<A, B> { public A First; public B Second; }
class Program
{
    public static int Main()
    {
        Pair<int, int> a;
        a.First = 3;
        a.Second = 4;
        Pair<int, int> b = a;
        b.First = 5;
        return a.First * 10 + b.First;
    }
}
)ns", 35);
    expectRun(tests, root, "P3GenericCopyMoveOnly", R"ns(
class Counter { public static int Drops; }
struct Guard
{
    public int Id;
    ~Guard() { Counter.Drops = Counter.Drops * 10 + Id; }
}
class Make { public static Guard G(int id) { Guard g; g.Id = id; return move(g); } }
copy struct Pair<A, B> { public A First; public B Second; }
class Program
{
    private static int Run()
    {
        Pair<Guard, int> a;
        a.First = Make.G(7);
        a.Second = 2;
        Pair<Guard, int> b = move(a);
        return b.First.Id * 10 + b.Second;
    }
    public static int Main()
    {
        int r = Run();
        return r * 100 + Counter.Drops;
    }
}
)ns", 7207);
    expectRun(tests, root, "P3SortedListMoveOnlyKey", R"ns(
using NativeSharp.Core;
using NativeSharp.Core.Collections.Generic;
class Counter { public static int Drops; }
struct Guard
{
    public int Id;
    ~Guard() { Counter.Drops = Counter.Drops * 10 + Id; }
    public static bool operator <(in Guard a, in Guard b) { return a.Id < b.Id; }
    public static bool operator >(in Guard a, in Guard b) { return a.Id > b.Id; }
}
class Make { public static Guard G(int id) { Guard g; g.Id = id; return move(g); } }
class Program
{
    public static int Main()
    {
        int r = 0;
        {
            InComparison<Guard> byId = (in Guard a, in Guard b) => a.Id - b.Id;
            raii SortedList<Guard, int> list = new SortedList<Guard, int>(byId);
            try
            {
                list.Add(Make.G(3), 30);
                list.Add(Make.G(1), 10);
                list.Add(Make.G(2), 20);
            }
            catch (ArgumentException e) { return 99; }
            int order = 0;
            list.ForEachKey((in Guard g) => { order = order * 10 + g.Id; });
            r = order * 10 + list.RemoveKeysWhere((in Guard g) => g.Id == 2);
            r = r * 10 + list.Count();
            r = r * 100 + Counter.Drops;
        }
        return r * 100 + Counter.Drops;
    }
}
)ns", 123120413, true);
    expectReject(tests, root, "P3GenericCopyRejected", R"ns(
class Counter { public static int Drops; }
struct Guard
{
    public int Id;
    ~Guard() { Counter.Drops = Counter.Drops * 10 + Id; }
}
class Make { public static Guard G(int id) { Guard g; g.Id = id; return move(g); } }
copy struct Pair<A, B> { public A First; public B Second; }
class Program
{
    public static int Main()
    {
        Pair<Guard, int> a;
        a.Second = 2;
        Pair<Guard, int> b = a;
        return b.Second;
    }
}
)ns", "cannot be copied into local 'b'");
    expectReject(tests, root, "P3NonGenericCopyField", R"ns(
class Counter { public static int Drops; }
struct Guard
{
    public int Id;
    ~Guard() { Counter.Drops = Counter.Drops * 10 + Id; }
}
class Make { public static Guard G(int id) { Guard g; g.Id = id; return move(g); } }
copy struct Bad { public Guard G; }
class Program { public static int Main() { return 0; } }
)ns", "contains non-Copy field 'G'");
}

/// EN: Freeze 3.0 DEC-12. Returning a field, an element or another place of a move-only type by value
///     copies it: the caller drops the copy and the owner drops the original. It is rejected with
///     NS3020 like any other copy; `return move(...)` and a reference return stay valid.
/// ES: Congelamiento 3.0 DEC-12. Devolver por valor un campo, un elemento u otro lugar de tipo
///     move-only lo copia: el llamador destruye la copia y el dueno destruye el original. Se
///     rechaza con NS3020 como cualquier otra copia; `return move(...)` y el retorno por referencia
///     siguen siendo validos.
void testMoveOnlyPlaceReturn(TestContext& tests, const std::filesystem::path& root)
{
    const std::string guard = R"ns(
class Counter { public static int Drops; }
struct Guard
{
    public int Id;
    ~Guard() { Counter.Drops = Counter.Drops * 10 + Id; }
}
)ns";
    expectReject(tests, root, "P3FieldReturnCopy", guard + R"ns(
class Holder
{
    public Guard Field;
    public Guard Get() { return Field; }
}
class Program { public static int Main() { raii Holder h = new Holder(); return h.Get().Id; } }
)ns", "cannot be copied into the return value");
    expectReject(tests, root, "P3ElementReturnCopy", guard + R"ns(
class Holder<T>
{
    private owned T[] items;
    public Holder() { items = new T[1]; }
    public T Get(int i) { return items[i]; }
}
class Program
{
    public static int Main() { raii Holder<Guard> h = new Holder<Guard>(); return h.Get(0).Id; }
}
)ns", "cannot be copied into the return value");
    expectRun(tests, root, "P3PlaceReturnByRef", guard + R"ns(
class Holder
{
    private owned Guard[] items;
    public Holder() { items = new Guard[1]; Guard g; g.Id = 7; items[0] = move(g); }
    public ref Guard Get(int i) { return ref items[i]; }
}
class Program
{
    private static int Run() { raii Holder h = new Holder(); return h.Get(0).Id; }
    public static int Main() { int r = Run(); return r * 10 + Counter.Drops; }
}
)ns", 77);
}

/// EN: Freeze 3.0 P3. An indexer or property accessor of a generic type may restrict the type's
///     parameter (`get where T : copy`). The getter fails at the access with the constraint message; a plain
///     `=` runs only the setter and stays valid for a move-only T.
/// ES: Congelamiento 3.0 P3. Un accesor de indexador o de propiedad de un tipo generico puede
///     restringir el parametro del tipo (`get where T : copy`). El getter falla en el acceso con el mensaje del
///     constraint; un `=` simple solo ejecuta el setter y sigue valido para un T move-only.
void testIndexerAccessorWhere(TestContext& tests, const std::filesystem::path& root)
{
    const std::string box = R"ns(
class Counter { public static int Drops; }
struct Guard
{
    public int Id;
    ~Guard() { Counter.Drops = Counter.Drops * 10 + Id; }
}
class Make { public static Guard G(int id) { Guard g; g.Id = id; return move(g); } }
class Box<T>
{
    private owned T[] items;
    public Box() { items = new T[2]; }
    public owned T this[int i]
    {
        get where T : copy { return items[i]; }
        set { items[i] = move(value); }
    }
    public ref T RefAt(int i) { return ref items[i]; }
}
)ns";
    expectRun(tests, root, "P3IndexerWhereCopy", box + R"ns(
class Program
{
    public static int Main()
    {
        raii Box<int> b = new Box<int>();
        b[0] = 4;
        b[1] = 5;
        return b[0] * 10 + b[1];
    }
}
)ns", 45);
    expectRun(tests, root, "P3IndexerWhereSetMoveOnly", box + R"ns(
class Program
{
    private static int Run()
    {
        raii Box<Guard> b = new Box<Guard>();
        b[0] = Make.G(7);
        b[0] = Make.G(8);
        return b.RefAt(0).Id;
    }
    public static int Main() { int r = Run(); return r * 100 + Counter.Drops; }
}
)ns", 878);
    expectReject(tests, root, "P3IndexerWhereGetMoveOnly", box + R"ns(
class Program
{
    public static int Main()
    {
        raii Box<Guard> b = new Box<Guard>();
        b[0] = Make.G(7);
        return b[0].Id;
    }
}
)ns", "'Box.get_Item' requires copyable storage ('where T : copy')");
    expectReject(tests, root, "P3PropertyWhereGetMoveOnly", R"ns(
struct Guard
{
    public int Id;
    ~Guard() { }
}
class Holder<T>
{
    private T v;
    public T Value
    {
        get where T : copy { return v; }
        set { v = move(value); }
    }
}
class Program
{
    public static int Main()
    {
        raii Holder<Guard> h = new Holder<Guard>();
        Guard g;
        g.Id = 7;
        h.Value = move(g);
        return h.Value.Id;
    }
}
)ns", "'Holder.get_Value' requires copyable storage ('where T : copy')");
}

/// EN: Freeze 3.0 P3. A `ref` local of a move-only type aliases its place: binding it is not a copy
///     (no NS3020) and it is not destroyed at the end of its scope; the owner destroys the value once.
/// ES: Congelamiento 3.0 P3. Un local `ref` de tipo move-only es alias de su lugar: enlazarlo no es
///     una copia (sin NS3020) y no se destruye al final de su scope; el dueno destruye el valor una vez.
void testMoveOnlyReferenceLocal(TestContext& tests, const std::filesystem::path& root)
{
    expectRun(tests, root, "P3MoveOnlyRefLocal", R"ns(
class Counter { public static int Drops; }
struct Guard
{
    public int Id;
    ~Guard() { Counter.Drops = Counter.Drops + 1; }
}
class Program
{
    private static int Run()
    {
        Guard[] arr = new Guard[1];
        Guard g;
        g.Id = 4;
        arr[0] = move(g);
        ref Guard r = ref arr[0];
        r.Id = 6;
        int v = arr[0].Id;
        delete[] arr;
        return v;
    }
    public static int Main() { int r = Run(); return r * 100 + Counter.Drops; }
}
)ns", 601);
}

/// EN: Freeze 3.0 P3 (library). Reference access without copies for any T: List/Span/Stack/Queue/
///     PriorityQueue RefAt, Peek by reference, SortedSet Min/Max/RefAt and SortedList KeyAt readonly,
///     ValueAt and LinkedListNode.Value by reference, and `foreach (ref ...)` over the live
///     enumerators. Lookups hand their parameter on without copying; what returns a copy requires
///     `copy`. RefAt out of range panics.
/// ES: Congelamiento 3.0 P3 (biblioteca). Acceso por referencia sin copias para todo T: RefAt de
///     List/Span/Stack/Queue/PriorityQueue, Peek por referencia, Min/Max/RefAt de SortedSet y KeyAt de
///     SortedList de solo lectura, ValueAt y LinkedListNode.Value por referencia, y `foreach (ref ...)`
///     sobre los enumeradores vivos. Las busquedas entregan su parametro sin copiarlo; lo que
///     devuelve una copia exige `copy`. RefAt fuera de rango hace panic.
void testCollectionReferences(TestContext& tests, const std::filesystem::path& root)
{
    expectRun(tests, root, "P3ListRefAtMoveOnly", R"ns(
using NativeSharp.Core;
using NativeSharp.Core.Collections.Generic;
class Counter { public static int Drops; }
struct Guard
{
    public int Id;
    ~Guard() { Counter.Drops = Counter.Drops + 1; }
    public static bool operator <(in Guard a, in Guard b) { return a.Id < b.Id; }
    public static bool operator >(in Guard a, in Guard b) { return a.Id > b.Id; }
    public static bool operator ==(in Guard a, in Guard b) { return a.Id == b.Id; }
    public static bool operator !=(in Guard a, in Guard b) { return a.Id != b.Id; }
}
class Box { public int V; public Box(int v) { V = v; } ~Box() { Counter.Drops = Counter.Drops * 10 + V; } }
class Make { public static Guard G(int id) { Guard g; g.Id = id; return move(g); } }
class Program
{
    private static int Run()
    {
        int id = 0;
        int during = 0;
        {
            raii List<Guard> list = new List<Guard>();
            list.Add(Make.G(7));
            list.Add(Make.G(8));
            id = list.RefAt(0).Id;
            list.RefAt(1).Id = 9;
            foreach (ref Guard g in list) { id = id * 10 + g.Id; }
            during = Counter.Drops;
        }
        return id * 10 + during;
    }
    public static int Main()
    {
        int r = Run();
        return r * 100 + Counter.Drops;
    }
}
)ns", 779002, true);
    expectRun(tests, root, "P3ListCopyableUnchanged", R"ns(
using NativeSharp.Core;
using NativeSharp.Core.Collections.Generic;
class Counter { public static int Drops; }
struct Guard
{
    public int Id;
    ~Guard() { Counter.Drops = Counter.Drops * 10 + Id; }
    public static bool operator <(in Guard a, in Guard b) { return a.Id < b.Id; }
    public static bool operator >(in Guard a, in Guard b) { return a.Id > b.Id; }
    public static bool operator ==(in Guard a, in Guard b) { return a.Id == b.Id; }
    public static bool operator !=(in Guard a, in Guard b) { return a.Id != b.Id; }
}
class Box { public int V; public Box(int v) { V = v; } ~Box() { Counter.Drops = Counter.Drops * 10 + V; } }
class Make { public static Guard G(int id) { Guard g; g.Id = id; return move(g); } }
class Program
{
    private static int Run()
    {
        raii List<int> a = new List<int>();
        a.Add(1);
        a.Add(2);
        a[1] = 5;
        int sum = a[0] * 10 + a[1];
        foreach (int x in a) { sum = sum * 10 + x; }
        foreach (ref mut int y in a) { y = y + 1; }
        sum = sum * 10 + a[0] + a[1];
        raii List<Box> b = new List<Box>();
        b.Add(new Box(3));
        b[0] = new Box(4);
        foreach (Box z in b) { sum = sum * 10 + z.V; }
        return sum % 1000000;
    }
    public static int Main()
    {
        int r = Run();
        return r * 1000 + Counter.Drops;
    }
}
)ns", 151584034, true);
    expectRun(tests, root, "P3PeekByReference", R"ns(
using NativeSharp.Core;
using NativeSharp.Core.Collections.Generic;
class Counter { public static int Drops; }
struct Guard
{
    public int Id;
    ~Guard() { Counter.Drops = Counter.Drops + 1; }
    public static bool operator <(in Guard a, in Guard b) { return a.Id < b.Id; }
    public static bool operator >(in Guard a, in Guard b) { return a.Id > b.Id; }
    public static bool operator ==(in Guard a, in Guard b) { return a.Id == b.Id; }
    public static bool operator !=(in Guard a, in Guard b) { return a.Id != b.Id; }
}
class Box { public int V; public Box(int v) { V = v; } ~Box() { Counter.Drops = Counter.Drops * 10 + V; } }
class Make { public static Guard G(int id) { Guard g; g.Id = id; return move(g); } }
class Program
{
    private static int Run()
    {
        int r = 0;
        {
            raii Stack<Guard> s = new Stack<Guard>();
            s.Push(Make.G(1));
            s.Push(Make.G(2));
            raii Queue<Guard> q = new Queue<Guard>();
            q.Enqueue(Make.G(3));
            raii PriorityQueue<Guard, int> p = new PriorityQueue<Guard, int>();
            p.Enqueue(Make.G(4), 1);
            try
            {
                r = s.Peek().Id * 100 + q.Peek().Id * 10 + p.Peek().Id;
                s.Peek().Id = 5;
            }
            catch (InvalidOperationException ex) { r = -1; }
            foreach (ref Guard g in s) { r = r * 10 + g.Id; }
            foreach (ref Guard h in q) { r = r * 10 + h.Id; }
            foreach (ref Guard k in p) { r = r * 10 + k.Id; }
        }
        return r % 100000;
    }
    public static int Main()
    {
        int r = Run();
        return r * 100 + Counter.Drops;
    }
}
)ns", 4513404, true);
    expectRun(tests, root, "P3SortedSetMoveOnly", R"ns(
using NativeSharp.Core;
using NativeSharp.Core.Collections.Generic;
class Counter { public static int Drops; }
struct Guard
{
    public int Id;
    ~Guard() { Counter.Drops = Counter.Drops + 1; }
    public static bool operator <(in Guard a, in Guard b) { return a.Id < b.Id; }
    public static bool operator >(in Guard a, in Guard b) { return a.Id > b.Id; }
    public static bool operator ==(in Guard a, in Guard b) { return a.Id == b.Id; }
    public static bool operator !=(in Guard a, in Guard b) { return a.Id != b.Id; }
}
class Box { public int V; public Box(int v) { V = v; } ~Box() { Counter.Drops = Counter.Drops * 10 + V; } }
class Make { public static Guard G(int id) { Guard g; g.Id = id; return move(g); } }
class Program
{
    private static int Run()
    {
        int r = 0;
        {
            raii SortedSet<Guard> s = new SortedSet<Guard>();
            bool a = s.Add(Make.G(3));
            bool b = s.Add(Make.G(1));
            bool c = s.Add(Make.G(3));
            r = (a ? 1 : 0) * 100 + (b ? 1 : 0) * 10 + (c ? 1 : 0);
            try { r = r * 100 + s.Min().Id * 10 + s.Max().Id; } catch (InvalidOperationException ex) { r = -1; }
            r = r * 10 + (s.Contains(Make.G(1)) ? 1 : 0);
            r = r * 10 + (s.Remove(Make.G(1)) ? 1 : 0);
            r = r * 10 + s.Count();
        }
        return r % 100000;
    }
    public static int Main()
    {
        int r = Run();
        return r * 100 + Counter.Drops;
    }
}
)ns", 1311105, true);
}

/// EN: Freeze 3.0 P3 (library), continued: SortedList, LinkedList and Span references, RefAt out of
///     range, and the copying List getter rejected for a move-only T.
/// ES: Congelamiento 3.0 P3 (biblioteca), continuacion: referencias de SortedList, LinkedList y Span,
///     RefAt fuera de rango, y el getter de List que copia rechazado para un T move-only.
void testCollectionReferencesSorted(TestContext& tests, const std::filesystem::path& root)
{
    expectRun(tests, root, "P3SortedListReferences", R"ns(
using NativeSharp.Core;
using NativeSharp.Core.Collections.Generic;
class Counter { public static int Drops; }
struct Guard
{
    public int Id;
    ~Guard() { Counter.Drops = Counter.Drops + 1; }
    public static bool operator <(in Guard a, in Guard b) { return a.Id < b.Id; }
    public static bool operator >(in Guard a, in Guard b) { return a.Id > b.Id; }
    public static bool operator ==(in Guard a, in Guard b) { return a.Id == b.Id; }
    public static bool operator !=(in Guard a, in Guard b) { return a.Id != b.Id; }
}
class Box { public int V; public Box(int v) { V = v; } ~Box() { Counter.Drops = Counter.Drops * 10 + V; } }
class Make { public static Guard G(int id) { Guard g; g.Id = id; return move(g); } }
class Program
{
    private static int Run()
    {
        int r = 0;
        {
            raii SortedList<Guard, Guard> s = new SortedList<Guard, Guard>();
            s.AddOrUpdate(Make.G(2), Make.G(5));
            s.AddOrUpdate(Make.G(1), Make.G(6));
            r = s.KeyAt(0).Id * 10 + s.ValueAt(0).Id;
            s.ValueAt(0).Id = 7;
            r = r * 10 + s.ValueAt(0).Id;
            r = r * 10 + (s.ContainsKey(Make.G(2)) ? 1 : 0);
            r = r * 10 + (s.Remove(Make.G(2)) ? 1 : 0);
            raii SortedDictionary<Guard, int> d = new SortedDictionary<Guard, int>();
            d.AddOrUpdate(Make.G(4), 1);
            r = r * 10 + (d.ContainsKey(Make.G(4)) ? 1 : 0);
        }
        return r % 100000;
    }
    public static int Main()
    {
        int r = Run();
        return r * 100 + Counter.Drops;
    }
}
)ns", 6711108, true);
    expectRun(tests, root, "P3LinkedListReferences", R"ns(
using NativeSharp.Core;
using NativeSharp.Core.Collections.Generic;
class Counter { public static int Drops; }
struct Guard
{
    public int Id;
    ~Guard() { Counter.Drops = Counter.Drops + 1; }
    public static bool operator <(in Guard a, in Guard b) { return a.Id < b.Id; }
    public static bool operator >(in Guard a, in Guard b) { return a.Id > b.Id; }
    public static bool operator ==(in Guard a, in Guard b) { return a.Id == b.Id; }
    public static bool operator !=(in Guard a, in Guard b) { return a.Id != b.Id; }
}
class Box { public int V; public Box(int v) { V = v; } ~Box() { Counter.Drops = Counter.Drops * 10 + V; } }
class Make { public static Guard G(int id) { Guard g; g.Id = id; return move(g); } }
class Program
{
    private static int Run()
    {
        int r = 0;
        {
            raii LinkedList<Guard> l = new LinkedList<Guard>();
            LinkedListNode<Guard> n = l.AddLast(Make.G(1));
            l.AddLast(Make.G(2));
            n.Value().Id = 3;
            r = n.Value().Id;
            r = r * 10 + (l.Contains(Make.G(2)) ? 1 : 0);
            r = r * 10 + (l.Find(Make.G(3)) != null ? 1 : 0);
            foreach (ref Guard g in l) { r = r * 10 + g.Id; }
        }
        return r % 100000;
    }
    public static int Main()
    {
        int r = Run();
        return r * 100 + Counter.Drops;
    }
}
)ns", 3113204, true);
    expectRun(tests, root, "P3SpanRefAt", R"ns(
using NativeSharp.Core;
using NativeSharp.Core.Collections.Generic;
class Counter { public static int Drops; }
struct Guard
{
    public int Id;
    ~Guard() { Counter.Drops = Counter.Drops + 1; }
    public static bool operator <(in Guard a, in Guard b) { return a.Id < b.Id; }
    public static bool operator >(in Guard a, in Guard b) { return a.Id > b.Id; }
    public static bool operator ==(in Guard a, in Guard b) { return a.Id == b.Id; }
    public static bool operator !=(in Guard a, in Guard b) { return a.Id != b.Id; }
}
class Box { public int V; public Box(int v) { V = v; } ~Box() { Counter.Drops = Counter.Drops * 10 + V; } }
class Make { public static Guard G(int id) { Guard g; g.Id = id; return move(g); } }
class Program
{
    private static int Run()
    {
        Guard[] arr = new Guard[2];
        arr[0] = Make.G(1);
        arr[1] = Make.G(2);
        Span<Guard> s = Spans.OfRange<Guard>(arr, 0, 2);
        s.RefAt(0).Id = 5;
        int r = s.RefAt(0).Id * 10 + s.AsReadOnly().RefAt(1).Id;
        int[] nums = new int[3];
        Span<int> t = Spans.OfRange<int>(nums, 0, 3);
        t[0] = 4;
        t.Fill(6);
        t[1] = 7;
        r = r * 1000 + t[0] * 100 + t[1] * 10 + t[2] + t[5];
        delete[] nums;
        delete[] arr;
        return r;
    }
    public static int Main()
    {
        int r = Run();
        return r * 100 + Counter.Drops;
    }
}
)ns", 5267602, true);
    expectRun(tests, root, "P3RefAtOutOfRange", R"ns(
using NativeSharp.Core;
using NativeSharp.Core.Collections.Generic;
class Counter { public static int Drops; }
struct Guard
{
    public int Id;
    ~Guard() { Counter.Drops = Counter.Drops * 10 + Id; }
    public static bool operator <(in Guard a, in Guard b) { return a.Id < b.Id; }
    public static bool operator >(in Guard a, in Guard b) { return a.Id > b.Id; }
    public static bool operator ==(in Guard a, in Guard b) { return a.Id == b.Id; }
    public static bool operator !=(in Guard a, in Guard b) { return a.Id != b.Id; }
}
class Box { public int V; public Box(int v) { V = v; } ~Box() { Counter.Drops = Counter.Drops * 10 + V; } }
class Make { public static Guard G(int id) { Guard g; g.Id = id; return move(g); } }
class Program
{
    private static int Run()
    {
        raii List<int> a = new List<int>();
        a.Add(1);
        return a.RefAt(1);
    }
    public static int Main()
    {
        int r = Run();
        return r * 1000 + Counter.Drops;
    }
}
)ns", 142, true);
    expectReject(tests, root, "P3ListGetMoveOnly", R"ns(
using NativeSharp.Core;
using NativeSharp.Core.Collections.Generic;
struct Guard
{
    public int Id;
    ~Guard() { }
}
class Make { public static Guard G(int id) { Guard g; g.Id = id; return move(g); } }
class Program
{
    public static int Main()
    {
        raii List<Guard> list = new List<Guard>();
        list.Add(Make.G(7));
        return list[0].Id;
    }
}
)ns", "'List.get_Item' requires copyable storage ('where T : copy')", true);
}


/// EN: Freeze 3.0 P4. A span made from an array keeps the array and its first index, so the array's
///     liveness map applies through the span, its slices, AsReadOnly, the implicit array view and
///     foreach: reading a dead slot traps (146), Put revives a dead slot and destroys a live one.
///     Spans over int arrays, raw pointers, parameter packs and u8 literals behave as before.
/// ES: Congelamiento 3.0 P4. Un span hecho de un arreglo guarda el arreglo y su primer indice, asi
///     que el mapa de vida del arreglo se aplica por el span, sus Slice, AsReadOnly, la vista
///     implicita del arreglo y foreach: leer un slot muerto atrapa (146), Put revive un slot muerto
///     y destruye uno vivo. Los spans sobre arreglos de int, punteros crudos, paquetes de parametros
///     y literales u8 se comportan como antes.
void testSpanLivenessMap(TestContext& tests, const std::filesystem::path& root)
{
    expectRun(tests, root, "P4SpanDeadRead", R"ns(
using NativeSharp.Core;
using NativeSharp.Core.Collections.Generic;
class Counter { public static int Drops; }
struct Guard
{
    public int Id;
    ~Guard() { Counter.Drops = Counter.Drops + 1; }
}
class Make { public static Guard G(int id) { Guard g; g.Id = id; return move(g); } }
class Program
{
    private static int Run()
    {
        Guard[] arr = new Guard[3];
        arr[0] = Make.G(1);
        Span<Guard> s = Spans.OfRange<Guard>(arr, 0, 3);
        int v = s.RefAt(0).Id * 10 + s.RefAt(1).Id;
        delete[] arr;
        return v;
    }
    public static int Main()
    {
        int r = Run();
        return r * 100 + Counter.Drops;
    }
}
)ns", 146, true);
    expectRun(tests, root, "P4SpanLiveRead", R"ns(
using NativeSharp.Core;
using NativeSharp.Core.Collections.Generic;
class Counter { public static int Drops; }
struct Guard
{
    public int Id;
    ~Guard() { Counter.Drops = Counter.Drops + 1; }
}
class Make { public static Guard G(int id) { Guard g; g.Id = id; return move(g); } }
class Program
{
    private static int Run()
    {
        Guard[] arr = new Guard[3];
        arr[0] = Make.G(1);
        Span<Guard> s = Spans.OfRange<Guard>(arr, 0, 3);
        int v = s.RefAt(0).Id * 10;
        delete[] arr;
        return v;
    }
    public static int Main()
    {
        int r = Run();
        return r * 100 + Counter.Drops;
    }
}
)ns", 1001, true);
    expectRun(tests, root, "P4SpanPutRevives", R"ns(
using NativeSharp.Core;
using NativeSharp.Core.Collections.Generic;
class Counter { public static int Drops; }
struct Guard
{
    public int Id;
    ~Guard() { Counter.Drops = Counter.Drops + 1; }
}
class Make { public static Guard G(int id) { Guard g; g.Id = id; return move(g); } }
class Program
{
    private static int Run()
    {
        Guard[] arr = new Guard[2];
        Span<Guard> s = Spans.OfRange<Guard>(arr, 0, 2);
        s.Put(1, Make.G(5));
        s.Put(1, Make.G(6));
        int during = Counter.Drops;
        int v = arr[1].Id * 10 + during;
        delete[] arr;
        return v;
    }
    public static int Main()
    {
        int r = Run();
        return r * 100 + Counter.Drops;
    }
}
)ns", 6102, true);
    expectRun(tests, root, "P4SliceKeepsMap", R"ns(
using NativeSharp.Core;
using NativeSharp.Core.Collections.Generic;
class Counter { public static int Drops; }
struct Guard
{
    public int Id;
    ~Guard() { Counter.Drops = Counter.Drops + 1; }
}
class Make { public static Guard G(int id) { Guard g; g.Id = id; return move(g); } }
class Program
{
    private static int Run()
    {
        Guard[] arr = new Guard[3];
        arr[1] = Make.G(4);
        Span<Guard> s = Spans.OfRange<Guard>(arr, 0, 3).Slice(1);
        int v = s.RefAt(0).Id;
        v = v * 10 + s.RefAt(1).Id;
        delete[] arr;
        return v;
    }
    public static int Main()
    {
        int r = Run();
        return r * 100 + Counter.Drops;
    }
}
)ns", 146, true);
    expectRun(tests, root, "P4AsReadOnlyKeepsMap", R"ns(
using NativeSharp.Core;
using NativeSharp.Core.Collections.Generic;
class Counter { public static int Drops; }
struct Guard
{
    public int Id;
    ~Guard() { Counter.Drops = Counter.Drops + 1; }
}
class Make { public static Guard G(int id) { Guard g; g.Id = id; return move(g); } }
class Program
{
    private static int Run()
    {
        Guard[] arr = new Guard[2];
        arr[0] = Make.G(3);
        ReadOnlySpan<Guard> r = Spans.OfRange<Guard>(arr, 0, 2).AsReadOnly();
        int v = r.RefAt(0).Id;
        v = v * 10 + r.RefAt(1).Id;
        delete[] arr;
        return v;
    }
    public static int Main()
    {
        int r = Run();
        return r * 100 + Counter.Drops;
    }
}
)ns", 146, true);
    expectRun(tests, root, "P4ForeachDeadSlot", R"ns(
using NativeSharp.Core;
using NativeSharp.Core.Collections.Generic;
class Counter { public static int Drops; }
struct Guard
{
    public int Id;
    ~Guard() { Counter.Drops = Counter.Drops + 1; }
}
class Make { public static Guard G(int id) { Guard g; g.Id = id; return move(g); } }
class Program
{
    private static int Run()
    {
        Guard[] arr = new Guard[2];
        arr[0] = Make.G(2);
        Span<Guard> s = Spans.OfRange<Guard>(arr, 0, 2);
        int v = 0;
        foreach (ref Guard g in s) { v = v * 10 + g.Id + 1; }
        delete[] arr;
        return v;
    }
    public static int Main()
    {
        int r = Run();
        return r * 100 + Counter.Drops;
    }
}
)ns", 146, true);
    expectRun(tests, root, "P4ForeachLiveSlots", R"ns(
using NativeSharp.Core;
using NativeSharp.Core.Collections.Generic;
class Counter { public static int Drops; }
struct Guard
{
    public int Id;
    ~Guard() { Counter.Drops = Counter.Drops + 1; }
}
class Make { public static Guard G(int id) { Guard g; g.Id = id; return move(g); } }
class Program
{
    private static int Run()
    {
        Guard[] arr = new Guard[2];
        arr[0] = Make.G(2);
        arr[1] = Make.G(4);
        Span<Guard> s = Spans.OfRange<Guard>(arr, 0, 2);
        int v = 0;
        foreach (ref Guard g in s) { v = v * 10 + g.Id + 1; }
        delete[] arr;
        return v;
    }
    public static int Main()
    {
        int r = Run();
        return r * 100 + Counter.Drops;
    }
}
)ns", 3502, true);
    expectRun(tests, root, "P4ImplicitViewKeepsMap", R"ns(
using NativeSharp.Core;
using NativeSharp.Core.Collections.Generic;
class Counter { public static int Drops; }
struct Guard
{
    public int Id;
    ~Guard() { Counter.Drops = Counter.Drops + 1; }
}
class Make { public static Guard G(int id) { Guard g; g.Id = id; return move(g); } }
class Program
{
    private static int Run()
    {
        Guard[] arr = new Guard[2];
        arr[0] = Make.G(7);
        Span<Guard> s = arr;
        ReadOnlySpan<Guard> r = s;
        int v = s.RefAt(0).Id * 10 + r.RefAt(0).Id;
        v = v * 10 + r.RefAt(1).Id;
        delete[] arr;
        return v;
    }
    public static int Main()
    {
        int r = Run();
        return r * 100 + Counter.Drops;
    }
}
)ns", 146, true);
    expectRun(tests, root, "P4IntSpanUnchanged", R"ns(
using NativeSharp.Core;
using NativeSharp.Core.Collections.Generic;
class Counter { public static int Drops; }
struct Guard
{
    public int Id;
    ~Guard() { Counter.Drops = Counter.Drops + 1; }
}
class Make { public static Guard G(int id) { Guard g; g.Id = id; return move(g); } }
class Program
{
    private static int Run()
    {
        int[] nums = new int[4];
        Span<int> t = Spans.OfRange<int>(nums, 0, 4);
        t[0] = 4;
        t.Fill(6);
        t[1] = 7;
        Span<int> u = t.Slice(2);
        u[0] = 8;
        ReadOnlySpan<int> ro = t.AsReadOnly();
        int sum = 0;
        foreach (int x in ro) { sum = sum * 10 + x; }
        foreach (ref mut int y in t) { y = y + 1; }
        Span<int> w = nums;
        sum = sum * 10 + w[0] + t[5];
        int[] other = new int[4];
        int copied = t.CopyTo(Spans.OfRange<int>(other, 0, 4));
        sum = sum * 10 + other[3] + copied;
        delete[] other;
        delete[] nums;
        return sum % 1000000;
    }
    public static int Main()
    {
        int r = Run();
        return r * 100 + Counter.Drops;
    }
}
)ns", 67868100, true);
    expectRun(tests, root, "P4RawSpanUnchanged", R"ns(
using NativeSharp.Core;
using NativeSharp.Core.Collections.Generic;
class Counter { public static int Drops; }
struct Guard
{
    public int Id;
    ~Guard() { Counter.Drops = Counter.Drops + 1; }
}
class Make { public static Guard G(int id) { Guard g; g.Id = id; return move(g); } }
class Program
{
    private static int Run()
    {
        int[] nums = new int[3];
        nums[1] = 5;
        Span<int> raw = Spans.OfRaw<int>(&nums[0], 3);
        raw[2] = 9;
        int v = raw[1] * 10 + nums[2];
        Span<int> part = raw.Slice(1);
        v = v * 10 + part.RefAt(1);
        delete[] nums;
        return v;
    }
    public static int Main()
    {
        int r = Run();
        return r * 100 + Counter.Drops;
    }
}
)ns", 59900, true);
    expectRun(tests, root, "P4PackAndUtf8Unchanged", R"ns(
using NativeSharp.Core;
using NativeSharp.Core.Collections.Generic;
class Counter { public static int Drops; }
struct Guard
{
    public int Id;
    ~Guard() { Counter.Drops = Counter.Drops + 1; }
}
class Make { public static Guard G(int id) { Guard g; g.Id = id; return move(g); } }
class Pack
{
    public static int Sum(params ReadOnlySpan<int> values)
    {
        int s = 0;
        foreach (int v in values) { s = s + v; }
        return s;
    }
}
class Program
{
    private static int Run()
    {
        ReadOnlySpan<byte> u = u8"abc";
        return Pack.Sum(1, 2, 3) * 1000 + u.Length() * 100 + u[1] - 90;
    }
    public static int Main()
    {
        int r = Run();
        return r * 100 + Counter.Drops;
    }
}
)ns", 630800, true);
}

/// EN: Freeze 3.0 P10. A non-owned parameter of an exception type is a loan from the caller (it may
///     be a caught exception): storing it, or a local that copies it, in a field, a static or an
///     element, returning it or capturing it in a lambda is rejected. Reading it, passing it on and
///     keeping it in a local are allowed; an owned parameter takes it with move(...).
/// ES: Congelamiento 3.0 P10. Un parametro no owned de tipo excepcion es un prestamo del llamador
///     (puede ser una excepcion atrapada): guardarlo, o un local que lo copia, en un campo, un static
///     o un elemento, retornarlo o capturarlo en un lambda se rechaza. Leerlo, pasarlo y tenerlo en
///     un local se permite; un parametro owned lo toma con move(...).
void testCaughtExceptionParameterLoan(TestContext& tests, const std::filesystem::path& root)
{
    expectReject(tests, root, "P10LoanStoredInField", R"ns(
using NativeSharp.Core;
class Boom : Exception
{
    public static int Destroyed;
    public int Code;
    public Boom(int code) : base("boom") { Code = code; }
    ~Boom() { Destroyed = Destroyed + 1; }
}
class Holder { public Boom Saved; public owned Boom Kept; public static Boom Global; }
class Program
{
    public static void Store(Holder h, Boom b) { h.Saved = b; }
    public static int Main()
    {
        raii Holder holder = new Holder();
        try { throw new Boom(7); }
        catch (Boom problem) { Store(holder, problem); }
        return Boom.Destroyed;
    }
}
)ns", "is a loan from the caller");
    expectReject(tests, root, "P10LoanStoredInStatic", R"ns(
using NativeSharp.Core;
class Boom : Exception
{
    public static int Destroyed;
    public int Code;
    public Boom(int code) : base("boom") { Code = code; }
    ~Boom() { Destroyed = Destroyed + 1; }
}
class Holder { public Boom Saved; public owned Boom Kept; public static Boom Global; }
class Program
{
    public static void Store(Holder h, Boom b) { Holder.Global = b; }
    public static int Main()
    {
        raii Holder holder = new Holder();
        try { throw new Boom(7); }
        catch (Boom problem) { Store(holder, problem); }
        return Boom.Destroyed;
    }
}
)ns", "is a loan from the caller");
    expectReject(tests, root, "P10LoanStoredInElement", R"ns(
using NativeSharp.Core;
class Boom : Exception
{
    public static int Destroyed;
    public int Code;
    public Boom(int code) : base("boom") { Code = code; }
    ~Boom() { Destroyed = Destroyed + 1; }
}
class Holder { public Boom Saved; public owned Boom Kept; public static Boom Global; }
class Program
{
    public static void Store(Holder h, Boom b) { Boom[] a = new Boom[1]; a[0] = b; }
    public static int Main()
    {
        raii Holder holder = new Holder();
        try { throw new Boom(7); }
        catch (Boom problem) { Store(holder, problem); }
        return Boom.Destroyed;
    }
}
)ns", "is a loan from the caller");
    expectReject(tests, root, "P10LoanReturned", R"ns(
using NativeSharp.Core;
class Boom : Exception
{
    public static int Destroyed;
    public int Code;
    public Boom(int code) : base("boom") { Code = code; }
    ~Boom() { Destroyed = Destroyed + 1; }
}
class Holder { public Boom Saved; public owned Boom Kept; public static Boom Global; }
class Program
{
    public static Boom Pass(Boom b) { return b; }
    public static void Store(Holder h, Boom b) { Boom x = Pass(b); }
    public static int Main()
    {
        raii Holder holder = new Holder();
        try { throw new Boom(7); }
        catch (Boom problem) { Store(holder, problem); }
        return Boom.Destroyed;
    }
}
)ns", "cannot be returned");
    expectReject(tests, root, "P10LoanAliasStored", R"ns(
using NativeSharp.Core;
class Boom : Exception
{
    public static int Destroyed;
    public int Code;
    public Boom(int code) : base("boom") { Code = code; }
    ~Boom() { Destroyed = Destroyed + 1; }
}
class Holder { public Boom Saved; public owned Boom Kept; public static Boom Global; }
class Program
{
    public static void Store(Holder h, Boom b) { Boom alias = b; h.Saved = alias; }
    public static int Main()
    {
        raii Holder holder = new Holder();
        try { throw new Boom(7); }
        catch (Boom problem) { Store(holder, problem); }
        return Boom.Destroyed;
    }
}
)ns", "the exception in 'alias' is a loan");
    expectReject(tests, root, "P10LoanCaptured", R"ns(
using NativeSharp.Core;
class Boom : Exception
{
    public static int Destroyed;
    public int Code;
    public Boom(int code) : base("boom") { Code = code; }
    ~Boom() { Destroyed = Destroyed + 1; }
}
delegate int Getter();
class Holder { public Boom Saved; public owned Boom Kept; public static Boom Global; }
class Program
{
    public static void Store(Holder h, Boom b) { Getter f = () => b.Code; }
    public static int Main()
    {
        raii Holder holder = new Holder();
        try { throw new Boom(7); }
        catch (Boom problem) { Store(holder, problem); }
        return Boom.Destroyed;
    }
}
)ns", "cannot be captured by a lambda");
    expectReject(tests, root, "P10BaseExceptionLoan", R"ns(
using NativeSharp.Core;
class Boom : Exception
{
    public static int Destroyed;
    public int Code;
    public Boom(int code) : base("boom") { Code = code; }
    ~Boom() { Destroyed = Destroyed + 1; }
}
class Holder { public Boom Saved; public owned Boom Kept; public static Boom Global; }
class Program
{
    public static void Store(Holder h, Exception b) { h.Saved = null; Exception e2 = b; }
    public static void Keep(Holder h, Exception b)
    {
        Holder.Global = null;
        Exception[] a = new Exception[1];
        a[0] = b;
    }
    public static int Main()
    {
        raii Holder holder = new Holder();
        try { throw new Boom(7); }
        catch (Boom problem) { Store(holder, problem); }
        return Boom.Destroyed;
    }
}
)ns", "is a loan from the caller");
    expectRun(tests, root, "P10LoanReadAndPassed", R"ns(
using NativeSharp.Core;
class Boom : Exception
{
    public static int Destroyed;
    public int Code;
    public Boom(int code) : base("boom") { Code = code; }
    ~Boom() { Destroyed = Destroyed + 1; }
}
class Holder { public Boom Saved; public owned Boom Kept; public static Boom Global; }
class Program
{
    public static int Read(Boom b) { return b.Code; }
    public static int Forward(Boom b) { Boom local = b; return Read(local) + Read(b); }
    public static int Main()
    {
        int r = 0;
        try { throw new Boom(7); }
        catch (Boom problem) { r = Forward(problem); }
        return r * 10 + Boom.Destroyed;
    }
}
)ns", 141);
    expectRun(tests, root, "P10OwnedTransfer", R"ns(
using NativeSharp.Core;
class Boom : Exception
{
    public static int Destroyed;
    public int Code;
    public Boom(int code) : base("boom") { Code = code; }
    ~Boom() { Destroyed = Destroyed + 1; }
}
class Holder { public Boom Saved; public owned Boom Kept; public static Boom Global; }
class Program
{
    public static void Keep(Holder h, owned Boom b) { h.Kept = move(b); }
    public static int Main()
    {
        int r = 0;
        {
            raii Holder holder = new Holder();
            try { throw new Boom(7); }
            catch (Boom problem) { Keep(holder, move(problem)); }
            r = holder.Kept.Code * 10 + Boom.Destroyed;
        }
        return r * 10 + Boom.Destroyed;
    }
}
)ns", 701);
    expectRun(tests, root, "P10LoanLocal", R"ns(
using NativeSharp.Core;
class Boom : Exception
{
    public static int Destroyed;
    public int Code;
    public Boom(int code) : base("boom") { Code = code; }
    ~Boom() { Destroyed = Destroyed + 1; }
}
class Holder { public Boom Saved; public owned Boom Kept; public static Boom Global; }
class Program
{
    public static int Use(Boom b) { Boom x = null; x = b; return x.Code; }
    public static int Main()
    {
        int r = 0;
        try { throw new Boom(7); }
        catch (Boom problem) { r = Use(problem); }
        return r * 10 + Boom.Destroyed;
    }
}
)ns", 71);
}

/// EN: Freeze 3.0 P10. The catch variable receives the caught exception: a lambda may not capture it
///     (the delegate may outlive the handler); a lambda in the handler that does not capture it is fine.
/// ES: Congelamiento 3.0 P10. La variable del catch recibe la excepcion atrapada: un lambda no puede
///     capturarla (el delegate puede sobrevivir al handler); un lambda del handler que no la captura sirve.
void testCaughtExceptionNotCaptured(TestContext& tests, const std::filesystem::path& root)
{
    expectReject(tests, root, "P10CatchVariableCaptured", R"ns(
using NativeSharp.Core;
class Boom : Exception
{
    public static int Destroyed;
    public int Code;
    public Boom(int code) : base("boom") { Code = code; }
    ~Boom() { Destroyed = Destroyed + 1; }
}
delegate int Getter();
class Holder { public Boom Saved; public owned Boom Kept; public static Boom Global; }
class Program
{
    public static Getter Saved;
    public static void Store(Holder h, Boom b) { }
    public static int Main()
    {
        raii Holder holder = new Holder();
        try { throw new Boom(7); }
        catch (Boom problem) { Saved = () => problem.Code; }
        return Boom.Destroyed;
    }
}
)ns", "cannot be captured by a lambda; move it into an owner first");
    expectRun(tests, root, "P10LambdaInHandler", R"ns(
using NativeSharp.Core;
class Boom : Exception
{
    public static int Destroyed;
    public int Code;
    public Boom(int code) : base("boom") { Code = code; }
    ~Boom() { Destroyed = Destroyed + 1; }
}
delegate int Getter();
class Holder { public Boom Saved; public owned Boom Kept; public static Boom Global; }
class Program
{
    public static Getter Saved;
    public static void Store(Holder h, Boom b) { }
    public static int Main()
    {
        int r = 0;
        try { throw new Boom(7); }
        catch (Boom problem) { int c = problem.Code; Getter g = () => 5; r = g() + c; }
        return r * 10 + Boom.Destroyed;
    }
}
)ns", 121);
}

/// EN: Freeze 3.0 P6 (DEC-09). IAllocator keeps only the C-08 signature. A class that still implements a
///     replaced member gets a diagnostic on that member naming the new signature.
/// ES: Congelamiento 3.0 P6 (DEC-09). IAllocator conserva solo la firma de C-08. Una clase que aun
///     implementa un miembro reemplazado recibe en ese miembro un diagnostico con la firma nueva.
void testReplacedAllocatorSignature(TestContext& tests, const std::filesystem::path& root)
{
    expectReject(tests, root, "P6OldAllocator", R"ns(
using NativeSharp.Core;
class OldArena : IAllocator
{
    public byte* Allocate(int bytes) { return null; }
    public byte* Resize(byte* ptr, int oldBytes, int newBytes) { return null; }
    public void Deallocate(byte* ptr) { }
}
class Program
{
    public static int Main()
    {
        raii OldArena a = new OldArena();
        return 0;
    }
}
)ns",
        "implements the IAllocator signature replaced in 3.0 (C-08); "
        "implement 'byte* Allocate(long bytes, int alignment)'");
    expectReject(tests, root, "P6OldDeallocate", R"ns(
using NativeSharp.Core;
class MixedArena : IAllocator
{
    public byte* Allocate(long bytes, int alignment) { return null; }
    public byte* Resize(byte* ptr, long oldBytes, long newBytes, int alignment) { return null; }
    public void Deallocate(byte* ptr) { }
}
class Program
{
    public static int Main()
    {
        raii MixedArena a = new MixedArena();
        return 0;
    }
}
)ns", "implement 'void Deallocate(byte* ptr, long bytes, int alignment)'");
    expectRun(tests, root, "P6NewAllocator", R"ns(
using NativeSharp.Core;
class NewArena : IAllocator
{
    public byte* Allocate(long bytes, int alignment) { return null; }
    public byte* Resize(byte* ptr, long oldBytes, long newBytes, int alignment) { return null; }
    public void Deallocate(byte* ptr, long bytes, int alignment) { }
}
class Program
{
    public static int Main()
    {
        raii NewArena a = new NewArena();
        return 3;
    }
}
)ns", 3);
}

/// EN: Freeze 3.0 P8. Object ABI 3 marks the array and span layout of DEC-11 and P4. A library with object ABI 2 is
///     rejected and the diagnostic asks for its recompilation; the NSMETA version is not consulted.
/// ES: Congelamiento 3.0 P8. El object ABI 3 marca el layout de arreglos y spans de DEC-11 y P4. Una biblioteca con
///     object ABI 2 se rechaza y el diagnostico pide recompilarla; no se consulta la version de NSMETA.
void testObjectAbiBoundary(TestContext& tests, const std::filesystem::path& root)
{
    (void)root;
    nativesharp::interop::LibraryMetadataCodec codec;
    nativesharp::interop::LibraryMetadata metadata;
    metadata.targetId = 1;
    metadata.pointerSize = 8;
    metadata.pointerAlignment = 8;
    metadata.producerVersion = std::string(nativesharp::kCompilerVersion);
    metadata.targetTriple = "x86_64-pc-windows-msvc";
    metadata.moduleName = "OldObjectAbi";
    metadata.objectAbiVersion = 2;
    std::string error;
    tests.expect(!codec.verifyCurrentCompiler(metadata, error) &&
                     error.find("object ABI expected 3, found 2; recompile the library with the current compiler") !=
                         std::string::npos,
                 "P8: a library with object ABI 2 is rejected with a recompile request (" + error + ")");
    metadata.objectAbiVersion = nativesharp::interop::kObjectAbiVersion;
    error.clear();
    tests.expect(codec.verifyCurrentCompiler(metadata, error),
                 "P8: the same library with the current object ABI is accepted (" + error + ")");
}

/// EN: Freeze 3.0 P3 regression. SortedDictionary's indexer getter forwarded `items[move(key)]`, which is rejected
///     for every key type; a SortedDictionary<int, int> read through the indexer did not compile.
/// ES: Regresion de P3 del congelamiento 3.0. El getter del indexador de SortedDictionary pasaba
///     `items[move(key)]`, que se rechaza con todo tipo de clave; leer un SortedDictionary<int, int> por el
///     indexador no compilaba.
void testSortedDictionaryIndexerGet(TestContext& tests, const std::filesystem::path& root)
{
    expectRun(tests, root, "P3SortedDictionaryGet", R"ns(
using NativeSharp.Core;
using NativeSharp.Core.Collections.Generic;
class Program
{
    public static int Main() throws ArgumentException
    {
        raii SortedDictionary<int, int> d = new SortedDictionary<int, int>();
        d.Add(2, 20);
        d.Add(1, 21);
        return d[1] + d[2] + d[3];
    }
}
)ns", 41);
}

constexpr std::string_view kErrorPrelude = R"ns(
using NativeSharp.Core;
class MyError : Exception { }
class OtherError : Exception { }
public delegate int Work(bool f) throws MyError;
public delegate int Plain(bool f);
)ns";

// EN: C-13. A call, a delegate invocation, a `new` and a `this(...)`/`base(...)` initializer that
//     reach a `throws E` callable need a try whose unfiltered catch accepts E, or an enclosing
//     `throws` of E or a base of E. A lambda body, a catch body and a filtered catch do not count.
// ES: C-13. Una llamada, una invocacion de delegate, un `new` y un inicializador
//     `this(...)`/`base(...)` que llegan a un callable `throws E` necesitan un try cuyo catch sin
//     filtro acepte E, o un `throws` envolvente de E o de una base de E. El cuerpo de una lambda, el
//     de un catch y un catch con filtro no cuentan.
void testDeclaredErrorsRejected(TestContext& tests, const std::filesystem::path& root)
{
    expectReject(tests, root, "C13Call", std::string(kErrorPrelude) + R"ns(
class Program
{
    private static int Fail(bool f) throws MyError { if (f) { throw new MyError(); } return 1; }
    public static int Main() { return Fail(false); }
}
)ns",
        "which is neither caught here nor declared");
    expectReject(tests, root, "C13New", std::string(kErrorPrelude) + R"ns(
class Thing
{
    public Thing(bool f) throws MyError { if (f) { throw new MyError(); } }
}
class Program
{
    public static int Main() { raii Thing t = new Thing(false); return 2; }
}
)ns",
        "which is neither caught here nor declared");
    expectReject(tests, root, "C13This", std::string(kErrorPrelude) + R"ns(
class Thing
{
    public Thing(bool f) throws MyError { if (f) { throw new MyError(); } }
    public Thing() : this(false) { }
}
class Program
{
    public static int Main() { raii Thing t = new Thing(); return 3; }
}
)ns",
        "which is neither caught here nor declared");
    expectReject(tests, root, "C13Base", std::string(kErrorPrelude) + R"ns(
class Base
{
    public Base(bool f) throws MyError { if (f) { throw new MyError(); } }
}
class Derived : Base
{
    public Derived() : base(false) { }
}
class Program
{
    public static int Main() { raii Derived d = new Derived(); return 4; }
}
)ns",
        "which is neither caught here nor declared");
    expectReject(tests, root, "C13Wrong", std::string(kErrorPrelude) + R"ns(
class Program
{
    private static int Fail(bool f) throws MyError { if (f) { throw new MyError(); } return 1; }
    private static int Caller() throws OtherError { return Fail(false); }
    public static int Main() { try { return Caller(); } catch (OtherError e) { return 9; } }
}
)ns",
        "which is neither caught here nor declared");
    expectReject(tests, root, "C13Delegate", std::string(kErrorPrelude) + R"ns(
class Program
{
    public static int Fail(bool f) throws MyError { if (f) { throw new MyError(); } return 1; }
    public static int Main() { Work m = Fail; return m(false); }
}
)ns",
        "which is neither caught here nor declared");
    expectReject(tests, root, "C13LambdaReject", std::string(kErrorPrelude) + R"ns(
class Program
{
    public static int Fail(bool f) throws MyError { if (f) { throw new MyError(); } return 1; }
    public static int Main()
    {
        try
        {
            raii Plain p = [] (bool f) => Program.Fail(f);
            return p(false);
        }
        catch (MyError e) { return 2; }
    }
}
)ns",
        "which is neither caught here nor declared");
    expectReject(tests, root, "C13Filtered", std::string(kErrorPrelude) + R"ns(
class Program
{
    public static int Fail(bool f) throws MyError { if (f) { throw new MyError(); } return 1; }
    public static int Main()
    {
        try { return Fail(false); }
        catch (MyError e) when (e != null) { return 2; }
    }
}
)ns",
        "which is neither caught here nor declared");
    expectReject(tests, root, "C13CatchBody", std::string(kErrorPrelude) + R"ns(
class Program
{
    public static int Fail(bool f) throws MyError { if (f) { throw new MyError(); } return 1; }
    public static int Main()
    {
        try { return 0; }
        catch (MyError e) { return Fail(false); }
    }
}
)ns",
        "which is neither caught here nor declared");
    expectReject(tests, root, "C13AwaitUncaught", std::string(kErrorPrelude) + R"ns(
class Program
{
    private static async int Load(bool f) throws MyError { if (f) { throw new MyError(); } return 5; }
    public static async int Main() { return await Load(false); }
}
)ns",
        "which is neither caught here nor declared");
    expectReject(tests, root, "C13PropGet", std::string(kErrorPrelude) + R"ns(
class Source
{
    public int Value { get throws MyError { return 3; } }
}
class Program
{
    public static int Main() { owned Source source = new Source(); return source.Value; }
}
)ns",
        "which is neither caught here nor declared");
    expectReject(tests, root, "C13IndexSet", std::string(kErrorPrelude) + R"ns(
class Source
{
    public int this[int index]
    {
        get { return 0; }
        set throws MyError { if (index < 0) { throw new MyError(); } }
    }
}
class Program
{
    public static int Main() { owned Source source = new Source(); source[0] = 1; return 4; }
}
)ns",
        "which is neither caught here nor declared");
}

// EN: An async body catches its own throws and the errors of the futures it awaits inside a try,
//     also after a suspension, and destroys the try's raii locals on the way.
// ES: Un cuerpo async atrapa sus propios throw y los errores de los futures que espera dentro de un
//     try, tambien despues de una suspension, y destruye en el camino los locales raii del try.
void testDeclaredErrorsHandled(TestContext& tests, const std::filesystem::path& root)
{
    expectRun(tests, root, "C13AsyncLocalThrow", std::string(kErrorPrelude) + R"ns(
class Program
{
    private static async int Outer()
    {
        int total = 0;
        try { throw new MyError(); } catch (MyError e) { total = total + 10; }
        return total;
    }
    public static int Main() { return await Outer(); }
}
)ns", 10);
    expectRun(tests, root, "C13AsyncAwaitCaught", std::string(kErrorPrelude) + R"ns(
class Source
{
    public int Value { get throws MyError { return 3; } }
}
class Program
{
    private static async int Load(bool f) throws MyError { if (f) { throw new MyError(); } return 5; }
    public static async int Main()
    {
        int total = 0;
        try { total = total + await Load(true); } catch (MyError e) { total = total + 10; }
        owned Source source = new Source();
        try { total = total + source.Value; } catch (MyError e) { total = total + 100; }
        return total;
    }
}
)ns", 13);
    expectRun(tests, root, "C13AsyncSuspendedError", std::string(kErrorPrelude) + R"ns(
class Counter { public static int Drops; }
class Guard
{
    public int Id;
    public Guard(int id) { Id = id; }
    ~Guard() { Counter.Drops = Counter.Drops * 10 + Id; }
}
class Program
{
    private static async int Slow(bool f) throws MyError { yield; if (f) { throw new MyError(); } return 5; }
    private static async int Outer()
    {
        int total = 0;
        try
        {
            raii Guard g = new Guard(7);
            total = total + await Slow(true);
        }
        catch (MyError e) { total = total + 10; }
        return total + 1;
    }
    public static async int Main() { int r = await Outer(); return r * 100 + Counter.Drops; }
}
)ns", 1107);
    expectRun(tests, root, "C13Accepted", std::string(kErrorPrelude) + R"ns(
class Thing
{
    public int V;
    public Thing(bool f) throws MyError { if (f) { throw new MyError(); } V = 1; }
    public Thing() throws MyError : this(true) { }
}
class Derived : Thing
{
    public Derived(bool f) throws MyError : base(f) { }
}
class Program
{
    private static int Fail(bool f) throws MyError { if (f) { throw new MyError(); } return 1; }
    private static int Propagate(bool f) throws MyError { return Fail(f); }
    private static int Widen(bool f) throws Exception { return Fail(f); }
    public static int Main()
    {
        int total = 0;
        try { total = total + Propagate(true); } catch (MyError e) { total = total + 10; }
        try { total = total + Widen(true); } catch (Exception e) { total = total + 100; }
        try { raii Thing t = new Thing(); } catch (MyError e) { total = total + 1000; }
        try { raii Derived d = new Derived(true); } catch (MyError e) { total = total + 10000; }
        try { total = total + Fail(false); } catch { total = total + 5; }
        return total;
    }
}
)ns", 11111);
    expectRun(tests, root, "C13Lambda", std::string(kErrorPrelude) + R"ns(
class Program
{
    public static int Fail(bool f) throws MyError { if (f) { throw new MyError(); } return 1; }
    private static int Run(Work w, bool f) throws MyError { return w(f); }
    public static int Main()
    {
        int total = 0;
        raii Work w = [] (bool f) => Program.Fail(f);
        try { total = total + Run(w, true); } catch (MyError e) { total = total + 10; }
        try { total = total + w(false); } catch (MyError e) { total = total + 100; }
        Work m = Fail;
        try { total = total + m(true); } catch (MyError e) { total = total + 1000; }
        return total;
    }
}
)ns", 1011);
}

constexpr std::string_view kBoxPrelude = R"ns(
using NativeSharp.Core;
class Counter { public static int Drops; }
class Box
{
    public int Id;
    public Box(int id) { Id = id; }
    ~Box() { Counter.Drops = Counter.Drops * 10 + Id; }
}
)ns";

// EN: C-14. A future that does not complete releases what its constructor stored: discarded,
//     out of scope, overwritten or handed to a parameter that drops it, the owned parameter is
//     destroyed once; an awaited future is consumed and destroys it at completion.
// ES: C-14. Un future que no completa libera lo que su constructor guardo: descartado, fuera de
//     scope, sobrescrito o entregado a un parametro que lo descarta, el parametro owned se destruye
//     una vez; un future esperado se consume y lo destruye al completar.
void testFutureDrops(TestContext& tests, const std::filesystem::path& root)
{
    expectRun(tests, root, "C14Discard", std::string(kBoxPrelude) + R"ns(
class Program
{
    private static async int Take(owned Box b) { return b.Id; }
    public static int Main() { Take(new Box(4)); return Counter.Drops; }
}
)ns", 4);
    expectRun(tests, root, "C14Scope", std::string(kBoxPrelude) + R"ns(
class Program
{
    private static async int Take(owned Box b) { return b.Id; }
    public static int Main()
    {
        {
            Future<int> f = Take(new Box(3));
        }
        return Counter.Drops;
    }
}
)ns", 3);
    expectRun(tests, root, "C14Overwrite", std::string(kBoxPrelude) + R"ns(
class Program
{
    private static async int Take(owned Box b) { return b.Id; }
    public static async int Main()
    {
        Future<int> f = Take(new Box(1));
        f = Take(new Box(2));
        int r = await f;
        return r * 100 + Counter.Drops;
    }
}
)ns", 212);
    expectRun(tests, root, "C14Await", std::string(kBoxPrelude) + R"ns(
class Program
{
    private static async int Take(owned Box b) { return b.Id; }
    public static int Main() { int r = await Take(new Box(5)); return r * 100 + Counter.Drops; }
}
)ns", 505);
    expectRun(tests, root, "C14MovedFuture", std::string(kBoxPrelude) + R"ns(
class Program
{
    private static async int Take(owned Box b) { return b.Id; }
    private static async int Pass(Future<int> f) { return await f; }
    public static async int Main()
    {
        Future<int> f = Take(new Box(8));
        Future<int> g = move(f);
        int r = await Pass(move(g));
        return r * 100 + Counter.Drops;
    }
}
)ns", 808);
    expectRun(tests, root, "C14UnusedFutureParameter", std::string(kBoxPrelude) + R"ns(
class Program
{
    private static async int Take(owned Box b) { return b.Id; }
    private static async int Pass(Future<int> f) { return 3; }
    public static async int Main()
    {
        int r = await Pass(Take(new Box(8)));
        return r * 100 + Counter.Drops;
    }
}
)ns", 308);
    expectRun(tests, root, "C14Hold", std::string(kBoxPrelude) + R"ns(
class Program
{
    private static async int Take(owned Box b) { return b.Id; }
    private static async int Tick() { yield; return 1; }
    public static async int Main()
    {
        Future<int> f = Take(new Box(9));
        Future<int> unused = Take(new Box(2));
        int t = await Tick();
        int r = await f;
        return (r + t) * 100 + Counter.Drops;
    }
}
)ns", 1009);
    expectRun(tests, root, "C14Spawn", std::string(kBoxPrelude) + R"ns(
struct InlineExecutor : IExecutor
{
    public int spawned;
    public InlineExecutor() { spawned = 0; }
    public int Spawn(Future<void> task) { spawned = spawned + 1; return spawned - 1; }
    public int PollJoin(int handle) { return 0; }
    public void Cancel(int handle) { }
}
class Program
{
    public static async void Child(owned Box b) { }
    public static int Main()
    {
        {
            TaskScope<InlineExecutor> scope = TaskScope<InlineExecutor>.Create(new InlineExecutor());
            scope.Spawn(Child(new Box(7)));
            scope.Join();
        }
        return Counter.Drops;
    }
}
)ns", 7);
}

void testFutureMoveOnly(TestContext& tests, const std::filesystem::path& root)
{
    expectReject(tests, root, "C14Copy", std::string(kBoxPrelude) + R"ns(
class Program
{
    private static async int Take(owned Box b) { return b.Id; }
    public static async int Main()
    {
        Future<int> f = Take(new Box(1));
        Future<int> g = f;
        return await g;
    }
}
)ns",
        "cannot be copied into local 'g'");
    expectReject(tests, root, "C14DoubleAwait", std::string(kBoxPrelude) + R"ns(
class Program
{
    private static async int Take(owned Box b) { return b.Id; }
    public static async int Main()
    {
        Future<int> f = Take(new Box(1));
        int a = await f;
        int b = await f;
        return a + b;
    }
}
)ns",
        "use of moved-from move-only local 'f'");
}

constexpr std::string_view kFutureFieldPrelude = R"ns(
class Holder { public Future<int> Pending; }
)ns";

// EN: Plan item 113. A `Future<T>` class field is stored with the layout of its future. The destruction of
//     the owner drops a future that did not complete, an assignment drops the one it replaces, and an await
//     consumes the field as it consumes a local, so the owner does not release the frame a second time.
//     Awaiting an empty field (never assigned or already consumed) traps as a null reference. A struct
//     field keeps its rejection.
// ES: Elemento 113 del plan. Un campo `Future<T>` de clase se guarda con el layout de su future. La
//     destrucción del dueño libera un future que no completó, una asignación libera el que reemplaza y un
//     await consume el campo como consume un local, así que el dueño no libera el frame otra vez. Esperar un
//     campo vacío (nunca asignado o ya consumido) atrapa como referencia nula. Un campo de struct conserva
//     su rechazo.
void testFutureFields(TestContext& tests, const std::filesystem::path& root)
{
    const std::string prelude = std::string(kBoxPrelude) + std::string(kFutureFieldPrelude);
    const std::string take = "    private static async int Take(owned Box b) { return b.Id; }\n";
    expectRun(tests, root, "F113FieldAwait", prelude + "class Program\n{\n" + take + R"ns(
    public static int Main()
    {
        int r = 0;
        {
            raii Holder h = new Holder();
            h.Pending = Take(new Box(7));
            r = await h.Pending;
        }
        return r * 100 + Counter.Drops;
    }
}
)ns", 707);
    expectRun(tests, root, "F113FieldDropped", prelude + "class Program\n{\n" + take + R"ns(
    public static int Main()
    {
        {
            raii Holder h = new Holder();
            h.Pending = Take(new Box(4));
        }
        return Counter.Drops;
    }
}
)ns", 4);
    expectRun(tests, root, "F113FieldOverwrite", prelude + "class Program\n{\n" + take + R"ns(
    public static int Main()
    {
        int r = 0;
        {
            raii Holder h = new Holder();
            h.Pending = Take(new Box(1));
            h.Pending = Take(new Box(2));
            r = await h.Pending;
        }
        return r * 100 + Counter.Drops;
    }
}
)ns", 212);
    expectRun(tests, root, "F113AsyncAwaitField", prelude + "class Program\n{\n" + take + R"ns(
    private static async int Outer(Holder h) { int v = await h.Pending; return v + 1; }
    public static int Main()
    {
        int r = 0;
        {
            raii Holder h = new Holder();
            h.Pending = Take(new Box(5));
            r = await Outer(h);
        }
        return r * 100 + Counter.Drops;
    }
}
)ns", 605);
    expectRun(tests, root, "F113EmptyField", std::string(kFutureFieldPrelude) + R"ns(
class Program
{
    public static int Main()
    {
        raii Holder h = new Holder();
        return await h.Pending;
    }
}
)ns", 135);
    expectReject(tests, root, "F113StructField", R"ns(
struct Slot { public Future<int> Pending; }
class Program
{
    public static int Main() { Slot s = new Slot(); return 0; }
}
)ns", "field type has no resolved named-type layout");
}

// EN: Plan item 114. The LLVM dump and verification actions lower a destructible struct as the emission does:
//     with its value drop glue, instead of failing with "value drop has no address/member model".
// ES: Elemento 114 del plan. Las acciones de volcado y verificación LLVM bajan un struct con destructor como lo
//     baja la emisión: con su drop de valor, en vez de fallar con «value drop has no address/member model».
void testValueDropVerification(TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path directory = root / "F114ValueDropVerify";
    const std::filesystem::path project = writeProject(directory, "F114ValueDropVerify");
    writeFile(directory / "Main.ns", R"ns(
using NativeSharp.Core;
struct Guard
{
    public int Id;
    ~Guard() { Console.Write(Id); }
}
class Program
{
    public static int Main() { Guard g = new Guard(); g.Id = 3; return 0; }
}
)ns");
    const std::string projectText = project.string();
    const std::array<std::string_view, 2> actions = {"--dump-project-oop-llvm", "--verify-project-oop-llvm-opt"};
    for (const std::string_view action : actions)
    {
        const std::array<std::string_view, 2> arguments = {action, projectText};
        Driver driver;
        std::ostringstream output;
        std::ostringstream error;
        const ExitCode code = driver.run(arguments, output, error);
        const std::string label = "F114ValueDropVerify " + std::string(action);
        tests.expect(code == ExitCode::Success, label + ": succeeds: " + output.str() + error.str());
        const std::string_view expected = action == "--dump-project-oop-llvm"
            ? "__nsc_drop_value_" : "LLVM optimization verification: ok";
        tests.expect(output.str().find(expected) != std::string::npos,
            label + ": prints " + std::string(expected));
    }
}

constexpr std::string_view kNestedAwaitPrelude = R"ns(
using NativeSharp.Core;
class Box
{
    public static int Freed;
    public int V;
    public Box(int v) { V = v; }
    ~Box() { Freed = Freed + 1; }
}
)ns";

// EN: Plan item 116. `await F(await G())` in a coroutine: when G suspends, the resume of G's await evaluates
//     the operand of the outer await again, so F is called with G's result. Operands evaluated before G
//     (a call, an owned argument) run once.
// ES: Elemento 116 del plan. `await F(await G())` en una corrutina: cuando G se suspende, la reanudación del
//     await de G vuelve a evaluar el operando del await exterior, así que F recibe el resultado de G. Los
//     operandos evaluados antes de G (una llamada, un argumento owned) corren una vez.
void testNestedAwaitResume(TestContext& tests, const std::filesystem::path& root)
{
    expectRun(tests, root, "F116NestedSuspend", std::string(kNestedAwaitPrelude) + R"ns(
class Program
{
    public static int Calls;
    public static async int G() { Calls = Calls + 1; yield; return 4; }
    public static async int F(int x) { Calls = Calls + 10; yield; return x * 10; }
    public static int Bump() { Calls = Calls + 100; return 0; }
    public static async int Main()
    {
        int r = Bump() + await F(await G());
        return r + Calls;
    }
}
)ns", 151);
    expectRun(tests, root, "F116NestedOwned", std::string(kNestedAwaitPrelude) + R"ns(
class Program
{
    public static async int G() { yield; return 4; }
    public static async int H(owned Box b, int x) { yield; return b.V * 10 + x; }
    public static async int Main()
    {
        int r = await H(new Box(2), await G());
        return r * 10 + Box.Freed;
    }
}
)ns", 241);
    expectRun(tests, root, "F116InnerOnlySuspends", std::string(kNestedAwaitPrelude) + R"ns(
class Program
{
    public static async int G() { yield; return 4; }
    public static async int F(int x) { return x * 10; }
    public static async int Main() { return await F(await G()); }
}
)ns", 40);
}

// EN: Plan item 117. A `loop` expression whose body suspends compiles in a coroutine: its result slot is in the
//     entry block, so the resume that enters the body reaches the `break` that stores the value.
// ES: Elemento 117 del plan. Una expresión `loop` cuyo cuerpo se suspende compila en una corrutina: su slot de
//     resultado está en el bloque de entrada, así que la reanudación que entra al cuerpo llega al `break` que
//     guarda el valor.
void testLoopExpressionAwait(TestContext& tests, const std::filesystem::path& root)
{
    expectRun(tests, root, "F117BreakAwait", std::string(kNestedAwaitPrelude) + R"ns(
class Program
{
    public static int Calls;
    public static async int G() { Calls = Calls + 1; yield; return 4; }
    public static async int Main()
    {
        int v = loop { break await G(); };
        return v * 10 + Calls;
    }
}
)ns", 41);
    expectRun(tests, root, "F117AwaitThenBreak", std::string(kNestedAwaitPrelude) + R"ns(
class Program
{
    public static int Calls;
    public static async int G() { Calls = Calls + 1; yield; return 4; }
    public static async int Main()
    {
        int v = loop { int t = await G(); break t; };
        return v * 10 + Calls;
    }
}
)ns", 41);
    expectRun(tests, root, "F117RaiiInBody", std::string(kNestedAwaitPrelude) + R"ns(
class Program
{
    public static async int F(int x) { yield; return x * 10; }
    public static async int Main()
    {
        int v = loop
        {
            raii Box b = new Box(5);
            break await F(b.V);
        };
        return v * 10 + Box.Freed;
    }
}
)ns", 501);
}

constexpr std::string_view kMatchReplayProgram = R"ns(
class Program
{
    public static int Calls;
    public static async int G() { Calls = Calls + 1; yield; return 4; }
    public static int Bump() { Calls = Calls + 100; return 0; }
    public static int Key() { Calls = Calls + 1000; return 1; }
    public static bool Check() { Calls = Calls + 10000; return true; }
)ns";

// EN: Plan item 117. The resume of an await inside a match arm replays the statement around the match without
//     running again what ran before the await: an operand before the match, the governing value and the guard
//     of the selected arm.
// ES: Elemento 117 del plan. La reanudación de un await dentro de un brazo de match repite la sentencia que lo
//     rodea sin volver a correr lo que corrió antes del await: un operando antes del match, el valor que
//     gobierna y el guard del brazo elegido.
void testMatchAwaitReplay(TestContext& tests, const std::filesystem::path& root)
{
    expectRun(tests, root, "F117MatchOperand", std::string(kMatchReplayProgram) + R"ns(
    public static async int Main()
    {
        int k = 1;
        int v = Bump() + match (k) { 1 => await G(), _ => 0 };
        return v * 1000 + Calls;
    }
}
)ns", 4101);
    expectRun(tests, root, "F117MatchGoverning", std::string(kMatchReplayProgram) + R"ns(
    public static async int Main()
    {
        int v = match (Key()) { 1 => await G(), _ => 0 };
        return v * 100000 + Calls;
    }
}
)ns", 401001);
    expectRun(tests, root, "F117MatchGuard", std::string(kMatchReplayProgram) + R"ns(
    public static async int Main()
    {
        int k = 1;
        int v = match (k) { 1 when Check() => await G(), _ => 0 };
        return v * 100000 + Calls;
    }
}
)ns", 410001);
}

constexpr std::string_view kLoopReplayProgram = R"ns(
class Box
{
    public static int Freed;
    public int V;
    public Box(int v) { V = v; }
    ~Box() { Freed = Freed + 1; }
}
class Program
{
    public static int Calls;
    public static async int G() { Calls = Calls + 1; yield; return 4; }
    public static int Bump() { Calls = Calls + 100; return 0; }
)ns";

// EN: Plan item 117. The resume of an await inside a loop expression replays the statement around the loop,
//     loading what ran before it, and the loop entry jumps to the statement of the body that suspended. An
//     operand before the loop runs once, also across iterations, with a raii local in the body and with an
//     array element as the destination; a yield in the body of a generator loop does the same.
// ES: Elemento 117 del plan. La reanudación de un await dentro de una expresión loop repite la sentencia que
//     rodea al loop, cargando lo que corrió antes, y la entrada del loop salta a la sentencia del cuerpo que se
//     suspendió. Un operando antes del loop corre una vez, también entre vueltas, con un local raii en el
//     cuerpo y con un elemento de arreglo como destino; un yield en el loop de un generador hace lo mismo.
void testLoopOperandResume(TestContext& tests, const std::filesystem::path& root)
{
    expectRun(tests, root, "F117LoopOperand", std::string(kLoopReplayProgram) + R"ns(
    public static async int Main()
    {
        int v = Bump() + loop { break await G(); };
        return v * 1000 + Calls;
    }
}
)ns", 4101);
    expectRun(tests, root, "F117LoopIterations", std::string(kLoopReplayProgram) + R"ns(
    public static async int Main()
    {
        int n = 0;
        int v = Bump() + loop { n = n + await G(); if (n >= 8) { break n; } };
        return v * 1000 + Calls;
    }
}
)ns", 8102);
    expectRun(tests, root, "F117LoopRaii", std::string(kLoopReplayProgram) + R"ns(
    public static async int Main()
    {
        int v = Bump() + loop { raii Box b = new Box(3); int t = await G(); break t + b.V; };
        return v * 10000 + Calls * 10 + Box.Freed;
    }
}
)ns", 71011);
    expectRun(tests, root, "F117LoopYield", R"ns(
class Program
{
    public static int Calls;
    public static int Bump() { Calls = Calls + 100; return 0; }
    private static Iterator<int, int> Gen()
    {
        int v = Bump() + loop { yield return 1; break 4; };
        yield return v;
    }
    public static int Main()
    {
        Iterator<int, int> it = Gen();
        int total = 0;
        while (it.MoveNext()) { total = total * 10 + it.Current; }
        return total * 1000 + Calls;
    }
}
)ns", 14100);
    expectRun(tests, root, "F117LoopElement", std::string(kLoopReplayProgram) + R"ns(
    public static async int Main()
    {
        int[] values = new int[2];
        int i = 1;
        values[i] = loop { break await G(); };
        return values[1] * 10 + Calls;
    }
}
)ns", 41);
}

constexpr std::string_view kParameterBoxPrelude = R"ns(
using NativeSharp.Core;
class Box
{
    public static int Freed;
    public int V;
    public Box(int v) { V = v; }
    ~Box() { Freed = Freed + 1; }
}
)ns";

/// EN: C-01, item 115. A `new` handed straight to a parameter that is neither owned nor borrowed
///     reaches no owner: static, instance, virtual and delegate calls, any argument position, a
///     constructor and a base-constructor initializer are rejected with LostOwnership. A
///     borrowed parameter keeps its own rejection (it needs an existing owner).
/// ES: C-01, elemento 115. Un `new` entregado directo a un parametro que no es owned ni borrowed
///     no llega a ningun dueno: llamadas estaticas, de instancia, virtuales y de delegate, en
///     cualquier posicion, un constructor y un inicializador de constructor base se rechazan con
///     LostOwnership. Un parametro borrowed conserva su propio rechazo (necesita un dueno).
void testNewToOrdinaryParameterRejected(TestContext& tests, const std::filesystem::path& root)
{
    const std::string box(kParameterBoxPrelude);
    const std::string lost = "reaches no owner through the parameter 'b' of";
    expectReject(tests, root, "F115Static", box + "class Program\n{\n"
        "    public static int Use(Box b) { return b.V; }\n"
        "    public static int Main() { return Use(new Box(3)); }\n}\n", lost);
    expectReject(tests, root, "F115Escape", box + "class Program\n{\n"
        "    public static Box Kept;\n"
        "    public static int Keep(Box b) { Kept = b; return b.V; }\n"
        "    public static int Main() { int v = Keep(new Box(3)); return Kept.V + v; }\n}\n", lost);
    expectReject(tests, root, "F115SecondArgument", box + "class Program\n{\n"
        "    public static int Use(int a, Box b) { return a + b.V; }\n"
        "    public static int Main() { return Use(1, new Box(3)); }\n}\n", lost);
    expectReject(tests, root, "F115Instance", box + "class Sink { public int Use(Box b) { return b.V; } }\n"
        "class Program\n{\n"
        "    public static int Main() { raii Sink s = new Sink(); return s.Use(new Box(3)); }\n}\n", lost);
    expectReject(tests, root, "F115Virtual", box +
        "class Sink { public virtual int Use(Box b) { return b.V; } }\nclass Program\n{\n"
        "    public static int Main() { raii Sink s = new Sink(); return s.Use(new Box(3)); }\n}\n", lost);
    expectReject(tests, root, "F115Delegate", box + "delegate int Work(Box b);\nclass Program\n{\n"
        "    public static int Use(Box b) { return b.V; }\n"
        "    public static int Main() { Work w = Use; return w(new Box(3)); }\n}\n",
        "reaches no owner through the parameter 'b' of delegate 'Work'");
    expectReject(tests, root, "F115Constructor", box +
        "class Node { public Box Item; public Node(Box item) { Item = item; } }\nclass Program\n{\n"
        "    public static int Main() { raii Node n = new Node(new Box(3)); return n.Item.V; }\n}\n",
        "reaches no owner through the parameter 'item' of 'Node'");
    expectReject(tests, root, "F115BaseInitializer", box +
        "class Base { public int V; public Base(Box b) { V = b.V; } }\n"
        "class Derived : Base { public Derived() : base(new Box(3)) { } }\nclass Program\n{\n"
        "    public static int Main() { raii Derived d = new Derived(); return d.V; }\n}\n", lost);
    expectReject(tests, root, "F115Borrowed", box + "class Program\n{\n"
        "    public static int Use(borrowed Box b) { return b.V; }\n"
        "    public static int Main() { return Use(new Box(3)); }\n}\n",
        "borrowed parameter 'b' requires a checked owner or borrowed source");
}

/// EN: C-01, item 115. The owning alternatives compile and destroy the object exactly once: an
///     owned parameter (static and delegate), a constructor and a base initializer that move an
///     owned parameter into an owned field, and a raii local lent to an ordinary or borrowed
///     parameter. Each program returns V * 10 + Freed = 31.
/// ES: C-01, elemento 115. Las alternativas con dueno compilan y destruyen el objeto exactamente
///     una vez: un parametro owned (estatico y de delegate), un constructor y un inicializador base
///     que mueven un parametro owned a un campo owned, y un local raii prestado a un parametro
///     ordinario o borrowed. Cada programa devuelve V * 10 + Freed = 31.
void testNewToOwnedParameterAccepted(TestContext& tests, const std::filesystem::path& root)
{
    const std::string box(kParameterBoxPrelude);
    expectRun(tests, root, "F115OwnedParameter", box + "class Program\n{\n"
        "    public static int Take(owned Box b) { return b.V; }\n"
        "    public static int Main() { int v = Take(new Box(3)); return v * 10 + Box.Freed; }\n}\n", 31);
    expectRun(tests, root, "F115OwnedDelegate", box + "delegate int Take(owned Box b);\nclass Program\n{\n"
        "    public static int T(owned Box b) { return b.V; }\n"
        "    public static int Main() { Take t = T; int v = t(new Box(3)); return v * 10 + Box.Freed; }\n}\n",
        31);
    expectRun(tests, root, "F115OwnedField", box +
        "class Node { public owned Box Item; public Node(owned Box item) { Item = move(item); } }\n"
        "class Program\n{\n    public static int Main()\n    {\n        int v = 0;\n"
        "        { raii Node n = new Node(new Box(3)); v = n.Item.V; }\n"
        "        return v * 10 + Box.Freed;\n    }\n}\n", 31);
    expectRun(tests, root, "F115OwnedBaseField", box +
        "class Base { public owned Box Item; public Base(owned Box b) { Item = move(b); } }\n"
        "class Derived : Base { public Derived() : base(new Box(3)) { } }\n"
        "class Program\n{\n    public static int Main()\n    {\n        int v = 0;\n"
        "        { raii Derived d = new Derived(); v = d.Item.V; }\n"
        "        return v * 10 + Box.Freed;\n    }\n}\n", 31);
    expectRun(tests, root, "F115LentOrdinary", box + "class Program\n{\n"
        "    public static int Use(Box b) { return b.V; }\n"
        "    public static int Main()\n    {\n        int v = 0;\n"
        "        { raii Box b = new Box(3); v = Use(b); }\n"
        "        return v * 10 + Box.Freed;\n    }\n}\n", 31);
    expectRun(tests, root, "F115LentBorrowed", box + "class Program\n{\n"
        "    public static int Use(borrowed Box b) { return b.V; }\n"
        "    public static int Main()\n    {\n        int v = 0;\n"
        "        { raii Box b = new Box(3); v = Use(b); }\n"
        "        return v * 10 + Box.Freed;\n    }\n}\n", 31);
}

/// EN: C-01, item 115a. `return new T()` from a callable whose return type is not owned reaches no owner and
///     is rejected; the owned return hands the object to a raii local that destroys it once (V * 10 + Freed).
/// ES: C-01, elemento 115a. `return new T()` desde un callable cuyo retorno no es owned no llega a ningun
///     dueno y se rechaza; el retorno owned entrega el objeto a un local raii que lo destruye una vez.
void testNewToNonOwnedReturn(TestContext& tests, const std::filesystem::path& root)
{
    const std::string box(kParameterBoxPrelude);
    expectReject(tests, root, "F115aReturn", box + "class Program\n{\n"
        "    public static Box Make() { return new Box(3); }\n"
        "    public static int Main() { return 0; }\n}\n",
        "reaches no owner through the return of 'Make', whose return type is not owned");
    expectRun(tests, root, "F115aOwnedReturn", box + "class Program\n{\n"
        "    public static owned Box Make() { return new Box(3); }\n"
        "    public static int Main()\n    {\n        int v = 0;\n"
        "        { raii Box b = Make(); v = b.V; }\n"
        "        return v * 10 + Box.Freed;\n    }\n}\n", 31);
}

/// EN: C-01, item 115b. `new T()` as the initializer of an instance field that is not owned reaches no owner
///     and is rejected; an owned field destroys the object exactly once with its container.
/// ES: C-01, elemento 115b. `new T()` como inicializador de un campo de instancia que no es owned no llega a
///     ningun dueno y se rechaza; un campo owned destruye el objeto exactamente una vez con su contenedor.
void testNewToNonOwnedField(TestContext& tests, const std::filesystem::path& root)
{
    const std::string box(kParameterBoxPrelude);
    expectReject(tests, root, "F115bField", box + "class Holder { public Box Item = new Box(3); }\n"
        "class Program\n{\n"
        "    public static int Main() { raii Holder h = new Holder(); return h.Item.V; }\n}\n",
        "reaches no owner through the field 'Item', which is not owned");
    expectRun(tests, root, "F115bOwnedField", box + "class Holder { public owned Box Item = new Box(3); }\n"
        "class Program\n{\n    public static int Main()\n    {\n        int v = 0;\n"
        "        { raii Holder h = new Holder(); v = h.Item.V; }\n"
        "        return v * 10 + Box.Freed;\n    }\n}\n", 31);
}

constexpr std::string_view kTransferPrelude = R"ns(
using NativeSharp.Core;
class Boom : Exception
{
    public Boom(string message) : base(message) { }
}
class Box
{
    public static int Made;
    public static int Freed;
    public int V;
    public Box(int v) { V = v; Made = Made + 1; }
    ~Box() { Freed = Freed + 1; }
}
)ns";

constexpr std::string_view kTransferProgram = R"ns(class Program
{
    public static int Fail() throws Boom { throw new Boom("x"); }
    public static int Take(owned Box b, int x) { return b.V + x; }
    public static int Take2(owned Box a, owned Box b, int x) { return x; }
    public static owned Box Make() { return new Box(1); }
)ns";

/// EN: Item 118. An object handed to an `owned` parameter is destroyed once when a later argument throws
///     before the call (a `new`, a `move`, an owned return; static call, constructor, base initializer,
///     delegate, two owned arguments, and after an await in a coroutine), and the callee still owns it when
///     the call runs. Each program returns Made * 10 + Freed.
/// ES: Elemento 118. Un objeto entregado a un parametro `owned` se destruye una vez cuando un argumento
///     posterior lanza antes de la llamada (un `new`, un `move`, un retorno owned; llamada estatica,
///     constructor, inicializador base, delegate, dos argumentos owned y despues de un await en una
///     corrutina), y el llamado sigue siendo el dueno cuando la llamada corre. Cada programa devuelve
///     Made * 10 + Freed.
void testOwnedArgumentTransfer(TestContext& tests, const std::filesystem::path& root)
{
    const std::string program = std::string(kTransferPrelude) + std::string(kTransferProgram);
    const std::string caught = "        catch (Boom b) { }\n        return Box.Made * 10 + Box.Freed;\n    }\n}\n";
    const std::string opening = "    public static int Main()\n    {\n        try { ";
    expectRun(tests, root, "F118New", program + opening + "Take(new Box(1), Fail()); }\n" + caught, 11);
    expectRun(tests, root, "F118Move", program + opening +
        "raii Box a = new Box(1); Take(move(a), Fail()); }\n" + caught, 11);
    expectRun(tests, root, "F118Return", program + opening + "Take(Make(), Fail()); }\n" + caught, 11);
    expectRun(tests, root, "F118TwoOwned", program + opening + "Take2(new Box(1), new Box(2), Fail()); }\n" +
        caught, 22);
    expectRun(tests, root, "F118Delegate", std::string(kTransferPrelude) +
        "delegate int Work(owned Box b, int x);\n" + std::string(kTransferProgram) + opening +
        "Work w = Take; w(new Box(1), Fail()); }\n" + caught, 11);
    expectRun(tests, root, "F118Constructor", std::string(kTransferPrelude) +
        "class Holder { public owned Box Item; public Holder(owned Box b, int x) { Item = move(b); } }\n" +
        std::string(kTransferProgram) + opening + "raii Holder h = new Holder(new Box(1), Fail()); }\n" + caught, 11);
    expectRun(tests, root, "F118Base", std::string(kTransferPrelude) +
        "class Base { public owned Box Item; public Base(owned Box b, int x) { Item = move(b); } }\n"
        "class Derived : Base { public Derived() throws Boom : base(new Box(1), Program.Fail()) { } }\n" +
        std::string(kTransferProgram) + opening + "raii Derived d = new Derived(); }\n" + caught, 11);
    expectRun(tests, root, "F118AfterAwait", program +
        "    public static async int G() { yield; return 4; }\n"
        "    public static int FailWith(int x) throws Boom { throw new Boom(\"x\"); }\n"
        "    public static async int Main()\n    {\n        try { int r = Take(new Box(1), FailWith(await G())); }\n" +
        caught, 11);
    expectRun(tests, root, "F118Completes", program +
        "    public static async int G() { yield; return 4; }\n"
        "    public static async int Main()\n    {\n        int r = Take(new Box(1), await G());\n"
        "        return r * 100 + Box.Made * 10 + Box.Freed;\n    }\n}\n", 511);
}

/// EN: Item 121. `delete p` of an owned class parameter consumes it: the object is destroyed once, also
///     when the delete runs on one branch only and through Dictionary.TryAdd / SortedList.TryAdd, which
///     delete the value they reject. Reading or deleting the parameter afterwards is rejected. Each
///     program returns Made * 10 + Freed.
/// ES: Elemento 121. `delete p` de un parametro owned de clase lo consume: el objeto se destruye una vez,
///     tambien cuando el delete corre en una sola rama y a traves de Dictionary.TryAdd /
///     SortedList.TryAdd, que borran el valor que rechazan. Leer o borrar el parametro despues se
///     rechaza. Cada programa devuelve Made * 10 + Freed.
void testDeleteOwnedParameter(TestContext& tests, const std::filesystem::path& root)
{
    const std::string prelude(kTransferPrelude);
    expectRun(tests, root, "F121Delete", prelude + "class Program\n{\n"
        "    private static void Eat(owned Box b) { delete b; }\n"
        "    private static bool EatIf(owned Box b, bool now) { if (now) { delete b; return false; } return true; }\n"
        "    public static int Main()\n    {\n"
        "        Eat(new Box(1));\n        EatIf(new Box(2), true);\n        EatIf(new Box(3), false);\n"
        "        return Box.Made * 10 + Box.Freed;\n    }\n}\n", 33);
    expectReject(tests, root, "F121DeleteThenRead", prelude + "class Program\n{\n"
        "    private static int Eat(owned Box b) { delete b; return b.V; }\n"
        "    public static int Main() { return Eat(new Box(1)); }\n}\n", "'b'");
    expectReject(tests, root, "F121DeleteTwice", prelude + "class Program\n{\n"
        "    private static void Eat(owned Box b) { delete b; delete b; }\n"
        "    public static int Main() { Eat(new Box(1)); return 0; }\n}\n", "'b'");
    const std::string collections = "using NativeSharp.Core.Collections.Generic;\n" + prelude;
    expectRun(tests, root, "F121DictionaryTryAdd", collections + "class Program\n{\n"
        "    public static int Main()\n    {\n        int added = 0;\n        {\n"
        "            raii Dictionary<int, Box> d = new Dictionary<int, Box>();\n"
        "            d.AddOrUpdate(1, new Box(1));\n            if (d.TryAdd(1, new Box(2))) added = 1;\n"
        "        }\n        return added * 100 + Box.Made * 10 + Box.Freed;\n    }\n}\n", 22, true);
    expectRun(tests, root, "F121SortedListTryAdd", collections + "class Program\n{\n"
        "    public static int Main()\n    {\n        int added = 0;\n        {\n"
        "            raii SortedList<int, Box> s = new SortedList<int, Box>();\n"
        "            s.AddOrUpdate(1, new Box(1));\n            if (s.TryAdd(1, new Box(2))) added = 1;\n"
        "        }\n        return added * 100 + Box.Made * 10 + Box.Freed;\n    }\n}\n", 22, true);
}

/// EN: Item 130 (F2). A local `T[]` declared without owned/raii stores references it does not own:
///     `delete[]` releases only the block, so the object TryGetValue or TryPeek copied into it stays alive
///     in its collection. An `owned T[]` local still destroys what it received by transfer (DEC-06), and
///     a plain array of structs with a destructor still drops its live slots (DEC-11). Each program
///     returns Made * 10 + Freed (and the value read afterwards, where there is one).
/// ES: Elemento 130 (F2). Un local `T[]` declarado sin owned/raii guarda referencias que no posee:
///     `delete[]` solo libera el bloque, asi que el objeto que TryGetValue o TryPeek copio en el sigue vivo
///     en su coleccion. Un local `owned T[]` sigue destruyendo lo que recibio por transferencia (DEC-06), y
///     un arreglo plano de structs con destructor sigue destruyendo sus slots vivos (DEC-11). Cada programa
///     devuelve Made * 10 + Freed (y el valor leido despues, donde lo hay).
void testPlainArrayDelete(TestContext& tests, const std::filesystem::path& root)
{
    const std::string collections = "using NativeSharp.Core.Collections.Generic;\n" + std::string(kTransferPrelude);
    const std::string tail = "        }\n        return v * 100 + Box.Made * 10 + Box.Freed;\n    }\n}\n";
    expectRun(tests, root, "F130TryGetValueArray", collections + "class Program\n{\n"
        "    public static int Main()\n    {\n        int v = 0;\n        {\n"
        "            raii Dictionary<int, Box> d = new Dictionary<int, Box>();\n"
        "            d.AddOrUpdate(1, new Box(4));\n"
        "            Box[] r = new Box[1];\n            if (d.TryGetValue(1, r)) delete[] r;\n"
        "            v = d[1].V;\n" + tail, 411, true);
    expectRun(tests, root, "F130TryPeekArray", collections + "class Program\n{\n"
        "    public static int Main()\n    {\n        int v = 0;\n        {\n"
        "            raii PriorityQueue<Box, int> q = new PriorityQueue<Box, int>();\n"
        "            q.Enqueue(new Box(5), 1);\n            Box[] r = new Box[1];\n            int[] p = new int[1];\n"
        "            if (q.TryPeek(r, p)) delete[] r;\n            delete[] p;\n"
        "            try { v = q.Peek().V; } catch (InvalidOperationException e) { v = 9; }\n" + tail, 511, true);
    expectRun(tests, root, "F130OwnedArrayDestroys", std::string(kTransferPrelude) + "class Program\n{\n"
        "    public static int Main()\n    {\n        int v = 0;\n        {\n"
        "            owned Box[] r = new Box[1];\n            raii Box b = new Box(6);\n"
        "            r[0] = move(b);\n            v = r[0].V;\n            delete[] r;\n" + tail, 611);
    expectRun(tests, root, "F130StructArrayDrops", std::string(kTransferPrelude) +
        "struct Guard\n{\n    public int Id;\n    ~Guard() { Box.Freed = Box.Freed + 1; }\n}\n"
        "class Program\n{\n    public static int Main()\n    {\n        int v = 0;\n        {\n"
        "            Guard[] a = new Guard[2];\n            Guard g;\n            g.Id = 7;\n"
        "            Box.Made = Box.Made + 1;\n            a[0] = move(g);\n            v = a[0].Id;\n"
        "            delete[] a;\n" + tail, 711);
}

constexpr std::string_view kGenericMovePrelude = R"ns(
using NativeSharp.Core;
using NativeSharp.Core.Collections.Generic;
class Box
{
    public static int Made;
    public static int Freed;
    public int V;
    public Box(int v) { V = v; Made = Made + 1; }
    ~Box() { Freed = Freed + 1; }
}
struct Guard
{
    public int Id;
    ~Guard() { Box.Freed = Box.Freed + 1; }
    public static bool operator <(in Guard a, in Guard b) { return a.Id < b.Id; }
    public static bool operator >(in Guard a, in Guard b) { return a.Id > b.Id; }
    public static bool operator ==(in Guard a, in Guard b) { return a.Id == b.Id; }
    public static bool operator !=(in Guard a, in Guard b) { return a.Id != b.Id; }
}
class Make { public static Guard G(int id) { Box.Made = Box.Made + 1; Guard g; g.Id = id; return move(g); } }
)ns";

/// EN: Item 130 (F3). In a closed generic type, move(p) of a parameter declared with a class type argument
///     copies the reference: SortedSet<Box>.Contains/Remove compile and find the element without transferring
///     it, while SortedSet<Guard> (move-only) still consumes the probe. Handing that copy to an owned
///     parameter, or moving a parameter whose class type the template names, stays rejected. Each program
///     returns flags * 100 + Made * 10 + Freed.
/// ES: Elemento 130 (F3). En un tipo generico cerrado, move(p) de un parametro declarado con un argumento de
///     tipo clase copia la referencia: SortedSet<Box>.Contains/Remove compilan y encuentran el elemento sin
///     transferirlo, mientras SortedSet<Guard> (move-only) sigue consumiendo la sonda. Entregar esa copia a un
///     parametro owned, o mover un parametro cuyo tipo clase escribe la plantilla, sigue rechazado. Cada
///     programa devuelve banderas * 100 + Made * 10 + Freed.
void testGenericMoveReferenceCopy(TestContext& tests, const std::filesystem::path& root)
{
    const std::string prelude(kGenericMovePrelude);
    const std::string tail = "        }\n        return r * 100 + Box.Made * 10 + Box.Freed;\n    }\n}\n";
    const std::string opening =
        "class Program\n{\n    public static int Main()\n    {\n        int r = 0;\n        {\n";
    expectRun(tests, root, "F130SortedSetClass", prelude + opening +
        "            InComparison<Box> byValue = (in Box a, in Box b) => a.V - b.V;\n"
        "            raii SortedSet<Box> s = new SortedSet<Box>(byValue);\n"
        "            s.Add(new Box(1));\n            s.Add(new Box(2));\n            raii Box probe = new Box(2);\n"
        "            if (s.Contains(probe)) r = r + 10;\n            if (s.Remove(probe)) r = r + 1;\n"
        "            r = r * 10 + s.Count();\n" + tail, 11133, true);
    expectRun(tests, root, "F130SortedSetStruct", prelude + opening +
        "            raii SortedSet<Guard> s = new SortedSet<Guard>();\n"
        "            s.Add(Make.G(1));\n            s.Add(Make.G(2));\n"
        "            if (s.Contains(Make.G(2))) r = r + 10;\n            if (s.Remove(Make.G(2))) r = r + 1;\n"
        "            r = r * 10 + s.Count();\n" + tail, 11144, true);
    expectReject(tests, root, "F130GenericMoveToOwned", prelude +
        "class Shelf<T>\n{\n    public void Keep(T value) { Take(move(value)); }\n"
        "    private void Take(owned T item) { }\n}\n" + opening +
        "            raii Shelf<Box> shelf = new Shelf<Box>();\n            raii Box b = new Box(1);\n"
        "            shelf.Keep(b);\n" + tail, "owned parameter", true);
    expectReject(tests, root, "F130TemplateClassMove", prelude +
        "class Wrap<T>\n{\n    public int Use(Box b) { return Peek(move(b)); }\n"
        "    private int Peek(Box x) { return x.V; }\n}\n" + opening +
        "            raii Wrap<int> w = new Wrap<int>();\n            raii Box b = new Box(1);\n"
        "            r = w.Use(b);\n" + tail, "move(...) source must be a RAII local or owned parameter", true);
}

constexpr std::string_view kImportPrelude = R"ns(
using NativeSharp.Core;
class Box { public int V; public Box(int v) { V = v; } }
struct Code { public int V; }
struct Guard
{
    public int V;
    ~Guard() { }
}
struct Holder
{
    public int V;
    public owned Box B;
}
class Native
{
    extern C from "kernel32.dll"
    {
        public static void SetLastError(Code code);
        public static int GetLastError();
    }
}
class NativePointer
{
    extern C from "kernel32.dll"
    {
        public static int lstrlenA(Guard* text);
    }
}
class NativeGuard
{
    extern C from "kernel32.dll"
    {
        public static void Sleep(Guard milliseconds);
    }
}
class NativeHolder
{
    extern C from "kernel32.dll"
    {
        public static int SetErrorMode(Holder mode);
    }
}
)ns";

/// EN: Item 130b. A by-value parameter of an `extern C` import must be ABI-trivial. A trivial struct and a
///     pointer to a struct with a destructor still pass; a struct with a destructor or with an owned field
///     passed by value is rejected at the call, because native code cannot run its destructor or release
///     its owned fields.
/// ES: Elemento 130b. Un parametro por valor de un import `extern C` debe ser ABI-trivial. Un struct trivial y
///     un puntero a un struct con destructor siguen pasando; un struct con destructor o con un campo owned
///     pasado por valor se rechaza en la llamada, porque el codigo nativo no puede correr su destructor ni
///     liberar sus campos owned.
void testImportedCArgumentTrivial(TestContext& tests, const std::filesystem::path& root)
{
    const std::string prelude(kImportPrelude);
    const std::string opening = "class Program\n{\n    public static int Main()\n    {\n";
    expectRun(tests, root, "F130bTrivialStruct", prelude + opening +
        "        Code c;\n        c.V = 7;\n        Native.SetLastError(c);\n"
        "        return Native.GetLastError();\n    }\n}\n", 7);
    expectRun(tests, root, "F130bPointerToGuard", prelude + opening +
        "        Guard g;\n        g.V = 3;\n        int length = NativePointer.lstrlenA(&g);\n"
        "        return length * 10 + g.V;\n    }\n}\n", 13);
    expectReject(tests, root, "F130bGuardByValue", prelude + opening +
        "        Guard g;\n        g.V = 3;\n        NativeGuard.Sleep(move(g));\n"
        "        return 0;\n    }\n}\n", "native code cannot run its destructor");
    expectReject(tests, root, "F130bOwnedFieldByValue", prelude + opening +
        "        Holder h;\n        h.V = 1;\n        h.B = new Box(1);\n        NativeHolder.SetErrorMode(move(h));\n"
        "        return 0;\n    }\n}\n", "release its owned fields");
}

constexpr std::string_view kPrintPrelude = R"ns(
using NativeSharp.Core;
class Box
{
    public int Id;
    public Box(int id) { Id = id; }
    ~Box() { Console.Write("d"); Console.Write(Id); }
}
struct Guard
{
    public int Id;
    ~Guard() { Console.Write("g"); Console.Write(Id); }
}
)ns";

// EN: C-16. A generator destroys exactly the locals initialized when it is abandoned: a branch not
//     taken, a value moved out or a finished loop iteration add nothing. With the unwind-native
//     profile a panic in a generator, an async function or an async generator runs each active
//     cleanup once.
// ES: C-16. Un generador destruye exactamente los locales inicializados cuando se abandona: una
//     rama no tomada, un valor movido o una vuelta de ciclo terminada no agregan nada. Con el perfil
//     unwind-native un panic en un generador, una funcion async o un generador async corre cada
//     cleanup activo una vez.
void testGeneratorCleanup(TestContext& tests, const std::filesystem::path& root)
{
    expectRunOutput(tests, root, "C16Cond", std::string(kPrintPrelude) + R"ns(
class Program
{
    private static Iterator<int> Gen(bool make)
    {
        raii Box a = new Box(1);
        yield return 1;
        if (make)
        {
            raii Box b = new Box(2);
            yield return 2;
        }
        raii Box c = new Box(3);
        yield return 3;
        yield return 4;
    }
    public static int Main()
    {
        int total = 0;
        foreach (int x in Gen(false)) { total = total + x; if (x == 3) break; }
        Console.Write("|");
        foreach (int x in Gen(true)) { total = total + x; if (x == 2) break; }
        Console.Write("|");
        foreach (int x in Gen(true)) { total = total + x; if (x == 3) break; }
        return total;
    }
}
)ns",
        13, "d3d1|d2d1|d2d3d1", false);
    expectRunOutput(tests, root, "C16Shapes", std::string(kPrintPrelude) + R"ns(
class Program
{
    private static void Sink(owned Box b) { Console.Write("s"); }
    private static void Keep(Guard g) { Console.Write("k"); }
    private static Iterator<int> Loop()
    {
        int i = 0;
        while (i < 3)
        {
            raii Box b = new Box(i + 1);
            yield return i;
            i = i + 1;
        }
    }
    private static Iterator<int> Moved(bool flag)
    {
        raii Box a = new Box(5);
        if (flag) { Sink(move(a)); }
        yield return 1;
        yield return 2;
    }
    private static Iterator<int> Value(bool flag)
    {
        Guard g;
        g.Id = 7;
        if (flag) { Keep(move(g)); }
        yield return 1;
        yield return 2;
    }
    public static int Main()
    {
        foreach (int x in Loop()) { if (x == 1) break; }
        Console.Write("|");
        foreach (int x in Moved(true)) { break; }
        Console.Write("|");
        foreach (int x in Moved(false)) { break; }
        Console.Write("|");
        foreach (int x in Value(true)) { break; }
        Console.Write("|");
        foreach (int x in Value(false)) { break; }
        return 0;
    }
}
)ns",
        0, "d1d2|sd5|d5|kg7|g7", false);
    expectRunOutput(tests, root, "C16Panic", std::string(kPrintPrelude) + R"ns(
class Program
{
    private static Iterator<int> Gen()
    {
        raii Box a = new Box(1);
        yield return 1;
        raii Box b = new Box(2);
        panic("stop");
        yield return 2;
    }
    public static int Main()
    {
        int total = 0;
        foreach (int x in Gen()) { total = total + x; }
        return total;
    }
}
)ns",
        142, "d2d1", true);
    expectRunOutput(tests, root, "C16PanicCond", std::string(kPrintPrelude) + R"ns(
class Program
{
    private static Iterator<int> Gen(bool make)
    {
        raii Box a = new Box(1);
        yield return 1;
        if (make)
        {
            raii Box b = new Box(2);
            yield return 2;
        }
        Guard g;
        g.Id = 3;
        panic("stop");
        yield return 3;
    }
    public static int Main()
    {
        foreach (int x in Gen(false)) { }
        return 0;
    }
}
)ns",
        142, "g3d1", true);
    expectRunOutput(tests, root, "C16PanicAsync", std::string(kPrintPrelude) + R"ns(
class Program
{
    private static async int Work()
    {
        raii Box a = new Box(1);
        yield;
        raii Box b = new Box(2);
        panic("stop");
        return 1;
    }
    public static async int Main() { return await Work(); }
}
)ns",
        142, "d2d1", true);
    expectRunOutput(tests, root, "C16AsyncGenPanic", std::string(kPrintPrelude) + R"ns(
class Program
{
    private static AsyncIterator<int, int> Gen()
    {
        raii Box a = new Box(1);
        yield return 1;
        raii Box b = new Box(2);
        panic("stop");
        yield return 2;
    }
    public static int Main()
    {
        AsyncIterator<int, int> it = Gen();
        bool h1 = await it.MoveNextAsync();
        bool h2 = await it.MoveNextAsync();
        return 0;
    }
}
)ns",
        142, "d2d1", true);
}

constexpr std::string_view kAsyncErrorPrelude = R"ns(
using NativeSharp.Core;
class MyError : Exception { public int Code; public MyError(int c) { Code = c; } }
class Counter { public static int Log; }
class Guard
{
    public int Id;
    public Guard(int id) { Id = id; }
    ~Guard() { Counter.Log = Counter.Log * 10 + Id; }
}
)ns";

// EN: C-17. An error thrown in an async generator reaches the future of `MoveNextAsync` whole: a
//     catch around the await receives the same object, from a synchronous or an async caller,
//     before the first yield or after a resume; the generator runs its active finally blocks and
//     destructions once and then stays finished. Without a catch the error propagates and the
//     consumer's own live locals are destroyed too.
// ES: C-17. Un error lanzado en un generador async llega completo al future de `MoveNextAsync`:
//     un catch alrededor del await recibe el mismo objeto, desde un llamador sincrono o async,
//     antes del primer yield o tras reanudar; el generador corre una vez sus finally y
//     destrucciones activas y luego queda terminado. Sin catch el error se propaga y tambien se
//     destruyen los locales vivos del consumidor.
void testAsyncGeneratorErrors(TestContext& tests, const std::filesystem::path& root)
{
    expectRun(tests, root, "C17Catch", std::string(kAsyncErrorPrelude) + R"ns(
class Program
{
    private static AsyncIterator<int, MyError> Gen(int n) throws MyError
    {
        raii Guard a = new Guard(1);
        yield return n;
        raii Guard b = new Guard(2);
        if (n > 0) { throw new MyError(37); }
        yield return 9;
    }
    public static int Main()
    {
        int r = 0;
        AsyncIterator<int, MyError> it = Gen(4);
        try
        {
            bool h1 = await it.MoveNextAsync();
            r = it.Current;
            bool h2 = await it.MoveNextAsync();
            r = r + 1000;
        }
        catch (MyError e) { r = r * 100 + e.Code; }
        return r * 100 + Counter.Log;
    }
}
)ns", 43721);
    expectRun(tests, root, "C17First", std::string(kAsyncErrorPrelude) + R"ns(
class Program
{
    private static AsyncIterator<int, MyError> Gen(int n) throws MyError
    {
        raii Guard a = new Guard(3);
        if (n > 0) { throw new MyError(n); }
        yield return n;
    }
    public static int Main()
    {
        int r = 1;
        AsyncIterator<int, MyError> it = Gen(5);
        try { bool h1 = await it.MoveNextAsync(); r = 99; }
        catch (MyError e) { r = e.Code; }
        return r * 10 + Counter.Log;
    }
}
)ns", 53);
    expectRun(tests, root, "C17After", std::string(kAsyncErrorPrelude) + R"ns(
class Program
{
    private static AsyncIterator<int, MyError> Gen() throws MyError
    {
        raii Guard a = new Guard(1);
        yield return 1;
        throw new MyError(2);
    }
    public static int Main()
    {
        int r = 0;
        AsyncIterator<int, MyError> it = Gen();
        try { bool h1 = await it.MoveNextAsync(); bool h2 = await it.MoveNextAsync(); r = 50; }
        catch (MyError e) { r = e.Code; }
        bool h3 = false;
        try { h3 = await it.MoveNextAsync(); }
        catch (MyError e2) { r = r + 500; }
        return r * 1000 + (h3 ? 100 : 0) + Counter.Log;
    }
}
)ns", 2001);
    expectRun(tests, root, "C17Finally", std::string(kAsyncErrorPrelude) + R"ns(
class Program
{
    private static AsyncIterator<int, MyError> Gen() throws MyError
    {
        raii Guard a = new Guard(1);
        try
        {
            yield return 1;
            raii Guard b = new Guard(2);
            throw new MyError(6);
        }
        finally { Counter.Log = Counter.Log * 10 + 8; }
    }
    public static int Main()
    {
        int r = 0;
        AsyncIterator<int, MyError> it = Gen();
        try { bool h1 = await it.MoveNextAsync(); bool h2 = await it.MoveNextAsync(); r = 50; }
        catch (MyError e) { r = e.Code; }
        return r * 1000 + Counter.Log;
    }
}
)ns", 6281);
    expectRun(tests, root, "C17Chain", std::string(kAsyncErrorPrelude) + R"ns(
class Program
{
    private static AsyncIterator<int, MyError> Gen() throws MyError
    {
        raii Guard a = new Guard(1);
        yield return 4;
        yield;
        throw new MyError(9);
    }
    private static async int Sum() throws MyError
    {
        int total = 0;
        AsyncIterator<int, MyError> it = Gen();
        while (await it.MoveNextAsync()) { total = total + it.Current; }
        return total + 100;
    }
    public static async int Main()
    {
        int r = 0;
        try { r = await Sum(); }
        catch (MyError e) { r = e.Code; }
        return r * 10 + Counter.Log;
    }
}
)ns", 91);
    expectRun(tests, root, "C17AsyncMain", std::string(kAsyncErrorPrelude) + R"ns(
class Program
{
    private static AsyncIterator<int, MyError> Gen(int n) throws MyError
    {
        raii Guard a = new Guard(1);
        yield return n;
        raii Guard b = new Guard(2);
        if (n > 0) { throw new MyError(37); }
        yield return 9;
    }
    public static async int Main()
    {
        int r = 0;
        AsyncIterator<int, MyError> it = Gen(4);
        try
        {
            bool h1 = await it.MoveNextAsync();
            r = it.Current;
            bool h2 = await it.MoveNextAsync();
            r = r + 1000;
        }
        catch (MyError e) { r = r * 100 + e.Code; }
        return r * 100 + Counter.Log;
    }
}
)ns", 43721);
    expectRun(tests, root, "C17SyncDrop", std::string(kAsyncErrorPrelude) + R"ns(
class Program
{
    private static AsyncIterator<int, MyError> Gen() throws MyError
    {
        raii Guard a = new Guard(1);
        yield return 1;
        throw new MyError(4);
    }
    private static int Walk() throws MyError
    {
        raii Guard w = new Guard(5);
        AsyncIterator<int, MyError> it = Gen();
        bool h1 = await it.MoveNextAsync();
        bool h2 = await it.MoveNextAsync();
        return 50;
    }
    public static int Main()
    {
        int r = 0;
        try { r = Walk(); } catch (MyError e) { r = e.Code; }
        return r * 100 + Counter.Log;
    }
}
)ns", 415);
    expectRun(tests, root, "C17AsyncDrop", std::string(kAsyncErrorPrelude) + R"ns(
class Program
{
    private static AsyncIterator<int, MyError> Gen() throws MyError
    {
        raii Guard a = new Guard(1);
        yield return 1;
        throw new MyError(4);
    }
    private static async int Walk() throws MyError
    {
        raii Guard w = new Guard(5);
        AsyncIterator<int, MyError> it = Gen();
        bool h1 = await it.MoveNextAsync();
        bool h2 = await it.MoveNextAsync();
        return 50;
    }
    public static async int Main()
    {
        int r = 0;
        try { r = await Walk(); } catch (MyError e) { r = e.Code; }
        return r * 100 + Counter.Log;
    }
}
)ns", 415);
    expectRun(tests, root, "C17AsyncAwaitDrop", std::string(kAsyncErrorPrelude) + R"ns(
class Program
{
    private static async int Fail() throws MyError { yield; throw new MyError(4); }
    private static async int Walk() throws MyError
    {
        raii Guard w = new Guard(5);
        int v = await Fail();
        return 50 + v;
    }
    public static async int Main()
    {
        int r = 0;
        try { r = await Walk(); } catch (MyError e) { r = e.Code; }
        return r * 100 + Counter.Log;
    }
}
)ns", 405);
}

// EN: C-17 (finding). The async-generator poll replayed its body from the top on every
//     MoveNextAsync. Now it resumes after its yield: each statement runs once, a parked generator
//     that is dropped or cancelled runs its active cleanup once, and one never started runs none.
// ES: C-17 (hallazgo). El poll del generador async repetia su cuerpo desde arriba en cada
//     MoveNextAsync. Ahora reanuda tras su yield: cada sentencia corre una vez, un generador
//     detenido que se destruye o se cancela corre una vez su cleanup activo, y uno que nunca
//     empezo no corre ninguno.
void testAsyncGeneratorResume(TestContext& tests, const std::filesystem::path& root)
{
    expectRunOutput(tests, root, "C17Rerun", R"ns(
using NativeSharp.Core;
class Box
{
    public int Id;
    public Box(int id) { Id = id; Console.Write("c"); Console.Write(Id); }
    ~Box() { Console.Write("d"); Console.Write(Id); }
}
class Program
{
    private static AsyncIterator<int, int> Gen()
    {
        Console.Write("s");
        raii Box a = new Box(1);
        yield return 1;
        Console.Write("m");
        yield return 2;
        Console.Write("e");
    }
    public static int Main()
    {
        AsyncIterator<int, int> it = Gen();
        bool h1 = await it.MoveNextAsync();
        Console.Write("|");
        bool h2 = await it.MoveNextAsync();
        Console.Write("|");
        bool h3 = await it.MoveNextAsync();
        Console.Write("|");
        bool h4 = await it.MoveNextAsync();
        Console.Write("|");
        return (h1 ? 1000 : 0) + (h2 ? 100 : 0) + (h3 ? 10 : 0) + (h4 ? 1 : 0);
    }
}
)ns",
        1100, "sc1|m|ed1||", false);
    // EN: A lambda may capture a local of an async generator; its cell lives in the frame.
    // ES: Una lambda puede capturar un local de un generador async; su celda vive en el frame.
    expectRun(tests, root, "C17Capture", R"ns(
delegate int Transform(int value);

class Program
{
    public static AsyncIterator<int, int> Gen()
    {
        int counter = 1;
        raii Transform bump = [ref mut counter] (int value) =>
        {
            counter = counter + value;
            return counter;
        };
        yield return bump(1);
        yield return bump(10);
        yield return counter;
    }

    public static int Main()
    {
        int sum = 0;
        AsyncIterator<int, int> it = Gen();
        while (await it.MoveNextAsync()) { sum = sum * 100 + it.Current; }
        return sum % 251;
    }
}
)ns", 128);
    expectRun(tests, root, "C17DropParked", R"ns(

public class Log { public static int FinallyCount; public static int DropCount; }
public class Resource { ~Resource() { Log.DropCount += 1; } }
class Program
{
    public static AsyncIterator<int, int> Gen(int n)
    {
        owned Resource res = new Resource();
        try { yield return n; if (n < 0) yield return -1; }
        finally { Log.FinallyCount += 1; }
    }

    private static int Use()
    {
        AsyncIterator<int, int> it = Gen(5);
        bool h1 = await it.MoveNextAsync();
        return h1 ? 1 : 0;
    }
    public static int Main()
    {
        int r = Use();
        return r * 100 + Log.FinallyCount * 10 + Log.DropCount;
    }
}
)ns", 111);
    expectRun(tests, root, "C17CancelFresh", R"ns(

public class Log { public static int FinallyCount; public static int DropCount; }
public class Resource { ~Resource() { Log.DropCount += 1; } }
class Program
{
    public static AsyncIterator<int, int> Gen(int n)
    {
        owned Resource res = new Resource();
        try { yield return n; if (n < 0) yield return -1; }
        finally { Log.FinallyCount += 1; }
    }

    public static int Main()
    {
        AsyncIterator<int, int> it = Gen(5);
        it.Cancel();
        bool h1 = await it.MoveNextAsync();
        return (h1 ? 100 : 0) + Log.FinallyCount * 10 + Log.DropCount;
    }
}
)ns", 0);
    expectRun(tests, root, "C17Cancel1", R"ns(

public class Log { public static int FinallyCount; public static int DropCount; }
public class Resource { ~Resource() { Log.DropCount += 1; } }
class Program
{
    public static AsyncIterator<int, int> Gen(int n)
    {
        owned Resource res = new Resource();
        try { yield return n; if (n < 0) yield return -1; }
        finally { Log.FinallyCount += 1; }
    }

    public static int Main()
    {
        AsyncIterator<int, int> it = Gen(-5);
        bool h1 = await it.MoveNextAsync();
        it.Cancel();
        bool h2 = await it.MoveNextAsync();
        return (!h2 && Log.FinallyCount == 1 && Log.DropCount == 1) ? 0 : 1;
    }
}
)ns", 0);
    expectRun(tests, root, "C17Cancel2", R"ns(

public class Log { public static int FinallyCount; public static int DropCount; }
public class Resource { ~Resource() { Log.DropCount += 1; } }
class Program
{
    public static AsyncIterator<int, int> Gen(int n)
    {
        owned Resource res = new Resource();
        try { yield return n; if (n < 0) yield return -1; }
        finally { Log.FinallyCount += 1; }
    }

    public static int Main()
    {
        AsyncIterator<int, int> it = Gen(-5);
        bool h1 = await it.MoveNextAsync();
        bool h2 = await it.MoveNextAsync();
        it.Cancel();
        bool h3 = await it.MoveNextAsync();
        return (h1 && h2 && !h3 && Log.FinallyCount == 1 && Log.DropCount == 1) ? 0 : 1;
    }
}
)ns", 0);
}

/// EN: `source` must compile; the program is not run (its imports may not exist on this machine).
/// ES: `source` debe compilar; el programa no se ejecuta (sus imports pueden no existir aqui).
void expectCompiles(
    TestContext& tests,
    const std::filesystem::path& root,
    std::string_view name,
    std::string_view source)
{
    const std::filesystem::path directory = root / std::string(name);
    const std::filesystem::path project = writeProject(directory, name);
    writeFile(directory / "Main.ns", source);
    std::string diagnostic;
    const ExitCode code = compile(project, directory / (std::string(name) + ".exe"), "-O0", diagnostic);
    tests.expect(code == ExitCode::Success, std::string(name) + ": compiles: " + diagnostic);
}

// EN: Freeze 3.0 (checked throw). C-13 left the `throw` statement unchecked. A thrown or rethrown
//     exception must now be caught by an enclosing try or declared by the callable; a throw in an
//     async function without `throws` used to crash the compiler, and is rejected the same way.
// ES: Congelamiento 3.0 (throw chequeado). C-13 dejo sin chequeo la sentencia `throw`. Una excepcion
//     lanzada o relanzada ahora debe atraparla un try envolvente o declararla el callable; un throw
//     en una funcion async sin `throws` tronaba el compilador, y se rechaza igual.
void testCheckedThrow(TestContext& tests, const std::filesystem::path& root)
{
    expectReject(tests, root, "P1Throw", R"ns(
using NativeSharp.Core;
class MyError : Exception { }
class Program
{
    private static int Fail(bool f) { if (f) { throw new MyError(); } return 1; }
    public static int Main() { return Fail(false); }
}
)ns", "throw of");
    expectReject(tests, root, "P1ThrowAsync", R"ns(
using NativeSharp.Core;
class MyError : Exception { }
class Program
{
    private static async int Fail(bool f) { yield; if (f) { throw new MyError(); } return 1; }
    public static async int Main() { return await Fail(false); }
}
)ns", "throw of");
    expectReject(tests, root, "P1Rethrow", R"ns(
using NativeSharp.Core;
class MyError : Exception { }
class Program
{
    private static int Fail(bool f) throws MyError { if (f) { throw new MyError(); } return 1; }
    private static int Wrap(bool f)
    {
        try { return Fail(f); }
        catch (MyError e) { throw; }
    }
    public static int Main() { return Wrap(false); }
}
)ns", "throw of");
    expectRun(tests, root, "P1Handled", R"ns(
using NativeSharp.Core;
class MyError : Exception { }
class Program
{
    private static int Local() { try { throw new MyError(); } catch (MyError e) { return 7; } }
    private static int Base(bool f) throws Exception { if (f) { throw new MyError(); } return 0; }
    private static int Fail(bool f) throws MyError { if (f) { throw new MyError(); } return 0; }
    private static int Wrap(bool f) throws MyError
    {
        try { return Fail(f); }
        catch (MyError e) { throw; }
    }
    public static int Main()
    {
        int r = Local() * 100;
        try { r = r + Base(true); } catch (Exception e) { r = r + 80; }
        try { r = r + Wrap(true); } catch (MyError e) { r = r + 9; }
        return r;
    }
}
)ns", 789);
}

// EN: Freeze 3.0 (owned arrays). An `owned T[]` local is an owner like `raii T[]`: it is released
//     at scope exit with its elements and is not deleted by hand. An owned array field is released
//     by its container; `delete[]` on it still works and clears the field, so it is released once.
// ES: Congelamiento 3.0 (arreglos owned). Un local `owned T[]` es dueno como `raii T[]`: se libera
//     al salir del scope con sus elementos y no se borra a mano. Un campo de arreglo owned lo libera
//     su contenedor; `delete[]` sobre el sigue funcionando y vacia el campo, asi se libera una vez.
void testOwnedArrays(TestContext& tests, const std::filesystem::path& root)
{
    expectRun(tests, root, "P3OwnedArray", R"ns(
class Counter { public static int Drops; }
class Box { public int Id; public Box(int id) { Id = id; } ~Box() { Counter.Drops = Counter.Drops * 10 + Id; } }
struct Val { public int Id; ~Val() { Counter.Drops = Counter.Drops * 10 + Id; } }
class Program
{
    private static void Classes()
    {
        owned Box[] boxes = new Box[2];
        boxes[0] = new Box(1);
        boxes[1] = new Box(2);
    }
    private static void Values()
    {
        owned Val[] vals = new Val[2];
        vals[0].Id = 3;
        vals[1].Id = 4;
    }
    public static int Main()
    {
        Classes();
        int a = Counter.Drops;
        Counter.Drops = 0;
        Values();
        return a * 100 + Counter.Drops;
    }
}
)ns", 1243);
    expectRun(tests, root, "P3OwnedArrayField", R"ns(
class Counter { public static int Drops; }
struct Val { public int Id; ~Val() { Counter.Drops = Counter.Drops * 10 + Id; } }
class Holder
{
    private owned Val[] vals;
    public Holder() { vals = new Val[2]; vals[0].Id = 5; vals[1].Id = 6; }
}
class Program
{
    public static int Main()
    {
        {
            raii Holder h = new Holder();
        }
        return Counter.Drops;
    }
}
)ns", 65);
    // EN: The manual model of ~List(): each element is deleted by hand and `delete[]` frees the block
    //     and clears the field; every element is dropped once and the block is freed once.
    // ES: El modelo manual de ~List(): cada elemento se borra a mano y `delete[]` libera el bloque y
    //     vacia el campo; cada elemento se destruye una vez y el bloque se libera una vez.
    expectRun(tests, root, "P3OwnedArrayFieldDelete", R"ns(
class Counter { public static int Drops; }
struct Val { public int Id; ~Val() { Counter.Drops = Counter.Drops * 10 + Id; } }
class Holder
{
    private owned Val[] vals;
    public Holder() { vals = new Val[2]; vals[0].Id = 5; vals[1].Id = 6; }
    ~Holder() { delete vals[0]; delete vals[1]; delete[] vals; }
}
class Program
{
    public static int Main()
    {
        {
            raii Holder h = new Holder();
        }
        return Counter.Drops;
    }
}
)ns", 56);
    // EN: DEC-06: `delete[]` of a local `owned T[]` consumes it: the elements and the block are
    //     released once, on the deleting path or by the lexical cleanup on the others (also when an
    //     exception leaves the scope), and a later read or a second delete is a use of a moved value.
    //     `raii T[]` keeps rejecting the explicit delete.
    // ES: DEC-06: `delete[]` de un local `owned T[]` lo consume: los elementos y el bloque se liberan
    //     una vez, en el camino que borra o por la limpieza lexica en los demas (tambien cuando una
    //     excepcion sale del scope), y leerlo o borrarlo otra vez es usar un valor movido. `raii T[]`
    //     sigue rechazando el delete explicito.
    expectRun(tests, root, "DEC06OwnedArrayDelete", R"ns(
class Program
{
    public static int Main()
    {
        owned int[] values = new int[3];
        values[0] = 40;
        values[1] = 2;
        int r = values[0] + values[1];
        delete[] values;
        return r;
    }
}
)ns", 42);
    expectRun(tests, root, "DEC06OwnedArrayDeleteBranch", R"ns(
class Counter { public static int Drops; }
struct Guard
{
    public int V;
    public Guard(int v) { V = v; }
    ~Guard() { Counter.Drops = Counter.Drops + V; }
}
class Program
{
    public static int Run(bool early)
    {
        owned Guard[] g = new Guard[2];
        g[0] = new Guard(1);
        g[1] = new Guard(2);
        if (early)
        {
            delete[] g;
            return 1;
        }
        return 2;
    }
    public static int Main()
    {
        int a = Run(true);
        int b = Run(false);
        return Counter.Drops * 10 + a + b;
    }
}
)ns", 63);
    expectRun(tests, root, "DEC06OwnedArrayDeleteUnwind", R"ns(
using NativeSharp.Core;
class Counter { public static int Drops; }
struct Guard
{
    public int V;
    public Guard(int v) { V = v; }
    ~Guard() { Counter.Drops = Counter.Drops + V; }
}
class Program
{
    public static void Run(bool early)
    {
        owned Guard[] g = new Guard[2];
        g[0] = new Guard(1);
        g[1] = new Guard(2);
        try
        {
            if (early) delete[] g;
            if (early) throw new Exception("x");
        }
        catch (Exception e)
        {
        }
    }
    public static int Main()
    {
        Run(true);
        int afterEarly = Counter.Drops;
        Run(false);
        return afterEarly * 10 + Counter.Drops;
    }
}
)ns", 36);
    expectReject(tests, root, "DEC06OwnedArrayUseAfterDelete", R"ns(
class Program
{
    public static int Main()
    {
        owned int[] values = new int[3];
        delete[] values;
        return values[0];
    }
}
)ns", "use of moved-from RAII local 'values'");
    expectReject(tests, root, "DEC06OwnedArrayDeleteTwice", R"ns(
class Program
{
    public static int Main()
    {
        owned int[] values = new int[3];
        delete[] values;
        delete[] values;
        return 0;
    }
}
)ns", "use of moved-from RAII local 'values'");
    expectReject(tests, root, "DEC06OwnedArrayDeleteBorrowed", R"ns(
class Program
{
    public static int Main()
    {
        owned int[] values = new int[3];
        ref int first = ref values[0];
        delete[] values;
        return first;
    }
}
)ns", "while a borrowed local is alive");
    expectReject(tests, root, "DEC06RaiiArrayDelete", R"ns(
class Program
{
    public static int Main()
    {
        raii int[] values = new int[3];
        delete[] values;
        return 0;
    }
}
)ns", "explicit delete of a RAII local is forbidden");
    // EN: `delete[]` of an owned array field of plain elements, in a plain and in a generic class,
    //     clears the field, so the destruction entry does not free the buffer a second time.
    // ES: `delete[]` de un campo de arreglo owned de elementos simples, en una clase normal y en una
    //     generica, vacia el campo, asi la entrada de destruccion no libera el buffer otra vez.
    expectRun(tests, root, "P3OwnedArrayFieldPlain", R"ns(
class Box
{
    private owned int[] items;
    public Box() { items = new int[2]; items[0] = 40; items[1] = 2; }
    public int Sum() { return items[0] + items[1]; }
    ~Box() { delete[] items; }
}
class Program
{
    public static int Main()
    {
        int r = 0;
        {
            raii Box box = new Box();
            r = box.Sum();
        }
        return r;
    }
}
)ns", 42);
    expectRun(tests, root, "P3OwnedArrayFieldGeneric", R"ns(
class Counter { public static int Drops; }
struct Guard { public int Id; ~Guard() { Counter.Drops = Counter.Drops * 10 + Id; } }
class Box<T>
{
    private owned T[] items;
    private int count;
    public Box() { items = new T[1]; }
    public void Add(T value)
    {
        if (count == items.Length)
        {
            const T[] grown = new T[items.Length * 2];
            int i = 0;
            while (i < count) { grown[i] = move(items[i]); i = i + 1; }
            delete[] items;
            items = grown;
        }
        items[count] = move(value);
        count = count + 1;
    }
}
class Plain<T>
{
    private owned T[] items;
    public Plain() { items = new T[1]; }
    ~Plain() { delete[] items; }
}
class Program
{
    private static Guard Make(int id) { Guard g; g.Id = id; return move(g); }
    public static int Main()
    {
        {
            raii Plain<int> plain = new Plain<int>();
            raii Box<Guard> box = new Box<Guard>();
            box.Add(Make(1));
            box.Add(Make(2));
        }
        return Counter.Drops;
    }
}
)ns", 21);
}

// EN: Freeze 3.0 (receiver temporaries). `Make().Field` whose result is plain data (here a struct
//     of scalars) destroys the temporary after copying the field out; before, it leaked.
// ES: Congelamiento 3.0 (temporales receptores). `Make().Campo` cuyo resultado son datos simples
//     (aqui un struct de escalares) destruye el temporal despues de copiar el campo; antes se fugaba.
void testReceiverTemporaries(TestContext& tests, const std::filesystem::path& root)
{
    expectRun(tests, root, "P4Receiver", R"ns(
class Counter { public static int Drops; }
struct Inner { public int A; public int B; }
struct Val
{
    public Inner In;
    public int Id;
    ~Val() { Counter.Drops = Counter.Drops * 10 + Id; }
}
class Box { public Inner In; public int Id; ~Box() { Counter.Drops = Counter.Drops * 10 + Id; } }
class Program
{
    private static Val MakeVal(int id) { Val v; v.Id = id; v.In.A = 1; v.In.B = 2; return move(v); }
    private static owned Box MakeBox(int id) { owned Box b = new Box(); b.Id = id; b.In.A = 3; return move(b); }
    public static int Main()
    {
        Inner i1 = MakeVal(7).In;
        Inner i2 = MakeBox(8).In;
        return (i1.A + i1.B + i2.A) * 100 + Counter.Drops;
    }
}
)ns", 678);
}

/// EN: Freeze 3.0 DEC-13. Compiles `source` with --validate-system-imports and checks the outcome:
///     success when `fragment` is empty, otherwise a failure whose diagnostic contains `fragment`.
/// ES: Congelamiento 3.0 DEC-13. Compila `source` con --validate-system-imports y comprueba el
///     resultado: exito si `fragment` esta vacio; si no, un fallo cuyo diagnostico contiene `fragment`.
void expectSystemImportValidation(
    TestContext& tests,
    const std::filesystem::path& root,
    std::string_view name,
    std::string_view source,
    std::string_view fragment)
{
    const std::filesystem::path directory = root / std::string(name);
    const std::filesystem::path project = writeProject(directory, name);
    writeFile(directory / "Main.ns", source);
    const std::string projectText = project.string();
    const std::string executableText = (directory / (std::string(name) + ".exe")).string();
    const std::array<std::string_view, 7> arguments = {"-O0", "--no-artifact-cache",
        "--validate-system-imports", "--emit-project-oop-exe", projectText, "-o", executableText};
    Driver driver;
    std::ostringstream output;
    std::ostringstream error;
    const ExitCode code = driver.run(arguments, output, error);
    const std::string diagnostic = output.str() + error.str();
    const bool accepted = fragment.empty()
        ? code == ExitCode::Success
        : code != ExitCode::Success && diagnostic.find(fragment) != std::string::npos;
    tests.expect(accepted, std::string(name) + ": --validate-system-imports outcome: " + diagnostic);
}

// EN: Freeze 3.0 DEC-13 (unresolved DLL imports). By default the build never reads host DLLs, so a
//     symbol the named DLL does not export still compiles. Under --validate-system-imports it is a
//     compile error; a DLL that cannot be found at compile time is not judged even then.
// ES: Congelamiento 3.0 DEC-13 (imports de DLL sin resolver). Por defecto la compilacion nunca lee
//     DLL del host, asi que un simbolo que la DLL nombrada no exporta compila. Con
//     --validate-system-imports es error de compilacion; una DLL que no se encuentra al compilar no
//     se juzga ni siquiera entonces.
void testExternalImports(TestContext& tests, const std::filesystem::path& root)
{
    constexpr std::string_view missingSymbol = R"ns(
class Native
{
    extern C from "kernel32.dll"
    {
        public static int NoSuchFunctionAnywhere(int x);
    }
}
class Program
{
    public static int Main() { return Native.NoSuchFunctionAnywhere(3); }
}
)ns";
    expectCompiles(tests, root, "P5DllCalled", missingSymbol);
    expectSystemImportValidation(
        tests, root, "P5DllCalledValidated", missingSymbol, "is not exported by 'kernel32.dll'");
    expectSystemImportValidation(tests, root, "P5DllMissingValidated", R"ns(
class Native
{
    extern C from "nosuchlibrary_xyz.dll"
    {
        public static int Anything(int x);
    }
}
class Program
{
    public static int Main() { return Native.Anything(3); }
}
)ns", "");
    expectCompiles(tests, root, "P5DllMissing", R"ns(
class Native
{
    extern C from "nosuchlibrary_xyz.dll"
    {
        public static int Anything(int x);
    }
}
class Program
{
    public static int Main() { return Native.Anything(3); }
}
)ns");
}

int main()
{
    TestContext tests;
    const std::filesystem::path root = std::filesystem::path(NSC_BINARY_DIR) / "freeze30_contracts";
    std::error_code filesystemError;
    std::filesystem::remove_all(root, filesystemError);
    std::filesystem::create_directories(root, filesystemError);

    testAllocatorAlignment(tests, root);
    testCoreDisposable(tests, root);
    testLostOwnershipRejected(tests, root);
    testOwningFormsAccepted(tests, root);
    testCatchOwnsException(tests, root);
    testCaughtExceptionStaysInCatch(tests, root);
    testSynchronousAwaitRejected(tests, root);
    testSynchronousAwaitAccepted(tests, root);
    testAsyncMainEntryPoint(tests, root);
    testCoroutineResume(tests, root);
    testAwaitReplayRunsOperandsOnce(tests, root);
    testCoroutineFrameLocals(tests, root);
    testGeneratorResume(tests, root);
    testGeneratorEarlyStop(tests, root);
    testFileModeMatchesProject(tests, root);
    testCaptureSurvivesSuspension(tests, root);
    testMissingReturn(tests, root);
    testGlobalDeclarationsRejected(tests, root);
    testValueLocalLifetimes(tests, root);
    testValueMoves(tests, root);
    testStructEnumerator(tests, root);
    testValueFieldsAndTemporaries(tests, root);
    testValueArrayElements(tests, root);
    testValueListElements(tests, root);
    testArrayElementLiveness(tests, root);
    testMemberCopyConstraint(tests, root);
    testConstructorCopyConstraint(tests, root);
    testInDelegateOverloads(tests, root);
    testReadonlyReferenceReturns(tests, root);
    testInAcceptsReadonlyPlace(tests, root);
    testRefToFieldElement(tests, root);
    testRefReturnReceiver(tests, root);
    testRefReturnLibrary(tests, root);
    testConditionalGenericCopy(tests, root);
    testMoveOnlyPlaceReturn(tests, root);
    testIndexerAccessorWhere(tests, root);
    testMoveOnlyReferenceLocal(tests, root);
    testCollectionReferences(tests, root);
    testCollectionReferencesSorted(tests, root);
    testSpanLivenessMap(tests, root);
    testCaughtExceptionParameterLoan(tests, root);
    testCaughtExceptionNotCaptured(tests, root);
    testReplacedAllocatorSignature(tests, root);
    testObjectAbiBoundary(tests, root);
    testSortedDictionaryIndexerGet(tests, root);
    testDeclaredErrorsRejected(tests, root);
    testDeclaredErrorsHandled(tests, root);
    testFutureDrops(tests, root);
    testFutureMoveOnly(tests, root);
    testFutureFields(tests, root);
    testValueDropVerification(tests, root);
    testNestedAwaitResume(tests, root);
    testLoopExpressionAwait(tests, root);
    testMatchAwaitReplay(tests, root);
    testLoopOperandResume(tests, root);
    testNewToOrdinaryParameterRejected(tests, root);
    testNewToOwnedParameterAccepted(tests, root);
    testNewToNonOwnedReturn(tests, root);
    testNewToNonOwnedField(tests, root);
    testOwnedArgumentTransfer(tests, root);
    testDeleteOwnedParameter(tests, root);
    testPlainArrayDelete(tests, root);
    testGenericMoveReferenceCopy(tests, root);
    testImportedCArgumentTrivial(tests, root);
    testGeneratorCleanup(tests, root);
    testAsyncGeneratorErrors(tests, root);
    testAsyncGeneratorResume(tests, root);
    testCheckedThrow(tests, root);
    testOwnedArrays(tests, root);
    testReceiverTemporaries(tests, root);
    testExternalImports(tests, root);
    return tests.resultKeeping(root);
}
