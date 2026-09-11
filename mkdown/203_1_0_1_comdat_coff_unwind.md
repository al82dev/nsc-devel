# 1.0.1 — Líderes COMDAT COFF compatibles con unwind

## Resultado

Las definiciones genéricas reutilizables usan ahora su propio símbolo físico
como clave y líder COMDAT. LLVM puede emitir las secciones auxiliares Win64
`.xdata` y `.pdata` directamente asociadas con la sección de la función, sin
introducir un líder global artificial.

Esto elimina el objeto que NSLink rechazaba con:

```text
NSL1025: COMDAT section has no section definition symbol:
section '.xdata' has no symbol to identify it
```

## Defecto raíz

La política anterior ya había corregido la granularidad: cada función o global
genérico tenía un COMDAT independiente. Sin embargo, la identidad del grupo era
un global constante `i8` separado:

```text
__nsc_generic_odr_<hash>
```

La función real y ese byte compartían el grupo. En Win64, una función con
información de unwind genera además `.xdata` y normalmente `.pdata`. La
combinación producía una asociación COFF de dos niveles:

```text
.xdata/.pdata -> sección de función -> líder global artificial
```

El lector COMDAT de NSLink no encontraba en `.xdata` un símbolo de definición
de sección utilizable y abortaba antes de construir la DLL. El problema se veía
en `-O0` porque las funciones genéricas conservaban unwind aun sin optimización.

## Implementación

### `src/LLVM/ProjectOopLlvmLowerer.cpp`

`applyGenericOdrDefinition()` conserva las garantías necesarias:

- linkage `weak_odr` para mantener definiciones públicas exportables;
- selección `Any` para deduplicar productor y consumidor;
- un grupo distinto por símbolo físico.

La diferencia es que `LLVMGetOrInsertComdat()` recibe ahora el nombre físico de
la propia función o global. Esa definición es simultáneamente el miembro y el
líder real del grupo COFF.

Se eliminaron:

- el hash `__nsc_generic_odr_<hash>`;
- el global neutro `i8`;
- el mapa de líderes artificiales, que dejó de ser necesario.

La granularidad por símbolo introducida anteriormente permanece intacta. No se
regresa al COMDAT único por especialización que podía descartar subconjuntos
distintos del productor y consumidor.

### `tests/unit/Stage44GenericsMonomorphizationTests.cpp`

La regresión de monomorfización comprueba ahora dos propiedades del LLVM IR:

- siguen existiendo grupos `comdat any` para las definiciones genéricas;
- ya no se emite ningún líder artificial con prefijo
  `__nsc_generic_odr_`.

La emisión de objeto COFF que ya forma parte del mismo caso conserva la
cobertura de la materialización Win64 posterior.

## Alcance

La regla se aplica uniformemente a todas las entidades que ya pasan por
`applyGenericOdrDefinition()`:

- métodos y constructores genéricos;
- entradas de destrucción;
- almacenamiento y accessors estáticos;
- vtables y tablas de interfaces;
- mapas, descriptors y accessors de descriptors;
- nombres RTTI materializados para especializaciones.

Las funciones son el caso que produce `.xdata`/`.pdata`; los globals utilizan
la misma regla por símbolo sin secciones unwind.

No cambia el formato NSMETA, el ABI público ni la resolución genérica. No se
modificaron NSLink, SaruGL, bindings, tutoriales ni repositorios externos.

## Verificación de esta entrega

Se reconstruye completamente `nsc` y se instala en `C:\native-sharp`. No se
ejecuta baseline ni se ejecutan bindings. El consumidor que produjo `NSL1025`
realiza la comprobación final con su `program.obj` real.

## Commit de referencia

### Español

**Título:** Usa el símbolo físico como líder COMDAT COFF

**Descripción:** Elimina los líderes globales artificiales de los COMDAT
genéricos y usa cada función o global físico como clave de su propio grupo.
Conserva deduplicación por símbolo y permite asociar directamente `.xdata` y
`.pdata` sin secciones unwind huérfanas para NSLink.

### English

**Title:** Use the physical symbol as the COFF COMDAT leader

**Description:** Remove artificial global leaders from generic COMDAT groups
and use each physical function or global as its own group key. Preserve
per-symbol deduplication while allowing `.xdata` and `.pdata` to associate
directly without orphan unwind sections for NSLink.

## English summary

Generic ODR definitions now follow the standard COFF shape: the physical
definition is the COMDAT leader. Function unwind metadata associates directly
with that section, while producer/consumer deduplication remains independent
for every symbol.
