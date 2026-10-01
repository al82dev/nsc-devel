# Tanda operativa 39 — ABI, parser, resolución y tipado contextual

**Fecha:** 1 de octubre de 2026

**Rama:** `development`

**Plataforma:** Windows x64

**Configuración validada:** Debug/O0

**Baseline:** no ejecutado; Alberto lo ejecutará en T53

## Objetivo

Reproducir con el compilador generado en `A:\nsc-devel` las limitaciones importadas §41, §60,
§69 y §142, corregir únicamente las que siguieran presentes y congelar el ABI que ya era correcto:

- `ref`, `out` e `in` en funciones libres sin copias ni direcciones sustitutas;
- locales `T value` y `T[] value` dentro de un tipo o método genérico;
- constantes y estados del namespace contenedor visibles dentro de métodos;
- contextualización de `null` en `?:` cuando la otra rama es pointer o referencia compatible;
- diagnósticos negativos causales, sin casts ni inicialización ficticia para esconder defectos.

## Reproducción anterior al cambio

El fixture combinado se compiló primero con el NSC 3.0 anterior. El parser se detuvo en las dos
declaraciones genéricas:

```text
Main.ns:21:15: error: expected ';', found 'local'
Main.ns:27:17: error: expected ';', found 'local'
```

La causa era determinista: `looksLikeVariableDeclaration()` sólo reconocía identifiers registrados
en `knownTypeNames_`, pero `parseGenericParameterList()` guardaba `T` en el AST sin instalarlo en el
alcance de tipos del parser. `T value` se interpretaba entonces como expresión.

La inspección del pipeline de funciones libres confirmó que `ParameterPassKind` ya recorría
namespace model, HIR, MIR y lowering. No se cambió su ABI. La prueba ejecutable posterior congela
ese comportamiento existente y comprueba identidad de dirección y valores para escalares, structs
y referencias de clase. §41 queda como `YA_FUNCIONA_3.0`, no como una corrección inventada.

También se reprodujeron:

- §69: el lookup de estado de namespace sólo se consultaba desde una función libre; un método
  terminaba en `unknown identifier` después de revisar miembros;
- §142: `pointer : null` llegaba directamente al error de tipos incompatibles, aunque la misma
  conversión `null -> pointer/reference` ya era legal en asignaciones;
- un identifier con forma inequívoca de tipo desconocido se degradaba a error de parser, lejos de
  la causa, en vez de alcanzar `UnknownTypeName`.

## Diseño e implementación

### Alcance léxico de parámetros genéricos

`KnownGenericTypeScope` instala temporalmente los parámetros de tipo y type-pack en
`knownTypeNames_`. El guard vive durante el parseo completo de aliases, delegates, structs,
variants, clases, interfaces, métodos y funciones libres, y retira sólo los nombres que él mismo
insertó. Así se preservan correctamente parámetros externos con el mismo nombre y no se filtra `T`
hacia declaraciones posteriores. Los const-generic parameters no se registran como tipos.

El lookahead conserva además la forma inequívoca `U value`, `U* value` y `U[] value` aunque `U` no
sea conocido. `parseType()` crea el nodo de tipo y `ProjectNameLookup` produce el diagnóstico causal:

```text
unknown type name 'U' in module 'UnknownGenericLocal'
```

No se acepta una expresión ambigua como declaración: la ruta exige identifier de tipo, sufijos
puramente tipográficos opcionales y un segundo identifier como nombre de local.

### Lookup de estado de namespace desde métodos

La resolución no altera precedencia. Primero ganan locales y parámetros; después miembros de clase
(eventos, fields, properties y methods); sólo si ninguno coincide se consulta el binding de estado
del namespace contenedor. Las funciones libres mantienen su ruta existente. El fixture usa
`Population` desde un método de clase y obtiene el valor constante real.

### `null` contextual en `?:`

El join del ternario reutiliza `classifyImplicit(null, destino)`. Si una rama es `builtin:null` y la
otra admite esa conversión, el resultado adopta el tipo pointer/referencia y la procedencia de
ownership de la rama no-null. Esto cubre ambos órdenes sin crear una segunda regla de conversión.
`int : null` continúa rechazado con los dos tipos exactos en el diagnóstico.

### ABI de funciones libres

La prueba llama tres funciones libres y verifica en ejecución:

- `ref int`: `&value` dentro de la función es la dirección del caller;
- `out int`: el caller recibe el valor producido;
- `in int`: se lee por referencia y el origen permanece intacto;
- `ref/out/in Pair`: el struct se actualiza y copia con sus dos campos correctos;
- `ref Box`: se sustituye la referencia del caller sin modificar el objeto original.

No se agregó cast, wrapper, copia defensiva ni inicialización que altere el ABI observado.

## Archivos modificados

- `src/Parser/Parser.cpp`
  - introduce el scope léxico de parámetros genéricos;
  - conserva declaraciones con tipo desconocido para diagnóstico semántico causal.
- `src/Frontend/ProjectMethodBodyModel.cpp`
  - contextualiza `null` mediante la conversión implícita existente;
  - agrega el fallback de namespace después de toda la precedencia léxica y de miembros.
- `CMakeLists.txt`
  - registra únicamente el verificador dirigido `nsc.ns3.t39.frontend-abi` cuando las pruebas están
    habilitadas.

## Archivos nuevos y razón

- `tests/integration/ns3_t39_frontend_abi/T39FrontendAbi.nsproj`
- `tests/integration/ns3_t39_frontend_abi/Main.ns`
  - fixture positivo único que ejecuta ABI, genéricos, lookup y `null` pointer/reference.
- `tests/integration/ns3_t39_frontend_abi/negative/ConditionalNullInt/*`
  - impide ampliar `null` a valores incompatibles.
- `tests/integration/ns3_t39_frontend_abi/negative/UnknownGenericLocal/*`
  - exige `UnknownTypeName` y prohíbe la regresión a `expected ';'`.
- `tests/integration/ns3_t39_frontend_abi/negative/InWrite/*`
  - congela que un parámetro `in` no es asignable.
- `cmake/VerifyNativeSharpT39FrontendAbi.cmake`
  - compila y ejecuta sólo el fixture T39, y valida las tres negativas por texto causal.
- `docs/3.0/TANDA-OPERATIVA-39-ABI-PARSER-RESOLUCION.md`
  - este registro de reproducción, diseño, archivos, mejora y evidencia.

## Validación dirigida

- Construcción: `cmake --build A:\nsc-devel\build-clang --config Debug --target nsc -j 12` — correcta.
- Identidad construida: `3.0.0+build.20261001T064447Z.6dd9da3c5b642ae8`.
- Verificador único: `VerifyNativeSharpT39FrontendAbi.cmake` — correcto.
- Positivo: compilación O0 y ejecución física con exit `0`.
- Negativos:
  - `ConditionalNullInt`: tipos incompatibles `builtin:int`/`builtin:null`;
  - `UnknownGenericLocal`: `unknown type name 'U'`, sin `expected ';'`;
  - `InWrite`: `left side of assignment is not assignable`.

No se habilitó ni ejecutó CTest completo, no se ejecutó baseline, no se probaron O1/O2/O3, no se
usaron remotas y no se modificó ni instaló nada en `C:\native-sharp`.

## Mejora obtenida

Los genéricos dejan de depender de que un nombre se haya registrado globalmente por accidente; el
lookup de namespace respeta la precedencia del lenguaje; el ternario usa la misma relación de
conversión que asignaciones y argumentos; y el ABI de paso por referencia queda protegido por un
ejecutable que detecta copias silenciosas. Las cuatro entradas importadas tienen ahora evidencia
repetible de NSC 3.0 y diagnósticos negativos que apuntan a la causa.
