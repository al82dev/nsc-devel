# T08-019: diagnósticos de tipos C incompletos

Fecha: 2026-11-08. Casillas: T08-019-03a–e y T08-019-04. También incluye la validación del bloque T08-019.

## Resultado

Verificadas, con una corrección en el compilador: un parámetro o un retorno por valor de un struct o union C
incompleto en un callable NativeSharp.

## Corrección: parámetro y retorno incompletos por valor (03e)

**Síntoma.** `int Take(Opaque o)` y `Opaque Give()`, con `Opaque` incompleto, no tenían diagnóstico semántico. La
compilación llegaba a la bajada y se detenía sin ubicación en el fuente:

> LLVM code unit #0 failed: lowering failed: Stage 40 cannot map named value type to LLVM carrier: type:N::Opaque

Pasaba igual con el tipo visto por encabezado y con el tipo restaurado desde metadata.

**Causa.** El analizador de sentencias rechaza los locales incompletos por valor, pero nadie revisaba las firmas
de los callables fuente.

**Corrección.** `ProjectMethodBodyModel::verifyIncompleteCValueSignatures`, en el post-análisis junto a
`verifyOutParameterAssignments`, recorre:

- los métodos de clase fuente;
- las funciones de namespace fuente con cuerpo.

Para cada parámetro por valor y para el tipo de retorno, si la clave es un struct o union C importado incompleto,
informa `IncompleteType` en el parámetro o en la declaración:

- «parameter 'o' of 'Take' cannot be passed by value because C/C++ type 'Opaque' is incomplete»;
- «'Give' cannot return C/C++ type 'Opaque' by value because it is incomplete».

Se omiten los parámetros `ref`/`in`/`out` (su clave es un puntero), las declaraciones C importadas (conservan su
diagnóstico de importación diferida) y los miembros precompilados (se validaron en su productor).

## Prueba

Nueva: `pruebas-tanda-08-incompletos-diag-c/validar-diagnosticos.py`. Todos los rechazos se ejecutan en O0 en dos
caminos:

- **directo**, con el encabezado;
- **metadata** (04): un consumidor sin encabezado que ve `Opaque` y `OpU` solo por las firmas públicas de un
  productor precompilado.

En cada camino, un control positivo ejecuta los punteros opacos. Cada rechazo exige el nombre del tipo o del
miembro, la operación inválida y la ubicación en `Main.ns`, sin «internal error».

- **03a:** `sizeof` y `alignof` de struct y union incompletos: «C/C++ type 'Opaque' is incomplete and has no
  object layout».
- **03b:**
  - local por valor (struct, union y `var x = *a`): «C object local 'o' cannot be created by value because its
    type is incomplete»;
  - campo por valor en un struct NativeSharp: «C/C++ type 'Opaque' is incomplete and has no object layout».
- **03c:** acceso a miembro por puntero a struct y union incompletos: «receiver/type 'type:Pub::Opaque' has no
  compatible member named 'v'».
- **03d:** `+`, índice, `++`, `--` y `+=` sobre puntero a incompleto: «pointer arithmetic requires a complete C
  type; 'Opaque' has no element size».
- **03e:**
  - parámetro y retorno por valor en una función de namespace y en un método de clase (corrección anterior);
  - una función C declarada con un parámetro incompleto por valor (`int take_opaque(Opaque o);`) queda
    diferida: «C/C++ function 'take_opaque' cannot be imported: C/C++ parameter 0 is not yet mapped». Solo en el
    camino directo, porque la función no se publica.
- **04:** los mismos diagnósticos con los tipos restaurados desde metadata. El receptor del 03c se identifica como
  `type:Pub::Opaque`, es decir, con el proyecto productor.

## Validación del bloque

`A:\nsc-interop-build\claude\scripts\regresion-t08-019.cmd`:

- la prueba nueva;
- las muestras previas T08-019-01 y 02: `pruebas-tanda-08-incompleto/validar-opaco.cmd` y `validar.cmd`;
- la validación del bloque T08-018;
- la batería de T08-017: callbacks, records, uniones, layout, incompletos, punteros, typedef, enum e identidad.
