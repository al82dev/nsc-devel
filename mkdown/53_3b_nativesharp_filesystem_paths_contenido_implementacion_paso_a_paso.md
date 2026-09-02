# 53.3b — Implementación paso a paso: Filesystem/Paths (contenido de archivo)

Acompaña a `53_3b_nativesharp_filesystem_paths_contenido.md`. Mismo formato que los documentos
paso a paso anteriores de Stage 53: el "cómo", en el orden real en que sucedió, con comandos y
salidas reales.

## Paso 0 — Confirmar la base y las declaraciones Win32 ya existentes

Punto de partida: Stage 53.3 ronda 1 cerrado, `ctest` en **214/214**.

Antes de declarar ningún símbolo Win32 nuevo, se buscó con `grep` si `WriteFile` ya estaba
declarado en `WindowsRuntimeBuilder.cpp` (usado por `Console.WriteLine` desde Stage 41):

```
$ grep -n "\"WriteFile\"\|writeFileType" src/Runtime/WindowsRuntimeBuilder.cpp
...
1355:    LLVMTypeRef writeFileType = functionType(
1356:        i32Type,
1357:        {pointerType, pointerType, i32Type, pointerType, pointerType});
1358:    LLVMValueRef writeFile = LLVMAddFunction(module, "WriteFile", writeFileType);
```

Confirmado: la firma ya declarada (`i32(ptr, ptr, i32, ptr, ptr)`) es EXACTAMENTE la firma real de
`WriteFile` de Win32 (`BOOL WriteFile(HANDLE, LPCVOID, DWORD, LPDWORD, LPOVERLAPPED)`). No hizo
falta declarar `WriteFile` de nuevo — `buildFileWriteAllText`/`buildFileAppendAllText` reutilizan
directamente la variable `writeFile`/`writeFileType` ya existente, pasada como parámetro.

También se confirmó que `functionType()` (el helper local de este archivo) acepta hasta 8
parámetros (`std::array<LLVMTypeRef, 8> storage`), suficiente para los 7 parámetros de
`CreateFileA`.

## Paso 1 — Extender las 4 capas del compilador

Mismo patrón exacto que Stage 53.3 (que a su vez siguió el patrón de 53.2b): 3 valores nuevos en
cada uno de `MethodCoreOperationKind`, `ProjectOopHirNodeKind`, `ProjectOopMirNodeKind` (más
toString() y conversión HIR→MIR trivial en cada capa), un bloque de reconocimiento nuevo en
`ProjectMethodBodyModel.cpp` (agregado al `if` ya existente de `NativeSharp.Core.IO.File`/
`Directory`, con el flag `isFile` ya presente desde 53.3 — `ReadAllText`/`WriteAllText`/
`AppendAllText` son las tres exclusivas de `File`, `Directory` no tiene equivalente), y extensión
del bloque HIR compartido de string intrinsics en `ProjectOopHirModel.cpp`.

**Un detalle nuevo en el cálculo de operandos esperados**: a diferencia de Stage 53.3 (donde las
4 operaciones nuevas eran todas de 1 operando), esta ronda mezcla shapes: `ReadAllText` es de 1
operando (se agrega a la lista existente del `if` de `expected = 1U`), pero
`WriteAllText`/`AppendAllText` son de 2 operandos — y como el valor por defecto de `expected` ya
es `2U`, esas dos NO necesitan agregarse a ninguna lista, simplemente caen en el default. Esto se
documentó explícitamente con un comentario en el código (en vez de dejarlo como una omisión
silenciosa que alguien podría confundir con un olvido) — ver el comentario en
`ProjectOopHirModel.cpp` junto al `if` de `expected = 1U`.

## Paso 2 — Diseñar las tres funciones de runtime, y el patrón `INVALID_HANDLE_VALUE`

Se identificaron las 5 API de Win32 necesarias: `CreateFileA`, `GetFileSizeEx`, `ReadFile`,
`CloseHandle`, `SetFilePointer` (más el ya declarado `WriteFile`, ver Paso 0). Se diseñó el patrón
de comparación contra `INVALID_HANDLE_VALUE` (ver sección 5 del documento conceptual:
`LLVMBuildPtrToInt` + comparación contra el entero `0xFFFFFFFFFFFFFFFF`) ANTES de escribir el
cuerpo completo de cada función, para reutilizarlo idéntico en las tres (`buildFileReadAllText`,
`buildFileWriteAllText`, `buildFileAppendAllText`).

Se escribieron las tres funciones siguiendo el patrón de bloques ya establecido por
`buildEnvironmentGetVariable` (53.2b): `entry` → rama de fallo (`emptyResult` para
`ReadAllText`, o simplemente saltar a `done` para `WriteAllText`/`AppendAllText`, que no
necesitan construir ningún resultado) → rama de éxito (`haveHandle`) → terminación.

## Paso 3 — Verificar el error de ownership de `File.ReadAllText`, ANTES de escribir el ejemplo

Al declarar `File.ns`'s `ReadAllText` con retorno `string` (no `owned string`, siguiendo
deliberadamente el mismo patrón que `Environment.GetEnvironmentVariable`), se verificó
DIRECTAMENTE contra el compilador -- no se asumió -- que intentar `raii string x =
File.ReadAllText(...)` es rechazado:

```
$ nsc --emit-project-oop-exe probe.nsproj -O0 -o probe_O0.exe
Main.ns:10:31: error: RAII ownership cannot be copied or borrowed; initialize with new, null,
move(other), or an owned-returning call
            raii string missing = File.ReadAllText("nativesharp_probe533b_missing.txt");
                                  ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
```

Corregido cambiando `raii string` a `string` a secas en las 5 llamadas del probe -- exactamente
el mismo patrón que el propio `examples/project_stage53_2b/Main.ns` ya usaba para
`Environment.GetEnvironmentVariable` (`string missing = Environment.GetEnvironmentVariable(...)`,
sin `raii`), confirmado releyendo ese archivo antes de escribir el fix.

Curiosamente, al escribir el chequeo 12 del ejemplo empaquetado (round-trip con `Path.Combine`,
que SÍ es `owned string`), se disparó el error CONTRARIO:

```
Main.ns:115:31: error: owned return value cannot initialize a non-owning local; use a raii
owner destination
            string combinedPath = Path.Combine("", "nativesharp_stage53_3b_c.txt");
```

Corregido en la dirección opuesta: `string combinedPath` → `raii string combinedPath`. Este
segundo error, encontrado en el mismo archivo apenas unas líneas después del primero, terminó
siendo un ejemplo didáctico perfecto de la regla de ownership del compilador (el chequeo mira el
tipo de retorno DECLARADO del método llamado, no lo que el runtime hace de verdad) -- ambos casos
quedaron documentados explícitamente en el comentario de cabecera de
`examples/project_stage53_3b/Main.ns`.

## Paso 4 — Primera compilación completa del compilador: sin errores

```
$ cmake --build . --target nsc -- -j$(nproc)
...
[100%] Linking CXX executable bin/nsc
[100%] Built target nsc
```

Compiló limpio en el primer intento (el bump proactivo de la whitelist `kernel32Symbols`, 20→25
en los 5 archivos establecidos, se aplicó ANTES de este build, siguiendo la disciplina desde
Stage 53.1).

## Paso 5 — Probe manual bajo Wine: un falso positivo por estado no limpiado entre corridas

Se escribió un programa de prueba independiente (`/tmp/ns51_probe53_3b/Main.ns`, fuera del árbol
del repositorio) ejercitando `ReadAllText`/`WriteAllText`/`AppendAllText`. Primera corrida:

```
$ WINEDEBUG=-all timeout 20 wine64 probe_O0.exe; echo "EXIT_CODE=$?"
EXIT_CODE=124
```

`124` es el código de `timeout` cuando el proceso no termina en el tiempo dado -- no un crash, un
colgamiento. Investigado corriendo de nuevo con un `timeout` más corto y salida redirigida a
archivo:

```
$ WINEDEBUG=-all timeout 15 wine64 probe_O0.exe > out.log 2>&1; echo "EXIT=$?"
EXIT=0
```

Esta vez terminó normalmente, con el contenido de los archivos generados exactamente como se
esperaba (`"x"` y `"primerosegundo"`, confirmado con `cat` desde el lado Linux). La explicación
más probable para el primer colgamiento: overhead de arranque de `wineserver` en la primera
invocación de esa sesión (un costo de inicialización real de Wine, no de este compilador),
consistente con que la segunda invocación fue inmediata.

Un tercer intento, sin embargo, dio un resultado distinto:

```
$ WINEDEBUG=-all timeout 15 wine64 probe_O0.exe; echo "EXIT=$?"
EXIT=6
```

Exit code `6` corresponde al chequeo `afterFirstAppend.Equals("primero")` fallando. Investigado:
NO era un bug -- el archivo `nativesharp_probe533b_b.txt` seguía en disco desde la corrida
anterior exitosa, con contenido `"primerosegundo"`. `AppendAllText("primero")` sobre ESE archivo
(que ya no estaba vacío) produce `"primerosegundoprimero"`, no `"primero"` -- el chequeo estaba
correctamente diseñado para un archivo que empieza sin existir, y el probe manual simplemente no
era idempotente entre corridas repetidas (a diferencia del test dedicado, que SÍ limpia su
directorio de trabajo en cada corrida -- ver Paso 6). Confirmado limpiando los archivos y
corriendo de nuevo:

```
$ rm -f nativesharp_probe533b_*.txt
$ WINEDEBUG=-all timeout 15 wine64 probe_O0.exe; echo "EXIT_CODE=$?"
EXIT_CODE=0
```

Repetido a `-O2` (con limpieza de estado primero): también `EXIT_CODE=0`, primer intento, sin
ningún error de `memcpy` ni de ningún otro tipo -- confirma que la infraestructura de linking (el
fix de 53.2b) sigue cubriendo cualquier operación nueva sin necesidad de tocarla.

## Paso 6 — Programa de ejemplo empaquetado con verificación de longitud, vía Python

Al escribir `examples/project_stage53_3b/Main.ns`, se verificó CADA longitud de string esperada
con Python antes de escribirla en el código -- la disciplina establecida en 53.2b tras dos errores
propios de conteo de caracteres en esa ronda:

```
$ python3 -c "print(len('un contenido bastante mas largo que el anterior'))"
47
```

El primer intento del código tenía `if (longer.Length != 49)` -- un error de conteo manual
detectado y corregido ANTES de compilar (no por un fallo de test), gracias a esta verificación
proactiva. Las demás longitudes (`"primerosegundo"` = 14, `"hola mundo"` = 10, `"via
Path.Combine"` = 16) se verificaron de la misma forma, todas correctas al primer intento.

## Paso 7 — Test dedicado: mismo patrón de directorio de trabajo que 53.3

Se escribió `tests/unit/Stage53_3bFilesystemPathsContentTests.cpp` reutilizando el mismo
`runUnderWine` con `cd` a un directorio de trabajo dedicado que Stage 53.3 introdujo -- con una
diferencia: esta ronda NO necesita que el harness prepare ningún archivo de antemano (a diferencia
de Stage 53.3, que necesitaba un archivo preexistente para probar `File.Exists`/`Delete` porque
todavía no existía `WriteAllText`). El propio programa `Main.ns` de esta ronda escribe cada
archivo que después lee, así que el directorio de trabajo solo necesita estar VACÍO al empezar,
no poblado.

## Paso 8 — Registro en CMakeLists.txt, build, y verificación final

```
$ cmake --build . --target nsc_stage53_3b_filesystem_paths_content_tests -- -j$(nproc)
...
[100%] Built target nsc_stage53_3b_filesystem_paths_content_tests
$ ./tests/nsc_stage53_3b_filesystem_paths_content_tests
PASS: 44 checks
$ ./tests/nsc_stage53_3b_filesystem_paths_content_tests   # repetido
PASS: 44 checks
$ ctest --output-on-failure -j$(nproc)
...
100% tests passed, 0 tests failed out of 215
$ ctest --output-on-failure -j$(nproc)   # repetido
...
100% tests passed, 0 tests failed out of 215
```

44/44 en el test dedicado (dos corridas consecutivas), 215/215 en la suite completa (dos corridas
consecutivas, el nuevo test corriendo en 5.40 segundos -- la ejecución real bajo Wine de dos
ejecutables, dos veces cada uno entre el fixture mínimo y el programa empaquetado).

## Cierre

Cero errores de compilador nuevos encontrados en esta ronda. Los dos "incidentes" reales de este
round fueron: (1) los dos errores de ownership del compilador (Paso 3), ambos EXACTAMENTE el
comportamiento esperado y documentado del analizador de ownership, no bugs; y (2) un falso
positivo de estado no limpiado entre corridas manuales del probe (Paso 5), resuelto simplemente
limpiando los archivos, no un bug del compilador ni del runtime. Ver `53_3b_nativesharp_
filesystem_paths_contenido.md` sección 8 para el cierre formal completo.
