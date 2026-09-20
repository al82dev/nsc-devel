# Tanda 28 — SH26 — semántica multimódulo

**Estado: compilado; gate dirigido aprobado; listo para commit.**

---

## 1. Archivos que entran en el commit

**Nuevos (13):**

```
selfhost/nsc/Sema/ModuleSet.ns
selfhost/contracts/module-set.contract.json
selfhost/bootstrap/verify_module_set.py
cmake/VerifyNativeSharpModuleSet.cmake
docs/2.0/self-hosting/tandas/TANDA-28-SH26-SEMANTICA-MULTIMODULO.md
tandas_claude_commit/TANDA-28-SH26.md
selfhost/tests/module-set/positive/A.ns
selfhost/tests/module-set/positive/B.ns
selfhost/tests/module-set/positive/ModuleSetPositive.nsproj
selfhost/tests/module-set/positive/ExternalFixture.nsmeta
selfhost/tests/module-set/negative/A.ns
selfhost/tests/module-set/negative/B.ns
selfhost/tests/module-set/negative/ModuleSetNegative.nsproj
```

**Modificados (6):**

```
selfhost/nsc/Sema/SemanticAnalyzer.ns
selfhost/nsc/Driver/Program.ns
selfhost/nsc/Projects/ProjectSystem.ns
selfhost/contracts/source-graph.contract.json
docs/2.0/SELF_HOSTING_ROADMAP.md
CMakeLists.txt
```

```bat
git add selfhost/nsc/Sema/ModuleSet.ns ^
        selfhost/nsc/Sema/SemanticAnalyzer.ns ^
        selfhost/nsc/Driver/Program.ns ^
        selfhost/nsc/Projects/ProjectSystem.ns ^
        selfhost/contracts/module-set.contract.json ^
        selfhost/contracts/source-graph.contract.json ^
        selfhost/bootstrap/verify_module_set.py ^
        cmake/VerifyNativeSharpModuleSet.cmake ^
        CMakeLists.txt ^
        docs/2.0/SELF_HOSTING_ROADMAP.md ^
        docs/2.0/self-hosting/tandas/TANDA-28-SH26-SEMANTICA-MULTIMODULO.md ^
        tandas_claude_commit/TANDA-28-SH26.md ^
        selfhost/tests/module-set/positive/A.ns ^
        selfhost/tests/module-set/positive/B.ns ^
        selfhost/tests/module-set/positive/ModuleSetPositive.nsproj ^
        selfhost/tests/module-set/positive/ExternalFixture.nsmeta ^
        selfhost/tests/module-set/negative/A.ns ^
        selfhost/tests/module-set/negative/B.ns ^
        selfhost/tests/module-set/negative/ModuleSetNegative.nsproj
```

## 2. Lo que NO entra

Igual que en la 27: nada de `selfhost/out/`, nada de `selfhost/evidence/runs/`, ni objetos,
ejecutables, `.rsp`, `.log` o temporales. `MANIFEST_SHA256.txt`: comprobar, no asumir.

## 3. Parches de integración aplicados

Claude los dejó inicialmente fuera para no pisar T27. ChatGPT los integró después de cerrar T27;
los bloques se conservan aquí como registro de la entrega.

### 3.1 `selfhost/nsc/Driver/Program.ns`

`using NativeSharp.Compiler.Sema;` ya está en la cabecera; no hace falta añadirlo.

Dos líneas de ayuda, tras la de `--check-project`:

```csharp
            Console.WriteLine("  nsc-next --dump-module-set <project.nsproj>");
            Console.WriteLine("  nsc-next --dump-module-exports <project.nsproj>");
```

Un método, junto a `DumpSourceGraph`:

```csharp
        // SH26. El conjunto de compilacion y su identidad de simbolos entre unidades.
        private static int DumpModuleSet(string projectPath, bool exports)
        {
            raii SourceGraph graph = SourceGraphBuilder.Build(projectPath);
            raii ProjectModel project = ProjectLoader.Load(projectPath);
            raii ModuleSet set = ModuleSet.Build(graph, project.Name());
            raii string dump = exports ? ModuleSetDumper.DumpExports(set) : ModuleSetDumper.Dump(set);
            Console.Write(dump);
            return set.Failed() ? 1 : 0;
        }
```

Y el despacho en `Main`, junto al de `--dump-source-graph`:

```csharp
            if (command == "--dump-module-set" || command == "--dump-module-exports")
            {
                if (count != 3) { return 2; }
                return DumpModuleSet(arguments[2], command == "--dump-module-exports");
            }
```

### 3.2 `CMakeLists.txt`

Tras el bloque `set_tests_properties(nsc.selfhost.source-graph ...)`, **conservando los finales de
línea del archivo** (está mezclado: 2868 CRLF y 991 LF, y la zona de estos bloques es LF):

```cmake

    add_test(
        NAME nsc.selfhost.module-set
        COMMAND "${CMAKE_COMMAND}"
            "-DNSC_EXECUTABLE=$<TARGET_FILE:nsc>"
            "-DPYTHON_EXECUTABLE=${Python3_EXECUTABLE}"
            "-DSOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}"
            "-DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/selfhost-module-set"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyNativeSharpModuleSet.cmake")
    set_tests_properties(nsc.selfhost.module-set PROPERTIES RUN_SERIAL TRUE)
```

## 4. Título del commit

```
selfhost(tanda-28): resolve symbols across the compilation set
```

## 5. Cuerpo del commit

```
selfhost(tanda-28): resolve symbols across the compilation set

SH26 gives the compilation set one symbol identity: a name declared in one
file resolves from another, duplicate types across units are diagnosed, and
callables sharing a qualified name form overload sets.

Scope follows the roadmap, not convenience. SH25 fixed the compilation set as
the root project's units. LibraryReference metadata is now consumed through
deterministic NSMETA/2 public-symbol records without parsing provider sources.
The positive fixture proves this by resolving a callable that has no source in
the compilation. Rebuilding Core, Collections, IO and LLVM.C from source is
SH40. Hence two deliberately different numbers: the graph holds 92 units, the
compilation set holds 23.

That also settles what the first SH25 gate run exposed. SemanticAnalyzer never
calls AddError in its 41 KB, and an unresolved identifier is not an error --
the symbol stays -1, the type becomes `error`, analysis continues. So
semantic.ErrorCount() is always zero and those library failures came from the
lexer or the parser, not from name resolution. This change does not fix that;
it puts it out of scope the way the roadmap already had it, as a located
question deferred to SH40.

Type identity in this compiler is nominal by text: TypeTable.Find compares
Display(). An export therefore carries its type text, and the receiving model
interns it to the same meaning without the two models sharing a single type id.
For a callable that text is its signature, which is what separates overloads
without rebuilding parameter lists.

Two identity rules, and they differ: a type may not repeat its qualified name
anywhere in the set; a callable may -- that is an overload -- but the pair
(qualified name, signature) may not, because return type is not part of
overload identity.

ModuleSet runs two passes. Pass A lexes, parses and calls the new DeclareOnly()
on each unit: declarations are all the index needs, and analysing bodies here
would double every build. Pass B analyses each unit with the other units'
globals already injected into scope 0 by DeclareExternal.

SemanticAnalyzer's logic is unchanged. Lookup already walks to scope 0; scope 0
now holds the rest of the program. A single file behaves exactly as before,
which is what protects the SH06 contract.

An injected symbol keeps declaration = -1, and Analyze() now skips those.
Without that guard the second pass would call tree.Node(-1) on every imported
function.

Out of scope and located: cross-module layout and size unification is SH29 --
an imported nominal type interns with SetLayout(0, 1) until its declaration is
seen. Overload resolution by argument type across modules, and generic instance
identity across projects, are SH34.

Gate: nsc.selfhost.module-set, O0-O3, both dumps compared for byte identity.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01KpTyhrh4QaKhjRJzUbmQsw
```

## 6. Cómo probar ESTOS cambios

No toca C++. `nsc0` no se recompila; CMake sí reconfigura.

### Paso 1 — humo

```bat
nsc.exe --emit-project-oop-exe A:\nsc-devel\selfhost\nsc\NativeSharp.Compiler.nsproj -O0 -g0 -o A:\nsc-devel\selfhost\out\t28\nsc-next-o0.exe
A:\nsc-devel\selfhost\out\t28\nsc-next-o0.exe --dump-module-set A:\nsc-devel\selfhost\nsc\NativeSharp.Compiler.nsproj
```

Forma esperada:

```
module-set schema=1
status=ok
units=23
external-projects=4
external-metadata=0
exports=<N>
problems=0
cross-unit-resolutions=<N mayor que cero>
external NativeSharpCore
external NativeSharpCoreCollectionsGeneric
external NativeSharpCoreIO
external NativeSharpLLVM_C
unit 70 Basic/GenerationArena.ns lex=0 parse=0 external=0
...
```

Lo que hay que mirar, en este orden:

1. `units=23`, no 92. Si sale 92 es que el filtro por propietario raíz no está haciendo nada.
2. `problems=0`. Cualquier línea `problem ...` nombra un tipo duplicado o una firma repetida entre
   unidades — y si aparece, **es un defecto real del árbol**, no del gate.
3. Todas las unidades con `lex=0 parse=0`. Una con `parse>0` es un archivo que el front-end de
   `nsc-next` no acepta: manda esa línea.
4. `cross-unit-resolutions=2845`: la presiembra se consume realmente, no sólo se construye.

### Paso 2 — los exports

```bat
A:\nsc-devel\selfhost\out\t28\nsc-next-o0.exe --dump-module-exports A:\nsc-devel\selfhost\nsc\NativeSharp.Compiler.nsproj
```

Una línea por global, con su clase, su nombre cualificado y su firma. El gate también exige que
`ExternalFixture.nsmeta` aporte `ExternalFixture.ExternalValue` y que `B.ns` lo resuelva sin que
exista código fuente del proveedor.

### Paso 3 — el gate

```bat
cmake -S A:\nsc-devel -B A:\nsc-devel\selfhost\out\nsc0-build
ctest --test-dir A:\nsc-devel\selfhost\out\nsc0-build -R nsc.selfhost.module-set -V
```

Comprueba lo que el humo no puede: que las dos superficies sean byte a byte idénticas entre O0–O3,
que SH25 siga aceptando las 23 unidades actuales y que la metadata externa se consuma realmente.

## 7. Si falla

Manda la salida completa, no el resumen. El verificador nombra la comparación exacta:
`O2 module-set dump differs from O0`, `compiled N units, contract says 23`,
`external projects are [...]`, `identity problems: [...]`, `front-end errors: N`.

**Un fallo que espero y que no es del gate:** si `ModuleSet.ns` o el parche de `SemanticAnalyzer.ns`
no compilan, el paso 1 muere en `nsc.exe`. Esa salida es lo más útil que puedes mandarme, porque es
la primera vez que se compila código mío que usa `owned`/`move` en campos de clase con construcción
en dos fases.
