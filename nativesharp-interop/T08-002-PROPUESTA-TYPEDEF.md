# T08-002: propuesta para typedef C equivalentes (hallazgo H1)

Estado: **PROPUESTA NO IMPLEMENTADA**. No cambia la resolución compartida de
nombres; requiere autorización de alcance transversal antes de implementarse.

## Problema

HECHO COMPROBADO (sondas 3 y 5 de `nsc_cpp_c_identity_tests` y fixture
`pruebas-tanda-08-identidad-c/homonimos`):

| Proyecto con dos `CppLibraryReference` | Texto de los encabezados | Resultado actual |
|---|---|---|
| `a.h` + `a.h` | idéntico | enlaza |
| `a.h` + `b.h` | idéntico (`typedef int CIdInt;`) | `conflicting C/C++ type alias 'CIdInt'` |
| `dirA/same.h` + `dirB/same.h` | idéntico (`typedef int CSameInt;`) | enlaza, compila y ejecuta |
| `dirA/same.h` + `dirC/same.h` | `int` frente a `long long` | `conflicting C/C++ type alias 'CSameInt'` |

El USR de un typedef C de ámbito de archivo es `c:<nombre de archivo>@T@<nombre>`
(`c:a.h@T@CIdInt`, `c:same.h@T@CSameInt`). La comprobación de
`QualifiedTypeNames.cpp:230-236` (commit `847a71ccb`) trata dos USR conocidos
distintos como ambigüedad. El resultado depende del nombre del archivo, no de la
declaración: la misma pareja de typedefs se rechaza si los archivos se llaman
`a.h` y `b.h` y se acepta si ambos se llaman `same.h`. El rechazo por tipo
distinto (`dirC`) no depende del USR: lo produce la comparación del tipo
canónico de la línea 227, que ya existía antes de `847a71ccb`.

## Requisitos

1. Aceptar dos observaciones del mismo typedef cuando tienen el mismo nombre,
   proyecto, destino, tipo canónico y, si son conocidos, los mismos USR del
   record o enum al que remiten.
2. No unir declaraciones distintas por accidente:
   - un tipo canónico distinto sigue siendo un conflicto (comprobación existente);
   - un USR de otra forma que discrepe sigue siendo ambigüedad (contrato de la
     prueba existente con USR sintéticos);
   - la identidad del record o enum referido la decide el agrupador de records y
     la procedencia, no el alias: si dos `struct S` difieren, el conflicto se
     informa al usar el tipo, como hoy.
3. No cambiar el comportamiento de aliases sin USR ni de aliases C++ con
   espacio de nombres.

## Regla propuesta

Sustituir solo la comparación de USR de la línea 233 por una función de un
archivo propio, `src/Frontend/NameLookup/CppTypedefIdentity.cpp` (con su `.h`):

```cpp
// EN: Two Clang USRs denote the same C typedef identity when they are equal, or when both have the
//     file-scope typedef form c:<file>@T@<name> for `name` and differ only in <file>. Clang derives
//     that form from the header file name, which is not part of the declared type.
// ES: Dos USR de Clang denotan la misma identidad de typedef C si son iguales, o si ambos tienen la
//     forma de typedef de ámbito de archivo c:<archivo>@T@<nombre> para `name` y solo difieren en
//     <archivo>. Clang deriva esa forma del nombre del encabezado, que no forma parte del tipo.
bool sameCppTypedefIdentity(std::string_view left, std::string_view right, std::string_view name);
```

Una forma solo se reconoce si empieza por `c:`, el segmento `<archivo>` no está
vacío, no contiene `@` y el resto es exactamente `@T@` seguido de `name`. Una
cadena distinta conserva la comparación literal actual.

En `expandCppTypeAlias`, la rama de USR conocido pasa de
`*knownAliasUsr != *usr` a `!sameCppTypedefIdentity(*knownAliasUsr, *usr, alias.declaration.name)`.
Además, cuando ambos lados se aceptan solo por esta regla, se comparan los USR
del record o enum subyacente (`cppAliasRecordType`/`cppAliasEnumType` de cada
declaración) con la regla existente `compatibleUsr` (ausente o igual).

Costo: una comparación de cadenas por observación repetida; sin asignaciones.

## Contraste con las pruebas existentes

Contrastado por lectura del código y de las pruebas; la propuesta no se ejecutó
porque no está implementada.

| Prueba existente | Ruta que ejercita | Efecto de la propuesta |
|---|---|---|
| `CppClangSessionTests.cpp:6943` "matching alias USRs resolve across an unknown observation" | USR iguales `test:count` | sin cambio: pasa |
| `CppClangSessionTests.cpp:6944` "conflicting alias USRs remain ambiguous across an unknown observation" | USR `test:count` frente a `test:other-count` (sin la forma `c:<archivo>@T@`) | sin cambio: sigue ambiguo |
| `CppClangSessionTests.cpp:4543` "alias resolution rejects an imported record with a different known USR" | USR del record referido (líneas 261-340) | sin cambio |
| `CppClangSessionTests.cpp:10393` aliases conservan USR de record y enum | captura | sin cambio |
| `pruebas-tanda-08-incompleto/validar-opaco.cmd` y `pruebas-tanda-08-meta-c/validar.cmd` (`conflict`) | "type alias conflicts with a native type" (línea 304) | sin cambio |
| `pruebas-tanda-08-identidad-c`: `AliasAB`, `AliasAC` | homónimos | sin cambio (`AliasAC` se rechaza por tipo) |
| sonda 3 `a.h` + `b.h` | este hallazgo | pasa a enlazar; la observación debe convertirse en aserción |

Alternativas descartadas:

- Ignorar el USR siempre que el tipo canónico coincida: contradice la prueba
  existente de la línea 6944, que fija que USR distintos son ambigüedad.
- Capturar en la sesión si el USR depende de la ubicación (por ejemplo, con
  `isExternallyVisible`): la prueba de la línea 6944 reutiliza una observación
  real de typedef de archivo y le asigna USR sintéticos, así que la regla los
  aceptaría y cambiaría ese contrato.
- Exigir la misma ruta o procedencia del encabezado: seguiría rechazando
  `a.h` + `b.h` y no aceptaría ningún caso compatible nuevo.

Riesgo: la regla depende de la forma textual del USR de Clang 23.1.0. Las sondas
1 y 5 fijan esa forma (dependencia del nombre del archivo y nombre base en
directorios distintos); un cambio de Clang las haría fallar antes de afectar a la
resolución.

## Matriz de typedef y revisión contra los contratos (tercer conjunto)

HECHO COMPROBADO con `nsc_cpp_c_identity_tests --typedef-matrix` y el fixture
`pruebas-tanda-08-identidad-c/typedefs` (tabla `casos.txt`), compilador de este
carril y Clang 23.1.0. Cada caso tiene tres proyectos: X1 (solo `x.h`, control de
la fuente), XY (`x.h` + `y.h`, nombres de archivo distintos) y PQ (`p/t.h` +
`q/t.h`, mismo nombre de archivo en directorios distintos). La sonda lee de cada
sesión el USR del typedef, su tipo canónico y el USR del record o enum al que
remite, y aplica una réplica de la regla actual y una de la regla propuesta. El
validador compila los 36 proyectos y exige el resultado de la tabla.

| Caso | Clase | XY hoy | PQ hoy | Regla propuesta en XY |
|---|---|---|---|---|
| T01 `typedef int` en ambos | compatible | alias | ejecuta | acepta → ejecuta |
| T02 `int` frente a `long long` | distinto | alias | alias | rechaza (tipo canónico) |
| T03 `unsigned int` frente a `unsigned` | compatible | alias | ejecuta | acepta |
| T04 cadena `typedef CTdBase` frente a `typedef int` | compatible | alias | ejecuta | acepta |
| T05 `typedef struct CTdS` con la misma definición | compatible | alias | ejecuta | acepta |
| T06 `typedef struct CTdO` con definiciones distintas | distinto | alias | record | acepta el alias; rechaza el record |
| T07 `typedef struct { ... } CTdAnon` | compatible | ejecuta | ejecuta | sin cambio |
| T08 `typedef enum CTdE` | compatible | alias | ejecuta | acepta |
| T09 mismo alias, etiquetas `CTdP` y `CTdQ` | distinto | alias | alias | rechaza (tipo canónico) |
| T10 puntero a función `int (*)(int)` | compatible | alias | ejecuta | acepta |
| T11 `struct CTdF` idéntico sin typedef | compatible | member | member | fuera de la regla |
| T12 `struct CTdG` distinto sin typedef | distinto | record | record | fuera de la regla |

`alias` = `conflicting C/C++ type alias`; `record` = `referenced C/C++ record has
conflicting or unverified definitions`; `member` = `has no compatible member named`.

Hechos de la matriz:

1. HECHO COMPROBADO: en PQ los dos USR del typedef son iguales (`c:t.h@T@...`); en
   XY difieren solo en el segmento de archivo (`c:x.h@T@...` frente a
   `c:y.h@T@...`). Con la regla propuesta XY se comporta exactamente como PQ en los
   diez casos con typedef: la sonda lo afirma por caso.
2. HECHO COMPROBADO: la regla propuesta solo cambia los casos compatibles (T01,
   T03, T04, T05, T08, T10). Los distintos siguen rechazados: T02 y T09 por el tipo
   canónico, que la propuesta no toca; T06 por el agrupador de records. PQ es la
   evidencia de T06: con USR de alias iguales, el uso de `CTdOtherAlias*` ya
   informa `referenced C/C++ record has conflicting or unverified definitions`. La
   propuesta no necesita decidir la identidad del record.
3. HECHO COMPROBADO: el typedef de un struct anónimo (T07) tiene USR sin archivo,
   `c:@T@CTdAnon`, y el record `c:@SA@CTdAnon`. No le afecta H1.
4. HECHO COMPROBADO (H5, fuera de esta propuesta): un mismo `struct` definido con el
   mismo texto en dos archivos distintos (T11) se enlaza como tipo, pero sus campos
   no son accesibles. `selectCppRecordMemberDefinition`
   (`CppSemanticRecord.cpp:728-760`) exige que todas las definiciones tengan el mismo
   archivo, línea y columna. Es un contrato del carril de layout de records, no de
   la identidad de typedef; no se modifica.

Revisión contra los contratos existentes, ahora con datos de Clang y no solo por
lectura:

| Contrato | Resultado |
|---|---|
| Tipo canónico distinto es conflicto (`QualifiedTypeNames.cpp:224-229`) | se conserva (T02, T09) |
| USR conocidos distintos de otra forma son ambigüedad (`CppClangSessionTests.cpp:6944`, `test:count` / `test:other-count`) | se conserva: la forma `c:<archivo>@T@` no se reconoce en esos USR |
| USR iguales se aceptan (`CppClangSessionTests.cpp:6943`) | se conserva |
| USR del record referido distinto rechaza el alias (`CppClangSessionTests.cpp:4543`) | se conserva: la regla exige USR compatibles del record y del enum |
| Definiciones distintas de un record se rechazan al usar el tipo | se conserva (T06 PQ, T12) |
| Homónimos `dirA`/`dirB`/`dirC` (`homonimos/AliasAB`, `AliasAC`) | sin cambio |
| Alias frente a tipo NativeSharp (`QualifiedTypeNames.cpp:304`) | ruta distinta, sin cambio |

Conclusión: la regla propuesta distingue en los doce casos los typedef
compatibles de los realmente distintos sin tocar el tipo canónico ni la
identidad de records. Sigue sin implementarse: cambia la resolución compartida
de nombres y necesita autorización.

Al implementarla, el cambio en las pruebas es mecánico: en `typedefs/casos.txt`
la columna XY pasa a valer lo que hoy vale PROP, y `validar.cmd` con
`NSC_T08_PROPUESTAS=1` ya exige ese resultado.

## Evidencia nueva sobre la forma del USR (cuarto conjunto)

HECHO COMPROBADO por `checkTypedefFileForms`, dentro del modo `--typedef-matrix`
(Clang 23.1.0, afirmado):

| Par de encabezados | USR del segundo | Regla actual | Regla propuesta |
|---|---|---|---|
| `plain.h` + `at@sign.h`, mismo `typedef int` | `c:at@sign.h@T@CTdForm` | rechaza | **rechaza** |
| `plain.h` + `with space.h`, mismo `typedef int` | `c:with space.h@T@CTdForm` | rechaza | acepta |
| `macro-x.h` + `macro-y.h`; el typedef lo declara una macro de `macros.h`, con `int` y `long long` | `c:macro-y.h@T@CTdMacro` (y `c:macro-x.h@T@CTdMacro`) | rechaza | rechaza (tipo canónico) |

Efecto sobre la regla:

1. Clang copia el nombre del archivo tal cual, sin escapar `@` ni espacios. La
   restricción "el segmento de archivo no contiene `@`" deja fuera los
   encabezados con `@` en el nombre: un typedef idéntico en `at@sign.h` sigue
   rechazado. Es un falso negativo conservador, no una unión indebida.
   - Si se quisiera aceptar, basta con comprobar el sufijo exacto `@T@<nombre>`.
     Así se toma como archivo todo lo que hay entre `c:` y ese sufijo.
   - La forma sigue siendo inequívoca para el mismo nombre de typedef, porque el
     tipo canónico se sigue comparando.
   - Decisión pendiente; no cambia los doce casos de la matriz.
2. El segmento de archivo es el de la **expansión**, no el de la definición de la
   macro. Un typedef generado por una macro común no comparte USR entre
   encabezados distintos, y la regla lo trata igual que uno escrito a mano.
3. La regla no se aplicó. La resolución compartida de nombres no se modificó.

## Pruebas al implementar

Las pruebas 1 a 4 ya existen como matriz (`typedefs/casos.txt`); al implementar,
se ejecuta `validar.cmd` con `NSC_T08_PROPUESTAS=1` y se actualiza la columna XY.

1. Sonda 3: `a.h` + `b.h` con el mismo typedef enlaza (aserción nueva).
2. `a.h` + `b.h` con tipos distintos: sigue `conflicting C/C++ type alias`.
3. `a.h` + `b.h` con `typedef struct S T;` y la misma definición de `S`: enlaza.
   Con definiciones distintas de `S`: el conflicto del record se informa al usar
   `T`; no se elige una de las dos.
4. Fixture real con `nsc`: dos encabezados de nombre distinto con el mismo
   typedef compila y ejecuta en O0–O3.
5. Prueba unitaria de `sameCppTypedefIdentity`: iguales, forma de archivo con
   nombre igual y archivo distinto, nombre distinto, segmento vacío, `@` en el
   archivo, USR de otra forma.
6. Sin cambios ni modificación: las pruebas existentes de la tabla anterior.

Archivos que cambiarían: `src/Frontend/NameLookup/QualifiedTypeNames.cpp` (una
condición), `src/Frontend/NameLookup/CppTypedefIdentity.cpp/.h` (nuevos),
`CMakeLists.txt` (fuente nueva), `tests/unit/CppCIdentityTests.cpp` y fixtures
nuevos.
