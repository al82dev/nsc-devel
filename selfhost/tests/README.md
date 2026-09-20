# Pruebas de self-hosting

`profile/positive` contiene código perteneciente al Bootstrap Profile 2.0 que `nsc0` debe compilar
y ejecutar. `profile/negative` contiene construcciones válidas o reconocibles de NativeSharp 2.0
que el perfil deliberadamente rechaza antes del bootstrap; `cases.json` fija sus códigos `NSBP`.

`host/CompilerHostWorkload` es la carga certificada de SH02. Lee el corpus hostil de `host/corpus`,
usa vistas sin copia, colecciones, hashing, orden estable, IO binario, procesos y una arena con
contadores; se ejecuta dos veces por linker y exige salida idéntica, retorno 42 y cero obligaciones
de arena después de `Reset()`.

`llvm/EmitMinimalObject` es la carga SH03: usa exclusivamente `NativeSharp.LLVM.C` para crear,
verificar, optimizar y emitir un COFF x64 que llama `ExitProcess(42)`. `llvm/LinkerAdapterSmoke`
enlaza ese objeto por response file con `lld-link` y NSLink, incluso bajo rutas con espacios.
`llvm/CompilerServicesSmoke` certifica filesystem, proceso, reloj y archivo mapeado a través de
`NativeSharp.Win32.Compiler`. Los tres proyectos se compilan O0 bajo el Bootstrap Profile.

El gate SH04 compila `selfhost/nsc/NativeSharp.Compiler.nsproj` en O0 y O2, ejecuta el self-test de
Basic/Source/Diagnostics, verifica ambos PE y compara contra `nsc0` los diagnósticos de archivo
ausente y opción desconocida. También exige resultados idénticos con rutas con espacios, distintos
directorios de ejecución y finales CRLF, CR y LF.

Las corridas generadas nunca se escriben aquí: viven bajo `selfhost/out` o
`selfhost/evidence/runs`.

- `bootstrap/`: cadena `nsc0 → nsc1 → nsc2 → nsc3` y aislamiento de procesos.
- `differential/`: comparaciones por tokens, sintaxis, semántica, IR, ABI y ejecución.
- `fixtures/`: corpus exclusivo del compilador autoalojado.

Las pruebas normales de NSC siguen viviendo en `tests/`. Este árbol sólo contiene pruebas cuya
razón de existir es el self-hosting.

SH10 reconstruye `nsc1` desde dos copias limpias del conjunto congelado y usa el propio `nsc1`
para verificar los 20 archivos fuente del compilador y producir la matriz ejecutable O0–O3.

SH11 ejecuta `nsc1 → nsc2` bajo observación de procesos, en dos raíces y dos entornos regionales;
después compara frontend, diagnósticos, metadata y objetos y ejecuta O0–O3 por `lld-link` y NSLink.

SH12 repite la transición a `nsc3` en clean-a/clean-b/clean-c, conserva tres ambientes regionales,
compara imágenes/objetos y genera la atestación que debe trasladarse a una segunda máquina.
