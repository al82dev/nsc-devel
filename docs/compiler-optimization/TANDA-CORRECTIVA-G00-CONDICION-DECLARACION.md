# Tanda correctiva G00: desambiguación de declaraciones en condiciones

## Problema

El reconocimiento anticipado de declaraciones en `if`, `while` y `switch` reutilizaba el
detector general de variables. Una expresión como `left * right >= limit` podía confundirse
con la forma declarativa `Type* name`, desincronizando el parser y haciendo que miembros
posteriores de la clase se diagnosticaran como declaraciones globales inválidas.

El baseline expuso la cascada en Stage 46: el productor `StaticProducer` no se generaba y sus
consumidores terminaban buscando correctamente el artefacto ausente mediante `NS_LIB_ROOT`.

## Cambios

- `include/nativesharp/Parser/Parser.h`: declara el detector específico de condiciones.
- `src/Parser/Parser.cpp`: una condición sólo se considera declaración cuando tiene la forma
  `T nombre = expresión`; una multiplicación permanece en el parser de expresiones. Además,
  el lookahead de miembros acepta el prefijo inequívoco `owned`/`borrowed` cuando el tipo de
  retorno es un parámetro genérico que se declara después del nombre del método. La misma
  desambiguación conserva `x * x;` como expresión cuando `x` no es un tipo conocido, y el
  rechazo de `extern X` mantiene el diagnóstico contractual `expected 'C'`.
- `tests/unit/NativeSharp3T49LanguageSurfaceTests.cpp`: cubre la expresión multiplicativa que
  originó la regresión.
- `tests/unit/Stage50_1OwnershipSyntaxTests.cpp`: cubre `owned T Transfer<T>(owned T value)`.

`NS_LIB_ROOT` no fue modificado: continúa siendo el fallback global para dependencias que no
existen junto al proyecto.

## Mejora

Las condiciones aritméticas dejan de corromper el resto del árbol sintáctico, mientras se
mantiene el diagnóstico explícito para las declaraciones en condición todavía diferidas.

## Validación específica

- Construcción exclusiva del target `nsc`.
- Compilación O0 aislada del productor de Stage 46 conservado por el baseline.
- Prueba unitaria específica de superficie de lenguaje T49.
- No se ejecutó el baseline general.
