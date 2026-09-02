# Stage 53.4 "Filesystem/Paths (cierre)" — implementación paso a paso

## Paso 0 — Confirmar la base y las plantillas exactas a copiar

Antes de escribir una sola línea se releyeron, desde disco (no de memoria — el contexto de la sesión anterior se había compactado):

- `lib/NativeSharp.Core.IO/Directory.ns` y `File.ns` — para el patrón exacto de `Delete`.
- `lib/NativeSharp.Core.IO/Path.ns` — para confirmar que, hasta esta ronda, no tenía ninguna operación núcleo.
- `lib/NativeSharp.Core/Environment.ns` — para el patrón exacto de `GetEnvironmentVariable` (la plantilla de dos llamadas).
- `src/Runtime/WindowsRuntimeBuilder.cpp`: `buildFileDelete` (líneas 776–806 antes de esta ronda) y `buildEnvironmentGetVariable` (líneas 554–638) — las dos plantillas reales, línea por línea.
- Las 4 capas del compilador (`ProjectMethodBodyModel.h/.cpp`, `ProjectOopHirModel.h/.cpp`, `ProjectOopMirModel.h/.cpp`, `ProjectOopLlvmLowerer.cpp`) en su estado post-53.3b, para ubicar exactamente dónde insertar cada pieza nueva sin duplicar ningún bloque existente.
- El array `kernel32Symbols` (25 entradas, duplicado en 5 archivos) — punto de partida antes de agregar `RemoveDirectoryA`/`GetFullPathNameA`.

## Paso 1 — Extender las 4 capas del compilador + runtime

`Directory.Delete` se agregó como una rama `else if (!isFile && selectedMember.name == "Delete" ...)` dentro del `if` compartido de `NativeSharp.Core.IO.File`/`Directory` en `ProjectMethodBodyModel.cpp` — el mismo `if` que ya reconocía `Exists`/`CreateDirectory`. `Path.GetFullPath` necesitó su **propio** `if` nuevo, porque `Path` es una tercera clase, y todos sus otros métodos (`Combine`, `GetFileName`, etc.) deben seguir sin pasar por ningún reconocimiento del compilador.

En las capas HIR/MIR, ambas se sumaron al bucket compartido de "string intrinsics de un operando" (`expected = 1U`) que ya usan `FileExists`/`FileDelete`/`EnvironmentGetVariable`/`FileReadAllText` — ninguna decisión de forma nueva, solo dos entradas más en listas ya existentes.

En `ProjectOopLlvmLowerer.cpp`: dos símbolos nuevos (`__nativesharp_directory_delete`, `__nativesharp_path_get_full_path`), dos pares de campos `LLVMValueRef`/`LLVMTypeRef`, dos entradas en `declareRuntime()` (`ptr(ptr)` para `DirectoryDelete`→void con retorno dummy, `ptr(ptr)`→`ptr` para `PathGetFullPath`), y dos `case` de lowering, calcados de `CoreFileDelete`/`CoreFileReadAllText` respectivamente.

En `WindowsRuntimeBuilder.cpp`: `buildDirectoryDelete` se insertó justo después de `buildDirectoryCreate`, copiando `buildFileDelete` casi sin cambios (solo el nombre de función Win32). `buildPathGetFullPath` se insertó justo después de `buildFileAppendAllText`, copiando `buildEnvironmentGetVariable` con la firma de cuatro parámetros de `GetFullPathNameA` (el parámetro extra `lpFilePart`, siempre `NULL`).

Whitelist de `kernel32Symbols`: `RemoveDirectoryA` y `GetFullPathNameA` agregadas en los 5 archivos (`src/Linker/WindowsLibraryBuilder.cpp`, `src/Linker/WindowsExecutableBuilder.cpp`, y los tres archivos de test de host-ABI), array `25 -> 27` en los cinco.

## Paso 2 — Primera compilación completa del compilador: sin errores

```
$ cmake --build . --target nsc -j$(nproc)
...
[100%] Linking CXX executable bin/nsc
[100%] Built target nsc
```

Compiló limpio en el primer intento — a diferencia de rondas anteriores, esta vez no hubo ningún error de tipos ni de firma al copiar las plantillas, porque el patrón ya estaba tan probado que no dejaba margen de error mecánico.

## Paso 3 — Escribir `Directory.ns`/`Path.ns` y el ejemplo empaquetado; probe manual bajo Wine

Se agregó `Directory.Delete` a `Directory.ns` y `Path.GetFullPath` a `Path.ns`, con comentarios de cabecera actualizados (el de `Path.ns` necesitó reescritura sustancial: ya no era cierto que la clase fuera 100% NativeSharp puro).

Se compiló el ejemplo empaquetado completo (`examples/project_stage53_4/Main.ns`, primera versión, con `Path.GetFullPath("")` como forma de pedir el directorio de trabajo actual):

```
$ nsc --emit-project-oop-exe examples/project_stage53_4/Stage53_4FilesystemPathsCierre.nsproj -O0 -o probe534_o0.exe
project OOP PE executable emission: ok
...
$ cd probe534_run && wine probe534_o0.exe; echo "EXIT_CODE=$?"
EXIT_CODE=6
```

Código de salida `6` — el chequeo 6 de `Main.ns` (`cwd.Length == 0`) falló, es decir, `Path.GetFullPath("")` devolvió un string vacío. Los chequeos 1–5 (todos los de `Directory.Delete`) habían pasado sin problema.

## Paso 4 — Aislar la sorpresa: un probe de una sola línea

Para confirmar que el fallo era realmente `Path.GetFullPath("")` y no algún efecto colateral del resto del programa, se escribió un probe mínimo, aislado, en un proyecto separado:

```csharp
using NativeSharp.Core.IO;

class Program
{
    public static int Main()
    {
        string cwd = Path.GetFullPath("");
        return cwd.Length;
    }
}
```

```
$ nsc --emit-project-oop-exe probe.nsproj -O0 -o probe_gfp.exe
$ wine probe_gfp.exe; echo "EXIT_CODE=$?"
EXIT_CODE=0
```

Código de salida `0` = longitud del string devuelto. Confirmado: `GetFullPathNameA("")` bajo Wine devuelve `0` (fallo), no el tamaño del directorio de trabajo actual — la intuición inicial (que `""` sería sinónimo de `.` para esta función) era incorrecta. Se probó de inmediato la alternativa:

```csharp
public static int Main()
{
    string cwd = Path.GetFullPath(".");
    if (cwd.Length == 0) { return 1; }
    string cwdAgain = Path.GetFullPath(cwd);
    if (cwdAgain.Length != cwd.Length) { return 2; }
    if (!cwdAgain.Equals(cwd)) { return 3; }
    string empty = Path.GetFullPath("");
    if (empty.Length != 0) { return 4; }
    return 0;
}
```

```
$ nsc --emit-project-oop-exe probe.nsproj -O0 -o probe_gfp.exe
$ wine probe_gfp.exe; echo "EXIT_CODE=$?"
EXIT_CODE=0
```

Los cuatro chequeos pasan: `"."` sí resuelve al directorio de trabajo actual, es idempotente sobre su propio resultado, y `""` sigue devolviendo `""`. Con esto confirmado, se corrigieron TRES lugares que habían heredado la suposición incorrecta original:

1. El comentario de cabecera de `Path.GetFullPath` en `Path.ns`.
2. El comentario de cabecera de `buildPathGetFullPath` en `WindowsRuntimeBuilder.cpp`.
3. `Main.ns` del ejemplo empaquetado: el chequeo de "directorio de trabajo actual" pasó a usar `"."` en vez de `""`, y se agregó un chequeo nuevo y explícito (`Path.GetFullPath("")` debe dar `""`) para no perder la cobertura del caso real, ahora documentado correctamente en vez de mal-asumido.

## Paso 5 — Recompilar y re-verificar el ejemplo empaquetado completo, en ambos niveles

```
$ nsc --emit-project-oop-exe .../Stage53_4FilesystemPathsCierre.nsproj -O0 -o probe534_o0.exe
$ nsc --emit-project-oop-exe .../Stage53_4FilesystemPathsCierre.nsproj -O2 -o probe534_o2.exe
$ cd probe534_run_o0 && wine probe534_o0.exe; echo "O0 EXIT_CODE=$?"
O0 EXIT_CODE=0
$ cd probe534_run_o2 && wine probe534_o2.exe; echo "O2 EXIT_CODE=$?"
O2 EXIT_CODE=0
```

Los 14 chequeos pasan en ambos niveles. Se repitió la corrida (directorios de trabajo limpiados primero) para confirmar estabilidad — mismo resultado, y ambos directorios de trabajo quedaron completamente vacíos al terminar (confirmación independiente, desde el lado Linux, de que `Directory.Delete`/`File.Delete` realmente borraron todo lo que el programa creó).

## Paso 6 — Test dedicado: fixture de lowering + programa empaquetado en O0/O2

Se escribió `tests/unit/Stage53_4FilesystemPathsCierreTests.cpp`, mismo esqueleto que `Stage53_3bFilesystemPathsContentTests.cpp`: un fixture mínimo que confirma que `Directory.Delete`/`Path.GetFullPath` emiten la llamada exacta a su símbolo de runtime, más el ejemplo empaquetado completo corrido bajo Wine en O0 y O2 con un directorio de trabajo dedicado. La única diferencia estructural frente al harness de 53.3b: en vez de confirmar el contenido final de un archivo dejado atrás, este harness confirma que el directorio de trabajo queda **completamente vacío** después de la corrida — el testigo independiente correcto para una ronda que crea y borra todo lo que usa.

Registrado en `CMakeLists.txt` como `nsc_stage53_4_filesystem_paths_cierre_tests` / `nsc.stage53-4.filesystem-paths-cierre`, siguiendo el mismo bloque que el target de 53.3b.

```
$ cmake . && cmake --build . --target nsc_stage53_4_filesystem_paths_cierre_tests -j$(nproc)
...
[100%] Built target nsc_stage53_4_filesystem_paths_cierre_tests
$ ./tests/nsc_stage53_4_filesystem_paths_cierre_tests
PASS: 43 checks
$ ./tests/nsc_stage53_4_filesystem_paths_cierre_tests
PASS: 43 checks
```

Estable en dos corridas.

## Paso 7 — Build completo y `ctest` completo, dos veces

```
$ cmake --build . -j$(nproc)
... (build completo, exit 0)
$ ctest --output-on-failure -j$(nproc)
...
216/216 Test  #78: nsc.stage53-4.filesystem-paths-cierre .............   Passed    5.89 sec
100% tests passed, 0 tests failed out of 216
$ ctest --output-on-failure -j$(nproc)
...
100% tests passed, 0 tests failed out of 216
```

216/216 (215 previos + 1 nuevo), estable en dos corridas consecutivas.

## Cierre

Cero bugs de compilador encontrados en esta ronda. El único "incidente" real fue una suposición semántica incorrecta sobre el comportamiento de `GetFullPathNameA("")` — detectada, aislada con un probe de una sola línea, y corregida en los tres lugares (código de comentarios, `Path.ns`, `Main.ns`) **antes** de empaquetar la entrega final, siguiendo la misma disciplina de "verificar antes de documentar" que este proyecto viene aplicando desde 53.2b. `Directory.Delete` no tuvo ninguna sorpresa: se comportó exactamente como su plantilla (`File.Delete`) desde el primer intento.
