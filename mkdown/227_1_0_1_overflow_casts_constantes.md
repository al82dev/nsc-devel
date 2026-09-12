# 227 — Una semántica para overflow, casts y constantes / One semantics for overflow, casts, and constants

**Línea / Line:** `1.0.1-dev`

**Contratos / Contracts:** `NS-NUM-001`, `NS-NUM-002`, `NS-NUM-003`

## Resumen

La ejecución y `ProjectIntegerEvaluator` ya definían el overflow entero como wrap, incluido el caso
físicamente problemático `MinValue / -1`. Sin embargo, la validación general de inicializadores
`const` conservaba una respuesta anterior: rechazaba esa división y su residuo. Un mismo texto era
válido en ejecución y en una etiqueta de `switch`, pero inválido como constante pública.

La entrega hace que el evaluador general produzca:

```text
int.MinValue  / -1 = int.MinValue
int.MinValue  % -1 = 0
long.MinValue / -1 = long.MinValue
long.MinValue % -1 = 0
```

sin ejecutar la operación indefinida equivalente del C++ que implementa el compilador.

También elimina el límite accidental de 32 saltos al expandir constantes nombradas. Una cadena
acíclica larga ahora se evalúa completa; un ciclo directo o transitivo se detecta mediante los
ordinales de miembros activos.

## Causa

NativeSharp tiene un evaluador entero tipado reutilizado por enums y `switch`, y un evaluador más
amplio para `const` que también cubre booleanos, strings y punto flotante. El segundo protegía
correctamente al proceso compilador contra UB de C++, pero lo hacía devolviendo “no constante” en
el par mínimo/-1. Esa protección contradijo el contrato de wrap ya aplicado por el primero y por el
backend.

Por separado, `ProjectIntegerEvaluator` usaba una sola profundidad para dos grafos distintos:
anidamiento del AST y dependencias entre miembros. El valor 32 evitaba ciclos por agotamiento, pero
también rechazaba programas acíclicos legítimos.

## Implementación

### División y residuo

`ProjectMethodBodyModel::isCompileTimeConstantExpression()` intercepta el par mínimo/-1 antes de
usar `/` o `%` de C++. Para división conserva el patrón de bits mínimo; para residuo devuelve cero.
El resto de operandos sigue la ruta firmada o sin signo que ya existía.

### Dependencias de constantes

`ProjectIntegerEvaluator::evaluate()` crea una pila de ordinales activos. Al expandir un miembro:

1. rechaza si su ordinal ya está en la pila;
2. lo inserta;
3. evalúa su AST con profundidad sintáctica reiniciada;
4. lo retira incluso cuando el resultado no es constante.

El límite de 1024 queda reservado al anidamiento de una sola expresión, no al largo del grafo de
dependencias. Esto mantiene una defensa contra AST hostil sin cambiar la semántica de referencias
`const` válidas.

### Casts

Los casts enteros siguen normalizando al ancho y signo del destino mediante
`ProjectIntegerEvaluator::normalize`. Las pruebas de la entrega materializan los resultados
plegados en IR para comprobar que la validación de `const` y la emisión observan los mismos bits;
no se agrega `checked`, `unchecked`, saturación ni una política dependiente de Debug/Release.

## Archivos modificados

| Archivo | Cambio |
|---|---|
| `include/nativesharp/Frontend/ProjectIntegerEvaluator.h` | Propaga la pila de dependencias y corrige el contrato documentado de mínimo/-1 |
| `src/Frontend/ProjectIntegerEvaluator.cpp` | Separa profundidad de AST y ciclo de miembros; acepta cadenas acíclicas largas |
| `src/Frontend/ProjectMethodBodyModel.cpp` | Alinea el evaluador general de `const` con el wrap entero definido |
| `tests/unit/Cc1IntegerEvaluatorTests.cpp` | Cubre 80 dependencias válidas y un ciclo transitivo |
| `tests/unit/Cc1CorrectnessTests.cpp` | Cubre bits emitidos para división/residuo mínimo/-1 en 32 y 64 bits |
| `docs/1.0.1/language/NativeSharp_Language_Specification_1_0_1.md` | Hace explícita la equivalencia entre plegado y ejecución |
| `mkdown/227_1_0_1_overflow_casts_constantes.md` | Registra diagnóstico, diseño, archivos y evidencia |

## Disciplina de verificación

No se ejecutan pruebas ni baseline durante esta entrega. Tras guardar el commit se reconstruye
desde cero únicamente `nsc`. Las pruebas incorporadas y la matriz acumulativa se ejecutan al cerrar
la entrega 234.

---

## English

Runtime execution and the typed integer evaluator already defined signed overflow as wrapping,
including `MinValue / -1`. The broader `const` validator still rejected that exact pair, so the
same expression had different answers in runtime code, switch labels, and constant initializers.
Delivery 227 handles the pair without invoking C++ undefined behavior: division returns the minimum
bit pattern and remainder returns zero for both 32- and 64-bit signed integers.

The delivery also separates expression nesting from named-constant dependencies. A stack of active
member ordinals rejects direct and transitive cycles, while a valid dependency chain is no longer
cut off after 32 references. The syntax-depth guard remains a hostile-input defense, not a language
limit.

Explicit integer casts continue to normalize to the destination width and signedness. Directed
tests cover an 80-member acyclic chain, a real cycle, and emitted constant bits for all four
minimum/-1 cases. No `checked`, `unchecked`, saturation, or configuration-dependent arithmetic is
introduced. Only a clean compiler rebuild follows the commit; execution waits for the cumulative
baseline after delivery 234.
