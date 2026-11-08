# SCOPE LOCK: T08-010, T08-011, T08-012 y T08-020 (carril Claude, layout de struct y union C)

## Objetivo autorizado

- Revisar cómo NSC importa de Clang tamaño, alineación y offset de campos de `struct` y `union` C.
- Distinguir lo implementado y probado de lo preparado sin construir o sin ejecutar.
- Comprobar que los valores de NativeSharp coinciden con Clang en campos empaquetados, anidados y de bits.
- Corregir únicamente defectos demostrados con evidencia, dentro de representación y layout.
- Pruebas dirigidas de tamaño, alineación, offsets y lectura/escritura cruzada C/NativeSharp.
- Un commit por cambio o ítem, cada uno con su Markdown.

## Naturaleza

- Correctiva y de validación funcional dentro de interop C. No es optimización ni refactor.

## Revisión, rama y rutas

- Revisión inicial: `ceac3f83b4db627920c937fd1dd2ea3d4dd95908`.
- Worktree: `A:\nsc-claude-t08-layout`, rama `interop-t08-claude-layout`.
- Build, temporales, logs y resultados: `A:\nsc-claude-t08-layout-build`.
- Toolchain única: `A:\al82dev-clang` (23.1.0) para NSC y para el C de referencia.
- `nslink.exe` existente de `A:\nsc-devel\build-clang\bin`: sólo ejecución, sin modificar ni reconstruir.

## Comportamiento que debe permanecer idéntico

- Todo programa NativeSharp que no importa records C.
- Ruta C++ (records, unions y campos C++ siguen con sus barreras actuales).
- Campos C ya proyectados y probados: mismos offsets, tamaños, alineaciones IR y diagnósticos.
- Formato y contenido de NSMETA y del catálogo C de metadata.

## Archivos que pueden modificarse (sólo si la evidencia lo exige)

- `src/Interop/Cpp/ClangSession.cpp` (captura de layout de records).
- `src/Interop/Cpp/CppSemanticRecord.cpp` (proyección de campos).
- `include/nativesharp/Interop/CppRecordDeclaration.h` (sólo campos opcionales en memoria, sin persistir).
- `src/Frontend/.../MemberAccessAnalyzer.cpp` y unidades LLVM de acceso a campo, sólo con cadena causal demostrada.
- Fixture nuevo: `nativesharp-interop/pruebas-tanda-08-layout-clang/` (archivos nuevos).
- `tests/unit/CppClangSessionTests.cpp` (compartido con Codex; se avisará cada cambio).
- `nativesharp-interop/registro-commits/` (Markdown nuevos), `TRACKING.md` (líneas de T08-010/011/012/020).

## Archivos y superficies protegidos

- NSLink, código fuente de LLVM/Clang, `A:\al82dev-clang`.
- `src/Interop/Internal/CppCRecordMetadataCodec.cpp` y todo formato NSMETA: sin autorización explícita.
- Fixtures y pruebas existentes (incluido `pruebas-tanda-08-layout`): no se editan; los casos nuevos van en
  archivos nuevos.
- Trabajo de identidad, TLS, globales, `_Bool`, volatile y caché de Codex.
- Carpeta `A:\nsc-devel` (sólo lectura del `nslink.exe` autorizado) y `A:\nsc-interop-build`.
- `SCOPE-LOCK-EJECUCION.md`: no se edita para evitar conflicto; este documento lo sustituye en esta rama.

## Acciones prohibidas

- Avanzar a la tanda 9 o declarar cerrada la tanda 8.
- Daikon, .NET, camino inverso C++ consumiendo NativeSharp.
- Escribir en `C:`; cambiar el `PATH` global; cambiar configuración, CMake o flags del producto.
- Modificar metadata protegida sin presentar antes el caso exacto y recibir autorización.
- Reconstruir todo o correr la batería completa por ítem; push; merge; tocar ramas o índice de Codex.
- Marcar ítems como completos con casos o evidencia faltantes.

## Pruebas que deben pasar sin modificaciones

- `pruebas-tanda-08-layout\validar.cmd` (copia adaptada sólo en rutas, en mi carpeta), O0–O3.
- Suite de sesión Clang (`CppClangSessionTests`) del build propio.

## Criterio de terminación

- Para cada caso del inventario: tamaño, alineación y offset de NSC iguales a Clang; alineación de cada
  acceso en IR igual a la de Clang o diferencia demostrada como segura y documentada; lectura y escritura
  cruzadas correctas en O0–O3; camino precompilado comprobado donde aplique.
- Cada diferencia: corregida con commit propio o presentada como caso exacto (entrada, actual, esperado,
  campos de metadata afectados).
- Informe con rama, commits, archivos, pruebas, resultados, faltantes y conflictos con Codex.

## Metodología

1. Build completo inicial en la carpeta propia (autorizado); después sólo incrementales.
2. Ejecutar la copia adaptada del validador de layout existente O0–O3 como línea base del build propio.
3. Resolver con esa ejecución la contradicción del tracking sobre arreglos de records empaquetados
   (`40a0062b8`, `84549e59f`, `35e9cf099`: "no construidos" en T08-010 frente a "pasaron" en T08-016).
4. Fixture nuevo `pruebas-tanda-08-layout-clang` con un caso por forma de layout:
   1. struct natural con padding interno y de cola;
   2. `pack(1)`, `pack(2)` y `pack(4)`;
   3. record natural anidado dentro de record empaquetado, dos niveles;
   4. record empaquetado anidado dentro de record natural;
   5. arreglos de records dentro de record empaquetado;
   6. `_Alignas` en campo y typedef con alineación;
   7. union natural, union empaquetada, union con struct anidado;
   8. bitfields en struct natural, empaquetado, con ancho cero y sin nombre, y en union;
   9. miembros anónimos C11 (struct y union);
   10. struct con nombre definido dentro de otro struct;
   11. miembro de arreglo flexible.
5. Oráculos por caso:
   1. `sizeof`, `alignof` y `offsetof` calculados en C contra los de NativeSharp;
   2. alineación de cada carga y escritura en el IR de Clang contra la del IR de NSC;
   3. escritura desde NativeSharp y lectura desde C, y al revés, O0–O3;
   4. productor precompilado y consumidor para los casos representables.
6. Clasificar cada diferencia: defecto de captura, de proyección, de acceso, de metadata o barrera
   intencional (campo no proyectado con diagnóstico).
7. Para cada defecto: cadena causal, cambio mínimo, build incremental, prueba dirigida, commit con Markdown.
8. Diferencias que requieran NSMETA: detener y presentar el caso exacto.
9. Validación agrupada final de las pruebas dirigidas, actualización del tracking sin cerrar ítems
   incompletos e informe.

## Registro de aprobación y enmiendas

- 2026-11-07: el usuario aprobó este SCOPE LOCK como base del trabajo, con estas precisiones: una sola cadena
  `A:\al82dev-clang`; build completo inicial permitido y después sólo incrementales; `nslink.exe` existente sólo
  ejecutado; copia del validador con todas sus rutas de salida aisladas, cambiando el `PATH` sólo dentro del
  proceso; NSMETA no se modifica sin presentar antes el caso exacto; no avanzar a la tanda 9 ni ejecutar la
  validación completa de integración, que hará Codex.
- La suite `nsc.cpp-interop.session` no se ejecutó en esta rama: espera fixtures preparados en
  `<raíz>/cpp-fixtures/project/CppProbe.nsproj`, que sólo existen bajo `A:\nsc-interop-build` (carpeta de Codex),
  como ya registra `TANDA-06-VALIDACION.md`. Esta rama no modifica código de producción.
