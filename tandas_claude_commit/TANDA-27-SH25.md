# Tanda 27 — SH25 — grafo fuente determinista

**Estado: compilado; gate dirigido aprobado; listo para commit.**
El baseline completo se ejecutará de forma acumulada al terminar las tandas de Claude. No
commitear hasta que pase:

```bat
ctest -R nsc.selfhost.source-graph -V
```

---

## 1. Archivos que entran en el commit

**Nuevos (6):**

```
selfhost/nsc/Projects/SourceGraph.ns
selfhost/contracts/source-graph.contract.json
selfhost/bootstrap/verify_source_graph.py
cmake/VerifyNativeSharpSourceGraph.cmake
docs/2.0/self-hosting/tandas/TANDA-27-SH25-GRAFO-FUENTE.md
tandas_claude_commit/TANDA-27-SH25.md
```

**Modificados (2):**

```
selfhost/nsc/Driver/Program.ns
CMakeLists.txt
```

Comando:

```bat
git add selfhost/nsc/Projects/SourceGraph.ns ^
        selfhost/nsc/Driver/Program.ns ^
        selfhost/contracts/source-graph.contract.json ^
        selfhost/bootstrap/verify_source_graph.py ^
        cmake/VerifyNativeSharpSourceGraph.cmake ^
        CMakeLists.txt ^
        docs/2.0/self-hosting/tandas/TANDA-27-SH25-GRAFO-FUENTE.md ^
        tandas_claude_commit/TANDA-27-SH25.md
```

---

## 2. Lo que NO entra

* nada bajo `selfhost/out/`
* nada bajo `selfhost/evidence/runs/`
* objetos, ejecutables y temporales: `*.obj`, `*.exe`, `*.lib`, `*.rsp`, `*.log`, `*.manifest`
* cualquier archivo ajeno a los ocho de arriba

**`MANIFEST_SHA256.txt`: comprobar, no asumir.** Su marca de tiempo en disco es anterior al cierre
de SH24, así que aparentemente **no** se regenera por tanda. No verificado. Si el baseline lo
reescribe, entra; si no lo toca, no.

---

## 3. Evidencia — entra DESPUÉS de que pase el baseline

```
selfhost/evidence/gates/SH25/gate.json
```

Más lo que genere la corrida bajo ese directorio, siguiendo la convención de SH24. Si el baseline no
se ha corrido, este bloque no existe y el commit va sin él.

---

## 4. Título del commit

```
selfhost(tanda-27): expand the deterministic source graph
```

---

## 5. Cuerpo del commit

```
selfhost(tanda-27): expand the deterministic source graph

SH25 replaces single-file project compilation with the full source graph.

ProjectLoader.FirstSourcePath returned the alphabetically first .ns file in the
whole tree, and ProjectLoader.Verify skipped any include containing '**' or
'..'. Globs were therefore never expanded and --emit-project-object compiled
one loose file.

SourceGraph.ns adds a layer over ProjectModel rather than rewriting it: include
expansion (literal, dir/*.ext, dir/**/*.ext), transitive ProjectReference
expansion with cycle detection, and a canonical order. ProjectModel,
ProjectDumper, MetadataContract, ProjectCache and ToolchainManifest are
untouched, because project-driver.contract.json pins expected_sources on the
raw include dump and a rewrite would have moved a gate that is already closed.

Canonical order, which is what the gate measures:
  1. referenced projects contribute before the project that references them;
  2. within a project, includes contribute in declaration order;
  3. within one include, files are ordered by ordinal path comparison;
  4. a file reached by two includes is kept once, at its first occurrence.

Each SourceUnit stores its path relative to its owning project directory, not
to the root project's. A referenced library lives outside the root tree, so a
root-relative spelling would drag an absolute prefix into the dump that SH44
compares across machines.

An include that matches no file is an error, not an empty expansion: an include
that silently contributes nothing is how a compilation loses a file unnoticed.

The ordering is written as an explicit selection over the name array even
though List<T>.Sort() exists, because the gate needs the order to be provably
ordinal and Sort()'s comparison semantics for string were not verified.

Driver: --dump-source-graph and --check-project. The latter drives every unit
through the existing per-file front end with its own FileId and reports
diagnostics per unit. Cross-file symbol resolution stays in SH26.

The root project now holds 22 units, not the 21 the roadmap quotes: this change
adds Projects/SourceGraph.ns. The contract pins those 22 by owner and
deliberately does not pin the total, which would include library units that
were never counted.

Gate: nsc.selfhost.source-graph, O0-O3, dumps compared for byte identity.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01KpTyhrh4QaKhjRJzUbmQsw
```

---

## 6. Cómo probar ESTOS cambios

Esta tanda **no toca C++**: sólo `.ns`, Python, CMake y documentación. `nsc0` no necesita
recompilarse; CMake sí necesita reconfigurar para dar de alta el test nuevo.

### Paso 1 — humo, y es el que más informa

Es más rápido que `ctest` y enseña la salida real en vez de un PASS/FAIL:

```bat
nsc.exe --emit-project-oop-exe A:\nsc-devel\selfhost\nsc\NativeSharp.Compiler.nsproj -O0 -g0 -o A:\nsc-devel\selfhost\out\t27\nsc-next-o0.exe
A:\nsc-devel\selfhost\out\t27\nsc-next-o0.exe --dump-source-graph A:\nsc-devel\selfhost\nsc\NativeSharp.Compiler.nsproj
```

Si el primer comando falla, es un error de compilación en `SourceGraph.ns` o en `Program.ns` y ahí
se acaba la prueba: manda la salida del compilador.

Forma esperada de la salida del segundo:

```
source-graph schema=1
status=ok
projects=5
units=<total, incluye las bibliotecas>
unit 0 NativeSharpCore <ruta relativa>
...
unit N NativeSharpCompilerNext Basic/GenerationArena.ns
unit N+1 NativeSharpCompilerNext Basic/Identifiers.ns
...
unit N+21 NativeSharpCompilerNext Driver/Program.ns
```

Tres cosas que se ven de un vistazo y que son el objeto de la tanda:

1. `projects=5`, y el primer propietario es `NativeSharpCore` — el post-orden funciona.
2. Las 22 unidades de `NativeSharpCompilerNext` van al final, empezando por `Basic/` y terminando
   en `Driver/Program.ns` — el orden de declaración de los includes se respeta.
3. Ninguna ruta lleva `A:/` ni letra de unidad — son relativas a su proyecto propietario.

### Paso 2 — el recorrido por unidad

```bat
A:\nsc-devel\selfhost\out\t27\nsc-next-o0.exe --check-project A:\nsc-devel\selfhost\nsc\NativeSharp.Compiler.nsproj
```

Una línea por cada unidad raíz y un resumen
`graph-units=<N> checked=22 failed=0`. Código de salida 0. Las unidades referenciadas forman parte
del grafo. T27 sólo cuenta errores de lexer/parser en las unidades raíz: `SemanticAnalyzer` no
produce diagnósticos semánticos todavía, y SH26 debe diseñarlos antes de certificar resolución.

Si alguna unidad trae `errors=` distinto de cero, **eso no es necesariamente un fallo de esta
tanda**: el roadmap afirma que las 21 unidades pasan individualmente, y esta tanda asume esa
afirmación sin haberla comprobado. Manda las líneas con error y se separa una causa de la otra.

### Paso 3 — el gate

```bat
cmake -S A:\nsc-devel -B A:\nsc-devel\selfhost\out\nsc0-build
ctest --test-dir A:\nsc-devel\selfhost\out\nsc0-build -R nsc.selfhost.source-graph -V
```

Comprueba lo que el humo no comprueba: que los cuatro volcados O0–O3 sean **byte a byte idénticos**
y que el orden coincida exactamente con el contrato.

### Paso 4 — baseline completo acumulado

Se ejecutará al terminar todas las tandas entregadas por Claude. En T27 se exige compilación y gate
dirigido; el expediente no puede declarar todavía que el baseline completo pasó.

---

## 7. Si falla

Manda la salida **completa**, no la línea de resumen. En concreto:

| Dónde falló | Qué mandar |
|---|---|
| Paso 1, compilación | la salida entera de `nsc.exe`, con todos los errores |
| Paso 1, volcado | el texto completo del volcado, aunque diga `status=failed` |
| Paso 2 | las líneas `unit ... errors=N` que no sean cero, y el resumen |
| Paso 3 | la salida de `ctest -V` entera, incluido lo que imprima el verificador Python |

El verificador nombra la comparación que falló (`O2 dump differs from O0`, `root unit list is [...]`,
`project order is [...]`), así que esa línea sola ya localiza el defecto.

---

## 8. Antes de commitear

* [x] paso 1 en verde: compila y el volcado tiene la forma esperada
* [x] paso 2 en verde: `graph-units=92 checked=22 failed=0`
* [x] paso 3 en verde: `ctest -R nsc.selfhost.source-graph` (10.69 s)
* [ ] paso 4 diferido: baseline completo acumulado al terminar las tandas de Claude
* [ ] `git status` limpio salvo los ocho archivos de §1
* [ ] el expediente `TANDA-27-SH25-GRAFO-FUENTE.md` pasa de `WRITTEN` a `LANDED`, con las casillas
  de su §8 marcadas y los números reales escritos en su §6 y §7
