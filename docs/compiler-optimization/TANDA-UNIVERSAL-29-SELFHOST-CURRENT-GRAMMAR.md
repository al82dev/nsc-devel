# Tanda universal 29 - Gramática vigente en el compilador self-hosted

## Problema confirmado

`nsc-next --check-project` rechazaba 23 de las 42 unidades de `NativeSharp.Core`. El fallo no
provenía de las fuentes ni de una edición del lenguaje: el parser self-hosted no consumía tres
formas que el parser nativo ya reconoce y que la biblioteca estándar usa actualmente.

- `copy borrowed struct` y `copy struct` se interpretaban como una declaración cuyo tipo era
  `copy`.
- `throws E` quedaba después de la lista de parámetros y se interpretaba como el inicio del cuerpo.
- `: base(...)` y `: this(...)` quedaban después de un constructor y provocaban una cascada de
  errores en las jerarquías de excepciones.

## Cambios

- `selfhost/nsc/Parser/Parser.ns`
  - reconoce `copy` únicamente cuando es un modificador contextual inequívoco de tipo;
  - consume y conserva `throws E` en funciones, métodos, delegates, operadores y accesores;
  - construye nodos explícitos para inicializadores de constructor base/this;
  - no habilita `throws` en constructores: ese feature continúa fuera de esta tanda.
- `selfhost/nsc/Syntax/SyntaxTree.ns`
  - conserva el tipo declarado por `throws` y lo incluye en el dump sintáctico;
  - usa el segundo enlace del constructor para su inicializador y mantiene el cuerpo en el primero.

## Mejora

La cobertura no depende de excepciones por nombre de archivo ni de reescribir Core. Una sola
corrección gramatical cubre todas las unidades que usan estas formas y mantiene `copy`/`throws`
como palabras contextuales, por lo que identificadores ordinarios con esos nombres siguen siendo
válidos.

## Validación prevista

1. Compilar `nsc-next` únicamente en O0.
2. Ejecutar `--check-project` sobre los cuatro proyectos que comprueba SH40.
3. Ejecutar la batería CTest completa con el entorno NSLink/NS_LIB_ROOT explícito.
