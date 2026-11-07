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

## Pruebas al implementar

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
