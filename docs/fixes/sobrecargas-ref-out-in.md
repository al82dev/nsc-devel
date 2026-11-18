# Sobrecargas que difieren solo en `ref`/`out`/`in` (LKF-061)

## Problema

Con `F(ref int)`, `F(out int)`, `F(in int)` y `F(int)`, las llamadas `F(ref a)`, `F(out o)` y
`F(in a)` daban "ambiguous instance method call to 'F'" en O0 y O2, también para métodos estáticos.
Solo `F(a)` resolvía. Sonda original: `A:/nsc-fixes-evidencia/lkfb/ov`.

## Causa

Un argumento `ref`/`out`/`in` se analiza como la dirección del referente (`int*`). Los tres
parámetros por referencia también tienen el tipo `int*`.

`evaluateCallCandidateArgument` decidía la viabilidad de un miembro solo por conversión, puntero y
propiedad. La palabra clave escrita no se comparaba, así que las tres sobrecargas eran viables con la
misma conversión y ninguna dominaba a las otras. La comparación existía solo después de elegir
(`checkArgumentPassKinds`) y, para funciones de namespace, en la evaluación de candidatas.

## Corrección

Cuando el argumento lleva `ref`, `out` o `in`, solo son viables los parámetros declarados con esa
misma palabra clave. Un argumento sin palabra clave no cambia: no convierte al puntero.

Si hay una sola candidata con la palabra equivocada, se sigue emitiendo el diagnóstico preciso
("must be passed with 'ref'"). Con varias candidatas y ninguna con esa palabra, el error es "no viable
overload of 'F' accepts the argument types".

## Prueba

`nsc.compiler.pass-kind-overloads` (`tests/integration/pass_kind_overloads.py`):

- Válido: las cuatro sobrecargas estáticas y tres de instancia (`ref`/`out`/`in`), con efectos
  verificados (incremento, asignación de `out`, `in` sin cambios). Sale 42 en O0 y O2; antes no
  compilaba.
- Inválidos, cada uno con su mensaje exacto y sin "ambiguous":
  - una sola sobrecarga `ref` llamada con `in`;
  - una sola sobrecarga `out` llamada sin palabra clave;
  - `in` cuando solo existen `ref`/`out`;
  - ninguna palabra clave cuando solo existen `ref`/`in`.

Regresión dirigida, todo en verde:

- `nsc.compiler.out-parameter-paths`, `missing-return-paths` y `operator-in-temporary`;
- cc1: 72/72;
- `nsc_driver_tests`: 237 comprobaciones;
- `nsc_stage56_c2c4_language_completion_tests`: 82 comprobaciones.
