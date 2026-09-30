# Tanda operativa 21 — T09/E021: `foreach` sobre rango entero

Fecha: 2026-09-29

Rama: `development`

Alcance: vertical productivo de NS3-T09-F01/F02 y E021

Estado de T09/E021–E025: `IMPLEMENTING`

## Objetivo

Conectar al compilador el primer range lazy real, sin colección temporal ni dependencia de una
biblioteca todavía inexistente:

```text
foreach (int value in begin..end)
{
    // end es exclusivo
}
```

La forma debe llegar a LLVM/PE en O0, evaluar cada endpoint exactamente una vez y reutilizar el
control de flujo ya certificado de `for`, incluido `continue`.

## Diagnóstico previo

- El compilador ya tenía `foreach` productivo para arrays y el patrón estructural
  `GetEnumerator/MoveNext/Current`.
- `RangeExpression` sólo se construía dentro de `receiver[begin..end]`; `begin..end` no era una
  expresión general y, por tanto, el range entero descrito por T09 jamás podía alimentar foreach.
- El helper `LazyIntegerRange` definía conteo y ausencia de materialización, pero no participaba en
  parser, semántica ni backend.
- Slicing sobre arrays/strings se rechazaba correctamente para evitar asignación implícita; esta
  tanda no relaja ese límite ni inventa layout de `Span`/`StringView`.

## Diseño implementado

### Expresión range general

El parser añade un nivel de precedencia de Edition 3 para `begin..end` y conserva el mismo nodo
`RangeExpression` usado por índices. Las formas de slicing existentes continúan pasando por el
mismo AST; no se crea un segundo operador ni un carrier ABI nuevo.

### Lowering storage-free de foreach

Cuando la fuente de `foreach` es un `RangeExpression`, el parser la transforma a formas ordinarias
ya productivas:

1. bind de `begin` a un temporal const higiénico;
2. bind de `end` a otro temporal const, después de begin;
3. cursor mutable inicializado con begin;
4. `for (cursor; cursor < end; cursor = cursor + 1)`;
5. variable de iteración const inicializada desde el cursor;
6. cuerpo original.

Los identificadores sintetizados contienen `$`, carácter que el lexer no admite en identificadores
de usuario. No pueden ser observados, sombreados ni reasignados desde el programa.

No se añade un nodo HIR/MIR/LLVM especial: el backend recibe los mismos locals, comparación,
incremento y ramas que un `for` escrito a mano. Esto mantiene `break`/`continue`, cleanup y O0 bajo
las rutas existentes y no introduce heap.

### Límites causales

- el final es obligatorio: `begin..` falla porque un foreach finito necesita condición;
- `^n` se rechaza: from-end sólo tiene significado relativo a un receptor con longitud;
- el paso actual es `+1` y el final es exclusivo;
- rangos descendentes/step explícito pertenecen a la extensión futura del protocolo range.

## Archivos modificados

- `include/nativesharp/Parser/Parser.h`: declara el nuevo nivel de expresión range.
- `src/Parser/Parser.cpp`: parsea `begin..end` y desazucara foreach entero.
- `tests/unit/Cc1CorrectnessTests.cpp`: registra c68 para el baseline final, sin ejecutarlo ahora.
- `docs/3.0/TANDA-09-ITERACION-RANGES-Y-CONSULTAS.md`: actualiza el estado productivo real.

## Archivos nuevos y por qué

- `tests/cc1/c68_ns3_integer_range/c68_ns3_integer_range.nsproj` y `Main.ns`: witness O0 que
  comprueba endpoint único, end exclusivo y `continue`; termina en 42.
- `tests/unit/Cc1CorrectnessTests.cpp` conserva además dos rechazos inline permanentes: final
  abierto y `^` sin receptor sized, sin inflar el número de proyectos del corpus.
- `docs/3.0/TANDA-OPERATIVA-21-T09-E021-INTEGER-RANGE.md`: expediente de alcance, evidencia y deuda.

## Validación dirigida ejecutada

1. `nsc` Debug/O0 construido en `A:\nsc-devel\build-clang`: correcto.
2. c68 compiló y enlazó en O0 con el compilador generado.
3. c68 terminó con código 42.
4. El programa confirmó una sola llamada a `Begin()` y `End()`, end exclusivo y avance correcto
   después de `continue`.
5. Dumps HIR y MIR de c68 se generaron correctamente sobre el lowering ordinario de locals/loop.
6. `1..` falló con `foreach integer range requires a finite end endpoint`.
7. `^1..4` falló con `a from-end endpoint requires a sized receiver`.
8. Target único T01: `PASS: 27 checks`; preservó el parsing previo de ranges dentro de `[]`.
9. Target único T09: `PASS: 8 checks`.
10. La protección del corpus confirmó una etiqueta por fixture después de registrar c68; los
    rechazos quedaron inline y no como proyectos huérfanos.
11. Los temporales y logs de error se eliminaron; `NSC_BUILD_TESTS` volvió a `OFF` y `nsc` se
    reconstruyó.

No se ejecutó baseline, CTest general, O1/O2/O3, instalación, deploy ni remotas.

## Mejora concreta

T09 deja de ser exclusivamente un helper: NativeSharp puede iterar un intervalo entero lazy hasta
PE sin reservar ni materializar una colección. La semántica de evaluación única y el costo son
observables en un programa ejecutable.

## Deuda explícita

- foreach lending/ref/mutable-ref no tiene todavía protocolo completo de borrow/invalidation;
- step, rangos descendentes e inclusivos no tienen sintaxis/lowering productivo;
- `Length/Slice` para Span/ReadOnlySpan/StringView sigue abierto: requiere safe borrow y retorno de
  view sin ligar el compilador a layout privado;
- E022–E025 algoritmos, adapters lazy, zip/chunk/flatten y materialización con allocator pertenecen
  a las bibliotecas y aún no están implementados;
- comprehensions y generators síncronos siguen siendo helpers sin parser/state-machine lowering;
- faltan comparación de assembly/MIR contra loop manual, metadata/tooling y gates integrales.

Por estas deudas T09 y E021–E025 permanecen `IMPLEMENTING`. La tanda operativa 21 cierra este
vertical entero acotado, no el subsystem completo de ranges.
