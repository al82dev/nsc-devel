"""Directed end-to-end checks for function calls evaluated by #run."""

import os
from pathlib import Path
import subprocess
import sys
import tempfile


def invoke(command, root):
    """Run the requested command and print its exact status and output."""
    print("RUN", *map(str, command), flush=True)
    result = subprocess.run(list(map(str, command)), cwd=root, text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            timeout=120, check=False)
    print("EXIT", result.returncode, flush=True)
    print(result.stdout, flush=True)
    return result


def check(compiler, root, name, source, expected, diagnostic=None):
    """Compile one isolated project and execute both optimization levels."""
    directory = root / name
    directory.mkdir()
    project = directory / "Test.nsproj"
    project.write_text(
        "<Project><PropertyGroup><Name>" + name + "</Name><Language>3</Language>"
        "</PropertyGroup><ItemGroup><Compile Include=\"Main.ns\" /></ItemGroup></Project>",
        encoding="utf-8")
    (directory / "Main.ns").write_text(source, encoding="utf-8")
    for opt in ("O0", "O2"):
        output = directory / (name + "-" + opt + ".exe")
        result = invoke([compiler, "--emit-project-oop-exe", project, "-" + opt,
                         "--no-artifact-cache", "-o", output], root)
        if diagnostic is not None:
            if result.returncode == 0 or diagnostic not in result.stdout:
                raise AssertionError(name + ": wrong rejection")
            continue
        if result.returncode:
            raise AssertionError(name + ": compilation failed")
        result = invoke([output], root)
        if result.returncode != expected:
            raise AssertionError(name + ": wrong program result")


def main():
    """Exercise same-file calls, arguments, and the feature-absent control."""
    if len(sys.argv) != 2 or os.name != "nt":
        raise SystemExit("usage: comptime_calls.py <isolated-nsc.exe> (Windows)")
    compiler = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="nsc-73-run-") as temporary:
        root = Path(temporary)
        check(compiler, root, "FreeCall",
              "int Answer() { return 42; }\n"
              "class Program { public static int Main() { return #run(Answer()); } }\n", 42)
        check(compiler, root, "Arguments",
              "int Add(int a, int b) { return a + b; }\n"
              "class Program { public static int Main() { return #run(Add(19, 23)); } }\n", 42)
        check(compiler, root, "Nested",
              "int Twice(int a) { return a + a; } int Add(int a, int b) { return a + b; } "
              "class Program { public static int Main() { return #run(Add(Twice(10), 22)); } }", 42)
        check(compiler, root, "Namespace",
              "namespace Test { int Answer() { return 42; } "
              "class Program { public static int Main() { return #run(Answer()); } } }", 42)
        check(compiler, root, "Wrapping",
              "int Wrap() { return (2147483647 + 1) - (2147483600 + 48) + 42; } "
              "class Program { public static int Main() { "
              "if (#run(Wrap()) != Wrap()) { return 1; } return 42; } }", 42)
        check(compiler, root, "SignedDivision",
              "int Divide(int value) { return value / 3; } "
              "class Program { public static int Main() { "
              "if (#run(Divide(-7)) != Divide(-7)) { return 1; } return 42; } }", 42)
        check(compiler, root, "ComptimeMarker",
              "int Answer() { return 42; } class Program { public static int Main() "
              "{ comptime int value = #run(Answer()); return value; } }", 42)
        check(compiler, root, "TwoCalls",
              "int Value() { return 21; } class Program { public static int Main() "
              "{ return #run(Value()) + #run(Value()); } }", 42)
        check(compiler, root, "EarlierDirective",
              "int Inner() { return 21; } int Outer() { return #run(Inner()); } "
              "class Program { public static int Main() { return #run(Outer()) + #run(Outer()); } }", 42)
        check(compiler, root, "ForwardDirective",
              "class Program { public static int Main() { return #run(Outer()); } } "
              "int Outer() { return #run(Inner()); } int Inner() { return 42; }",
              None, "unevaluated #run inside a called function")
        check(compiler, root, "Comments",
              "// #run(NotAFunction())\nint Value() { return 42; } "
              "class Program { public static int Main() { return #run(Value()); } }", 42)
        boundary = ("int Value() { return 42; } class Program { public static int Main() "
                    "{ return #run(Value()); } }")
        check(compiler, root, "SourceBoundary", boundary + (" " * (4096 - len(boundary))), 42)
        parameters = ", ".join("int a" + str(i) for i in range(16))
        arguments = ", ".join(["42"] + ["0"] * 15)
        check(compiler, root, "ParameterBoundary",
              "int Value(" + parameters + ") { return a0; } "
              "class Program { public static int Main() { return #run(Value(" + arguments + ")); } }", 42)
        too_many = ", ".join("int a" + str(i) for i in range(17))
        too_many_arguments = ", ".join(["0"] * 17)
        check(compiler, root, "ParameterLimit",
              "int Value(" + too_many + ") { return a0; } class Program { "
              "public static int Main() { return #run(Value(" + too_many_arguments + ")); } }",
              None, "too many function parameters")
        check(compiler, root, "NoRun",
              "class Program { public static int Main() { return 42; } }\n", 42)
        check(compiler, root, "LongWithoutFeature",
              "class Program { public static int Main() { return 42; } } //" + ("x" * 4096), 42)
        check(compiler, root, "LegacyExpression",
              "class Program { public static int Main() { return #run(20 + 22); } }", 42)
        check(compiler, root, "SideEffect",
              "int state = 0; int Effect() { return ++state; } "
              "class Program { public static int Main() { return #run(Effect()); } }",
              None, "comptime sandbox forbids")
        check(compiler, root, "Statement",
              "int Value() { int x = 42; return x; } "
              "class Program { public static int Main() { return #run(Value()); } }",
              None, "comptime sandbox forbids")
        check(compiler, root, "External",
              "int Value() { return Missing(); } "
              "class Program { public static int Main() { return #run(Value()); } }",
              None, "comptime sandbox forbids")
        check(compiler, root, "AsyncEffect",
              "async int Value() { return 42; } "
              "class Program { public static int Main() { return #run(Value()); } }",
              None, "unsupported function signatures or effects")
        check(compiler, root, "WrongType",
              "long Value(long x) { return x; } "
              "class Program { public static int Main() { return #run(Value(42)); } }",
              None, "comptime sandbox forbids")
        check(compiler, root, "Overload",
              "int Value(int x) { return x; } int Value(long x) { return 1; } "
              "class Program { public static int Main() { return #run(Value(42)); } }",
              None, "ambiguous function lookup")
        check(compiler, root, "Shadow",
              "int Value() { return 42; } class Program { public static int Main() "
              "{ int Value = 1; return #run(Value()); } }",
              None, "potentially shadowed function name")
        check(compiler, root, "MethodShadow",
              "int Value() { return 42; } class Program { public static int Value() { return 1; } "
              "public static int Main() { return #run(Value()); } }",
              None, "ambiguous function lookup")
        check(compiler, root, "PropertyShadow",
              "int Value() { return 42; } class Program { public static int Value "
              "{ get { return 7; } } public static int Main() { return #run(Value()); } }",
              None, "potentially shadowed function name")
        check(compiler, root, "InstanceScope",
              "int Value() { return 42; } class Program { public int Get() "
              "{ return #run(Value()); } public static int Main() { return 42; } }",
              None, "instance function lookup")
        check(compiler, root, "StaticMethod",
              "class Program { public static int Twice(int x) { return x + x; } "
              "public static int Value() { return Twice(21); } "
              "public static int Main() { return #run(Value()); } }", 42)
        check(compiler, root, "QualifiedMethod",
              "class Program { public static int Value() { return 42; } "
              "public static int Main() { return #run(Program.Value()); } }", 42)
        check(compiler, root, "StaticState",
              "class Program { public static int marker = 1; public static int Value() { return 42; } "
              "public static int Main() { return #run(Value()); } }",
              None, "potential type initialization")
        check(compiler, root, "QualifiedShadow",
              "class Program { public static int Value() { return 42; } "
              "public static int Main() { int Program = 1; return #run(Program.Value()); } }",
              None, "unsupported qualified function lookup")
        check(compiler, root, "OtherType",
              "class Other { public static int Value() { return 42; } } "
              "class Program { public static int Main() { return #run(Other.Value()); } }",
              None, "unsupported qualified function lookup")
        check(compiler, root, "Import",
              "using Sample; int Value() { return 42; } "
              "class Program { public static int Main() { return #run(Value()); } }",
              None, "function lookup through imports")
        check(compiler, root, "Partial",
              "int Value() { return 42; } partial class Program { "
              "public static int Main() { return #run(Value()); } }",
              None, "partial declarations and raw strings")
        check(compiler, root, "InheritedScope",
              "int Value() { return 42; } class Base { } class Program : Base { "
              "public static int Main() { return #run(Value()); } }",
              None, "inherited or generic function lookup")
        check(compiler, root, "OversizedResult",
              "int F() { return 2147483647; } class Program { public static int Main() "
              "{ return #run(F()); } }",
              None, "result does not fit")
        check(compiler, root, "Cycle",
              "int Value() { return Value(); } "
              "class Program { public static int Main() { return #run(Value()); } }",
              None, "comptime nesting limit exceeded")
        check(compiler, root, "DivisionOverflow",
              "int Value() { return (2147483647 + 1) / -1; } "
              "class Program { public static int Main() { return #run(Value()); } }",
              None, "comptime integer division trap")
        check(compiler, root, "Zero",
              "int Value() { return 1 / 0; } "
              "class Program { public static int Main() { return #run(Value()); } }",
              None, "comptime integer division trap")
        repeated = " + ".join(["#run(Value())"] * 100)
        check(compiler, root, "CumulativeMemory",
              "int Value() { return 1; } class Program { public static int Main() "
              "{ return " + repeated + "; } }",
              None, "comptime memory budget exceeded")
        check(compiler, root, "SourceLimit",
              "int Value() { return 42; } class Program { public static int Main() "
              "{ return #run(Value()); } } //" + ("x" * 4096),
              None, "comptime function source limit exceeded")
    return 0


if __name__ == "__main__":
    sys.exit(main())
