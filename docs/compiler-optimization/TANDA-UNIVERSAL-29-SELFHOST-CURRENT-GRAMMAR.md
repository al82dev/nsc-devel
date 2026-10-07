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

## Evidencia ejecutada

### O0 dirigido

Se compiló `NativeSharp.Compiler.nsproj` en O0 dentro de
`A:/nsc-devel/build-test-nslink/selfhost-current-grammar` y el ejecutable resultante ejecutó
`--check-project` sobre los cuatro proyectos de SH40:

| Proyecto | Unidades comprobadas | Fallos de lexer/parser |
|---|---:|---:|
| NativeSharp.Core | 42 | 0 |
| NativeSharp.Core.Collections.Generic | 25 | 0 |
| NativeSharp.Core.IO | 4 | 0 |
| NativeSharp.LLVM.C | 3 | 0 |

Antes de esta tanda, el mismo chequeo de Core reportaba 23 de 42 unidades fallidas.

### Batería completa

Comando ejecutado con `NSC_LINKER`, `NSC_NSLINK`, `NSC_NSLINK_EXECUTABLE` y `NS_LIB_ROOT`
explícitos:

```text
ctest --test-dir A:/nsc-devel/build-test-nslink --interactive-debug-mode 0
      --output-on-failure --parallel 12
```

Resultado: **645 de 684 aprobadas; 39 fallos; 1197.25 s**. La lista de 39 es idéntica a la
corrida anterior con el entorno correcto: no se introdujo una regresión nueva. Las pruebas
profundas `nsc.selfhost.frontend`, `nsc.selfhost.semantics` y `nsc.selfhost.ir` aprobaron en
420.13 s, 53.86 s y 386.67 s respectivamente.

SH40 ya supera el parseo de todas las fuentes. Su fallo restante ocurre después, en el verificador
Python, por un conteo congelado de dependencias (`o0 dependency counts differ`); no es un rechazo
de la gramática ni invalida el cierre de esta tanda.
