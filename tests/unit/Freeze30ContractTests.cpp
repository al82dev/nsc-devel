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
#include "nativesharp/Driver/Driver.h"

#include "../support/TestContext.h"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
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
    bool collections = false)
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
        "<Project><PropertyGroup><Name>" + std::string(name) +
            "</Name></PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup>"
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

/// EN: `source` must be rejected and the diagnostic must contain `fragment`.
/// ES: `source` debe rechazarse y el diagnostico debe contener `fragment`.
void expectReject(
    TestContext& tests,
    const std::filesystem::path& root,
    std::string_view name,
    std::string_view source,
    std::string_view fragment)
{
    const std::filesystem::path directory = root / std::string(name);
    const std::filesystem::path project = writeProject(directory, name);
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
    public static void Fail() { throw new Boom("bad"); }

    public static void FailOwned()
    {
        raii Boom problem = new Boom("owned");
        throw move(problem);
    }

    public static void Middle()
    {
        try { Fail(); }
        catch (Boom b) { throw; }
    }

    public static void Again()
    {
        try { Fail(); }
        catch (Boom b) { throw b; }
    }

    public static void Wrap()
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
)ns", 10302);
}

// EN: List<T> of a struct with a destructor drops each live element exactly once, also across
//     growth and RemoveAt; a removed slot is left with the default value (Id 0).
// ES: List<T> de un struct con destructor destruye cada elemento vivo una sola vez, tambien al
//     crecer y en RemoveAt; un slot removido queda con el valor por defecto (Id 0).
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
)ns", 22013, true);
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
    testSynchronousAwaitRejected(tests, root);
    testSynchronousAwaitAccepted(tests, root);
    testAsyncMainEntryPoint(tests, root);
    testCoroutineResume(tests, root);
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
    return tests.resultKeeping(root);
}
