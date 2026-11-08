# T08-002: informe del carril de identidad C para revisión

Documento único para quien revise este carril en otra máquina. Reúne lo hecho,
los resultados, los parches y propuestas, las preguntas abiertas y la lista de
comprobaciones. Los detalles de cada tema están en los documentos que se
enlazan; aquí van las conclusiones y dónde verificarlas.

**Estado: T08-002 ABIERTO. La tanda 8 no está cerrada.** Nada de lo que sigue es
validación integral.

## 1. Instrucciones para el revisor

1. Lee primero las secciones 5 (hallazgos) y 7 (preguntas).
2. Recorre la lista de la sección 8. Cada punto dice qué afirmación comprobar y
   dónde: archivo y línea, o comando.
3. Separa en tu revisión lo comprobado, lo inferido y lo no verificado. Los
   puntos marcados **NO VERIFICADO** en este informe no se ejecutaron.
4. No integres, no hagas push ni merge, y no apliques los parches salvo que el
   usuario lo pida. El parche de H7 se entrega sin aplicar a propósito.
5. Si algo no cuadra con el código de la rama, anótalo como discrepancia con la
   evidencia. No lo corrijas en silencio.

## 2. Alcance y restricciones del carril

- Rama `interop-t08-claude-identidad`, base `ceac3f83b`, en `A:\nsc-devel` (laptop).
- Build aislado `A:\nsc-devel\build-claude-t08\build-ON`, con Clang oficial
  23.1.0 de `A:\al82dev-clang` para NSC y para el C de referencia. `nslink.exe` de
  `A:\nsc-devel\build-clang\bin` solo en lectura.
- Prohibido:
  - modificar NSLink, LLVM/Clang externo o el formato NSMETA, o desplegar en C:;
  - avanzar a la tanda 9;
  - trabajar en Daikon, en .NET o en C++ que consume NativeSharp;
  - cambiar la resolución compartida de nombres.
- Las partes A y B de H4 las implementa Codex en su rama; aquí no se tocaron.
- En todo el carril no se modificó ningún archivo de `src/` ni de `include/`.
  `CMakeLists.txt` solo cambió para dar de alta el objetivo de prueba
  `nsc_cpp_c_identity_tests` y enlazarlo con `nsc_driver` (commits 2 y 6).

## 3. Commits (19, de `ceac3f83b` a `fbaba7882`)

| # | Commit | Asunto | Contenido |
|---|---|---|---|
| 1 | `d03974c64` | auditoria-identidad-c | auditoría inicial `T08-002-IDENTIDAD-AUDITORIA.md` |
| 2 | `e0bf804a1` | pruebas-identidad-c | `tests/unit/CppCIdentityTests.cpp` y alta en `CMakeLists.txt` |
| 3 | `1e91166e2` | validador-identidad-c | `pruebas-tanda-08-identidad-c/validar.cmd` |
| 4 | `9a1b4b6f5` | resultados-identidad-c | primeros resultados |
| 5 | `142034b93` | homonimos-c | encabezados homónimos en directorios distintos |
| 6 | `12c47cdc3` | ruta-driver-c | restauración de USR por la ruta completa del driver |
| 7 | `14cb59c6d` | propuesta-typedef | propuesta H1 |
| 8 | `8fbaba6bc` | propuesta-metadata-c | primera propuesta H3 (luego reescrita) |
| 9 | `4f8cfad46` | resultados-identidad-c-2 | resultados del segundo conjunto |
| 10 | `34b725544` | matriz-typedef | matriz de 12 casos de typedef |
| 11 | `33db1cae0` | enum-campo | H4 de extremo a extremo y propuesta en tres partes |
| 12 | `78fc7d3aa` | plantillas-metadata | 13 casos de plantillas, versionado, H7 y H8 |
| 13 | `41bfdd146` | global-static | global `static` y T08-004 |
| 14 | `185ab395c` | resultados-identidad-c-3 | resultados del tercer conjunto |
| 15 | `b573de8af` | h7-reproduccion | fixture `constantes-enum`, modo `--inspect-c-constants` |
| 16 | `b5e373808` | h7-parche | parche de H7 (sin aplicar) y su medición |
| 17 | `1b0ae316e` | h8-catalogo | fixture de H8, seis productores, modo `--catalogue-version-controls` |
| 18 | `7f81e7788` | h1-formas-usr | evidencia nueva para H1 |
| 19 | `fbaba7882` | resultados-identidad-c-4 | resultados del cuarto conjunto |

Este documento va en un commit propio después del 19.

## 4. Cómo reproducir

1. Construir solo lo necesario, de forma incremental:
   `cmake --build <build> --target nsc nsc_cpp_c_identity_tests`. En el carril se
   usa `build-claude-t08\tools\construir.cmd`, que fija INCLUDE, LIB y TEMP del
   toolset aislado.
2. Ejecutar el validador con estas variables:
   - `NSC_T08_BUILD` (carpeta de build);
   - `NSC_T08_OUT` (salida y temporales);
   - `NSC_T08_CLANG` (raíz de Clang 23.1.0);
   - `NSC_LINKER` (nslink.exe).

   Comando:
   `nativesharp-interop\pruebas-tanda-08-identidad-c\validar.cmd [constantes|catalogo]`.
   Sin argumento ejecuta todo.
3. Cada tabla `casos.txt` tiene la columna ACTUAL (lo de hoy) y la columna PROP
   (lo que debe dar la propuesta una vez implementada). Con
   `NSC_T08_PROPUESTAS=1`, el validador exige PROP. Solo tiene sentido por
   sección, una vez integrada la propuesta correspondiente.
4. Medición de H7: aplicar
   `nativesharp-interop/parches/t08-002-h7-excluir-constantes.patch`, reconstruir
   `nsc` y ejecutar `validar.cmd constantes` con `NSC_T08_PROPUESTAS=1`.

Resultado final en la laptop (`nsc` con hash `368fe14bbf0d356a`, fuente igual a
HEAD): código 0, 67 casos de compilación, 20 restauraciones O0–O3 y ningún `FAIL`.
Registro: [t08-002-resultados-identidad-c-4.md](registro-commits/t08-002-resultados-identidad-c-4.md).

## 5. Hallazgos y resultados

| Id | Tema | Estado | Documento |
|---|---|---|---|
| — | USR de cada categoría C, estabilidad y restauración | comprobado | [auditoría](T08-002-IDENTIDAD-AUDITORIA.md) |
| H1 | typedef idéntico en dos encabezados rechazado por el USR | propuesta sin aplicar | [typedef](T08-002-PROPUESTA-TYPEDEF.md) |
| H2 | el USR no distingue encabezados homónimos | comprobado; contrato de la auditoría | auditoría |
| H3 | funciones, globales y alias C no viajan para plantillas públicas | propuesta sin implementar | [metadata](T08-002-PROPUESTA-METADATA-C.md) |
| H4 | enum declarado dentro de un campo de record | partes A/B en la rama de Codex; C pendiente | [enum de campo](T08-002-PROPUESTA-ENUM-CAMPO.md) |
| H5 | struct idéntico en dos archivos: campos inaccesibles | registrado; carril de layout | auditoría |
| H6 | USR del enum anónimo anidado depende del archivo; nombre con ruta absoluta | comprobado | enum de campo |
| H7 | constante de enum C en un cuerpo de biblioteca rompe el productor | parche entregado sin aplicar | [H7](T08-002-H7-CONSTANTES.md) |
| H8 | catálogo 1.42 solo de enums confundido con la envoltura 4 | reproducido; propuesta | [H8](T08-002-H8-CATALOGO.md) |
| — | global `static` de encabezado | documentado para T08-004 | [global static](T08-002-GLOBAL-STATIC.md) |

### Identidad base (sondas 1–6)

- Todas las categorías C tienen USR estable entre sesiones.
- No dependen del encabezado: función, global, struct, union, enum, campo,
  enumerador, struct o enum anónimo con typedef, enum anónimo de ámbito de
  archivo y typedef de struct anónimo (`c:@T@X`).
- Dependen del nombre base del encabezado: typedef (`c:a.h@T@X`), `static inline`
  (`c:a.h@F@f`) y global `static` (`c:a.h@x`). En directorios distintos con el
  mismo nombre dan el mismo USR.
- El consumidor sin encabezado restaura los USR de records, unions, campos, enums
  y enumeradores en O0–O3, por el módulo de la prueba y por la ruta del driver.

### H1 typedef

- Matriz de 12 casos (`typedefs/casos.txt`, 36 proyectos): la regla propuesta
  solo acepta los casos compatibles.
- Los casos realmente distintos siguen rechazados:
  - por el tipo canónico (`int` frente a `long long`, distinta etiqueta);
  - por el agrupador de records (struct con definiciones distintas).
- Evidencia nueva:
  - Clang copia el nombre del archivo de expansión tal cual. La regla rechaza un
    archivo con `@` (falso negativo conservador) y acepta espacios.
  - Un typedef generado por una macro toma el archivo de expansión.

### H3 plantillas

- Un cuerpo ya compilado no necesita metadata C.
- Una plantilla pública que usa una función, una global, un alias, un record
  local o un `static inline` falla en el consumidor (`unknown identifier` o
  `unknown type name`).
- Lo que necesita el consumidor son los snapshots de declaración Clang que la
  proyección vuelve a ejecutar. Los campos exactos están en el documento.
- Recomendación de versionado: opción B.
  - La disposición del catálogo se elige por un minor 46, que sube solo cuando la
    sección existe.
  - Una máscara de características sustituye a las combinaciones de versión de
    envoltura.
  - Una sola subida de versión cubriría H3 y H4C.

### H4 enum en campo

- Con el encabezado directo, el campo de enum anónimo se omite y los
  enumeradores anidados no existen.
- Un enum con nombre declarado dentro del struct no se importa.
- El productor falla (`no importable identity` / `required C enum is unavailable`).
- Propuesta:
  - A: capturar los enums declarados dentro de records C;
  - B: proyectar el campo con su entero subyacente;
  - C: entrada de catálogo con clave en el campo propietario (cambio de formato).

### H7 constantes de enum (cuarto conjunto)

- Hoy fallan en el productor los cinco casos que usan una constante, con nombre o
  anónima, en un método ordinario o en una plantilla. El error es
  `metadata contains an incomplete or duplicate namespace state '.CConstModeB'`.
- Causa: toda constante de enum C importada es un estado público con espacio de
  nombres vacío y `appendNamespaceStateMetadata` la publica.
- Parche A medido (aplicado de forma temporal y revertido):
  - ningún productor publica estados (`states=0` en los siete);
  - métodos ordinarios y plantilla con el enum en la superficie ejecutan;
  - la plantilla con enum con nombre fuera de la superficie y la plantilla con
    enum anónimo fallan en el consumidor (`unknown identifier`). Necesitan
    información adicional.

### H8 catálogo (cuarto conjunto)

Seis productores reales cubren las envolturas 1 a 4. Su catálogo se reescribió
en cada minor 1.42–1.45 que lo admite (16 variantes):

- el lector actual rechaza solo la 1.42 solo de enums, con `metadata before 1.43
  cannot represent C source provenance`;
- la regla propuesta (formato según el minor declarado) decodifica las 16;
- el escritor actual produce 1.43, 1.44 o 1.45 según la envoltura.

## 6. Parches y propuestas entregados (ninguno aplicado)

| Entrega | Archivo destino | Cambio |
|---|---|---|
| `parches/t08-002-h7-excluir-constantes.patch` | `src/Interop/LibraryMetadataConstruction.cpp` | `\|\| state.isCppEnumConstant` en `appendNamespaceStateMetadata`, más un comentario |
| Regla `sameCppTypedefIdentity` (texto) | `src/Frontend/NameLookup/QualifiedTypeNames.cpp:233` y archivo nuevo `CppTypedefIdentity.cpp/.h` | aceptar USR `c:<archivo>@T@<nombre>` que solo difieren en el archivo |
| Siembra del catálogo para constantes de plantillas (diseño) | `src/Interop/Cpp/CppSemanticRecord.cpp` (`provideCppCRecordMetadataCatalog`) | añadir el enum con nombre de cada constante usada por una plantilla |
| H4 parte C (diseño) | codec, `CppSemanticRecord.cpp`, verificación y construcción | entrada de enum anónimo de campo |
| H3 (diseño) | codec, inventario, driver, verificación, construcción y carga | snapshots de declaración C para plantillas |
| H8 (diseño) | codec, verificación, carga, proyección ABI, `PrecompiledSyntaxModule.h` y otros | decodificar según el minor declarado |

## 7. Preguntas abiertas (decisiones del usuario o de Codex)

1. **H1:** ¿se autoriza la regla de typedef? ¿Debe aceptar también nombres de
   archivo con `@` (comparando solo el sufijo `@T@<nombre>`)?
2. **H7:** ¿se integra el parche A? Para las plantillas con enum con nombre fuera
   de la superficie, ¿se siembra el catálogo con todas las constantes importadas
   o solo con las de plantillas?
3. **H7/H3:** ¿qué hacer con una plantilla que usa una constante de enum anónimo
   antes de que exista el cambio de formato? ¿Diagnóstico en el productor o fallo
   en el consumidor, como hoy con funciones?
4. **H3 y H4C:** ¿opción B de versionado? ¿Qué número de minor se reserva y en qué
   orden respecto a los carriles de TLS, globales y layout?
5. **H8:** ¿se pasa a decodificar por el minor? ¿Se añade el minor a
   `PrecompiledSyntaxModule`, o se lleva el catálogo ya decodificado?
6. **H4C:** con las partes A y B de Codex, ¿se avanza con la entrada de catálogo
   para el enum anónimo de campo?
7. **Global `static` (T08-004):** ¿diagnóstico explícito o materializar una copia
   por proyecto y referencia?
8. **H5:** ¿el carril de layout revisa la regla de "misma ubicación" para
   definiciones idénticas en archivos distintos?

## 8. Qué revisar

Afirmaciones que conviene comprobar en el código de la rama:

- [ ] H7, causa:
  - `CppSemanticInventory.cpp:848-920` crea la constante con espacio de nombres
    vacío y `declaredTypeKey` `builtin:int`;
  - `LibraryMetadataConstruction.cpp:2961-2981` no excluye `isCppEnumConstant`;
  - `LibraryMetadataVerification.cpp:1371-1384` exige el espacio de nombres.
- [ ] H7, que ningún NSMETA válido existente cambia con el parche. Argumento:
  todo productor con constantes falla hoy. Comprobar que no hay otra ruta que
  ponga `isCppEnumConstant` con espacio de nombres no vacío
  (`projectCppEnumConstant` rechaza C++).
- [ ] H7, que el parche no cambia la semilla del catálogo:
  `provideCppCRecordMetadataCatalog` visita `declaredTypeKey`, y para estas
  constantes es `builtin:int`.
- [ ] H8, la detección por bytes:
  - `CppCRecordMetadataCodec.cpp:671-676`;
  - las puertas de `LibraryMetadataVerification.cpp:1420-1455`;
  - la proyección ABI de `LibraryMetadataPayload.cpp:760-783`, que con la 1.42
    solo de enums usa el bloque crudo.
- [ ] H8, la afirmación de que solo colisiona el bloque de records vacío. El
  tamaño u64 debe ser menor que 256 y el primer byte estar entre 1 y 4, y el
  mínimo es 4.
- [ ] H8, la **inferencia** de que decodificar por el minor no cambia la huella
  ABI de ningún archivo legible hoy (NO VERIFICADO: no se modificó el codec).
- [ ] H1, que la regla propuesta no rompe `CppClangSessionTests.cpp:6943`, `6944`
  y `4543` (contrastado por lectura).
- [ ] H3, la lista de campos que lee `projectFunction`
  (`CppSemanticFunction.cpp:225-310`) y `projectGlobalWithLookup`
  (`CppSemanticInventory.cpp:180` y siguientes).
- [ ] Pruebas: `nsc_cpp_c_identity_tests` afirma solo hechos observados con
  Clang 23.1.0. Revisar que las observaciones sin aserción estén marcadas como
  `OBSERVATION`.

NO VERIFICADO (declarado así en los documentos):

- Efecto del parche de H7 sobre `plantillas/casos.txt`: esa sección no se ejecutó
  con el parche.
- Si existe algún NSMETA 1.42 real con catálogo solo de enums.
- Propagación de la dependencia binaria C cuando solo la usa una plantilla
  instanciada en el consumidor (H3).
- Mensaje exacto de un lector anterior ante un archivo con minor 46.
- Los commits intermedios compilan (`nsc_cpp_c_identity_tests` se construyó tras
  cada commit de código), pero el validador completo solo se ejecutó sobre el
  último estado de cada ronda.

## 9. Archivos compartidos

Modificados en la rama (solo se añadieron líneas): `nativesharp-interop/TRACKING.md`
y `nativesharp-interop/SCOPE-LOCK-EJECUCION.md`. Pueden dar conflicto textual
al integrar.

Los tocarían las propuestas, en coordinación:

- `src/Interop/LibraryMetadataConstruction.cpp`: H7 y el minor condicional.
  Compartido con metadata y con el carril de variables globales.
- `src/Interop/Cpp/CppSemanticRecord.cpp`: H4, siembra de H7, H3 y H8.
  Compartido con el layout de records y con H4 A/B de Codex.
- `src/Interop/Internal/CppCRecordMetadataCodec.cpp/.h`,
  `LibraryMetadataVerification.cpp`, `LibraryMetadataPayload.cpp` e
  `include/nativesharp/Interop/LibraryMetadata.h`: metadata.
- `src/Interop/Cpp/CppSemanticInventory.cpp`: carril de variables globales.
- `src/Frontend/NameLookup/QualifiedTypeNames.cpp`: resolución compartida de
  nombres (H1).
- `include/nativesharp/Frontend/PrecompiledSyntaxModule.h`,
  `src/Compilation/PrecompiledProjectPreparation.cpp` y
  `src/Driver/PrecompiledSurface/MetadataSourceMapping.cpp`: carga precompilada
  (H8).

Al integrar el parche de H7 hay que actualizar dos tablas:

- en `constantes-enum/casos.txt`, la columna ACTUAL pasa a los valores de PROP;
- en `plantillas/casos.txt`, revisar las cuatro filas de constantes.

## 10. Incidencias que conviene conocer

- **Push al remoto que no hizo este carril.** El reflog de
  `origin/interop-t08-claude-identidad` muestra "update by push" a las 16:46:42
  (a `34b725544`) y a las 16:52:28 (a `185ab395c`), hora local de la laptop. No
  hay hooks en `.git/hooks`. El carril no ejecutó push ni operaciones remotas.
  Los commits 15–19 y este documento solo están en local hasta que el usuario
  los suba.
- **Medición temporal de H7:**
  - el parche se aplicó al árbol de trabajo y se construyó `nsc` (hash
    `c756fa1bb11fb393`);
  - se ejecutó la sección y se revirtió con `git checkout`;
  - se reconstruyó (hash `368fe14bbf0d356a`);
  - nunca se hizo commit con el parche.
- El hash de `nsc` cambia en cada construcción porque se regenera `Version.cpp`.
- `copy` de Windows conserva la fecha del archivo. Tras copiar un archivo fuente
  hay que actualizar su fecha (`build-claude-t08\tools\touch-test.cmd`); si no,
  ninja no recompila.
- Avisos de Git "LF will be replaced by CRLF" (autocrlf activo). El índice guarda
  LF; el parche de H7 se generó con `git diff` y está en LF.
