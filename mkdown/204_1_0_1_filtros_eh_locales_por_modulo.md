# 1.0.1 — Filtros EH tipados locales por módulo

## Resultado

Los filtros Win64 generados para `catch` tipado dejan de publicarse como
definiciones externas fuertes. Cada objeto conserva su propia implementación
local y su propia `.xdata` puede referenciarla sin crear una colisión al enlazar
un framework precompilado con su consumidor.

El símbolo reportado fue:

```text
__nativesharp_eh_filter_type_9325109546701631482
```

NSLink lo encontraba definido tanto en `program.obj` como en
`Saru.NativeSharp.lib` y rechazaba el enlace con `NSL1021`.

## Defecto raíz

`defineTypedCatchFilters()` deriva el nombre del filtro a partir del
`stableTypeId` del tipo capturado. Esa estabilidad es correcta: todos los
módulos deben comprobar el mismo tipo nominal con el mismo identificador.

El error era confundir nombre estable con identidad pública. El filtro:

- no forma parte de NSMETA;
- no es un export de biblioteca;
- no es una entrada del runtime NativeSharp;
- sólo se referencia desde las tablas EH del módulo que lo genera;
- llama helpers RTTI internos de ese mismo módulo.

Por tanto, dos módulos que contienen un `catch` del mismo tipo necesitan dos
copias privadas equivalentes, no una definición global compartida.

## Implementación

### `src/LLVM/ProjectOopLlvmLowerer.cpp`

`defineTypedCatchFilters()` aplica `LLVMInternalLinkage` inmediatamente después
de crear cada función `__nativesharp_eh_filter_type_*`.

La función conserva:

- el nombre determinista basado en `stableTypeId`;
- su firma Win64 `int (void*, void*)`;
- su subprograma de depuración sintetizado;
- la comprobación de dominio de excepciones NativeSharp;
- la consulta RTTI del tipo capturado;
- las referencias desde `.xdata` del módulo.

Sólo cambia su visibilidad COFF. El símbolo queda estático al objeto y deja de
participar en la tabla global de definiciones del linker.

Esta política coincide con `__nativesharp_rtti_is_class` y
`__nativesharp_rtti_is_interface`, que ya son internos precisamente para que
cada biblioteca pueda emitir sus helpers sin duplicados fuertes.

### `tests/unit/Stage45TypedCatchRttiTests.cpp`

La regresión exige ahora:

- filtros `internal` para el tipo derivado y el tipo base;
- exactamente un filtro local por tipo utilizado en cláusulas `catch`;
- conservación del nombre estable y de la lógica RTTI existente.

## Por qué no COMDAT

Un COMDAT también podría seleccionar una copia, pero convertiría un detalle
privado en una identidad de enlace compartida y añadiría asociaciones de unwind
innecesarias. `internal` expresa el contrato real:

```text
una implementación por módulo, cero identidad entre módulos
```

No se requiere que NSLink reconozca este nombre ni aplique una excepción para
él. El linker sólo recibe símbolos locales legítimos y relocations locales.

## Alcance

La corrección cubre cualquier tipo capturado por `catch`, sea local,
precompilado, base o derivado. Dos módulos pueden usar el mismo `stableTypeId`
sin producir `NSL1021`.

No cambia NSMETA, el ABI de excepciones, la representación de tipos, la tabla
PE de excepciones ni la semántica de `catch` múltiple.

No se modificaron NSLink, SaruGL, bindings, tutoriales ni repositorios externos.
No se ejecuta baseline ni se ejecutan bindings durante esta entrega.

## Commit de referencia

### Español

**Título:** Mantiene locales los filtros EH tipados

**Descripción:** Emite cada `__nativesharp_eh_filter_type_*` con linkage
interno porque sólo lo referencia la tabla EH de su propio módulo. Evita
definiciones fuertes duplicadas entre productor precompilado y consumidor sin
añadir otro contrato COMDAT público.

### English

**Title:** Keep typed EH filters module-local

**Description:** Emit every `__nativesharp_eh_filter_type_*` with internal
linkage because it is referenced only by its module's own EH table. Prevent
duplicate strong definitions between a precompiled producer and consumer
without adding another public COMDAT contract.

## English summary

Typed Win64 catch filters are now module-local implementation details. Their
stable type-based names remain useful for diagnostics and PDB data, while COFF
linking no longer sees competing global definitions across libraries.
