# T09-003: receptor C++ inicializado por expresión condicional

## Defecto

Una plantilla NativeSharp precompilada que inicializa el receptor con
`value > 0 ? T09Instance.makeAlternate() : T09Instance.make()` no publicaba el
método de instancia `size0`. El consumidor con encabezado fallaba en O0 con
`receiver/type 'type:T09Producer::T09Instance.Probe' has no compatible member named 'size0'`
(`A:/temp/nsc-t09-instance-object-size-e7fdpjo0/O0-condicional/resultado-con-encabezado.log`,
compilador `f70c0d1abff69ca6`). El control Clang y la muestra NativeSharp directa ya pasaban.

## Causa comprobada

- AST: `ExpressionIterationMachine::buildConditional` crea `ConditionalExpression`
  con `first` = condición, `second` = brazo verdadero y `third` = brazo falso.
- `selectResolvedTemplateInstanceMethods` (`CppSemanticRecord.cpp`) registra como
  inicializadores resueltos solo las llamadas-fábrica de las demandas directas
  (`demand.syntax`), con su propietario, USR, símbolo y referencia.
- `selectTemplateInstanceMethods` buscaba el inicializador de la declaración
  (`group[0]->first()`) directamente en ese mapa. En la muestra ese nodo es el
  condicional, que no está registrado; la fábrica de cada brazo tampoco es receptor
  directo de `->`. Por eso no se seleccionaba ningún método.

## Corrección

`CppTemplateInstanceMethodSelection.h` añade `resolveTemplateReceiverValue`. Un
valor se resuelve si es una fábrica resuelta o un `ConditionalExpression` cuyos dos
brazos son fábricas resueltas con el mismo conjunto de identidades (propietario,
USR del record y referencia de origen). Los brazos con identidades distintas, un
brazo no resuelto y los condicionales anidados quedan sin resolver; no se atribuye
un propietario por nombre del método. Se usa en el inicializador del receptor y,
por ser el mismo patrón, en el valor de una reasignación simple. Las comprobaciones
de declaración única, sombra, escape, asignación compuesta o por referencia y la
procedencia de la reasignación no cambian. No cambia NSMeta, Clang ni NSLink.

## Validación

- Construcción incremental de `nsc` y `nsc_cpp_c_identity_tests` en
  `build-tanda09` (build `3.0.0+build.20261114T153702Z.8a4ac2f7fd5db7fc`), código 0.
  Las advertencias emitidas pertenecen a líneas existentes.
- `nsc_cpp_c_identity_tests` con Clang 23.1.0: código 0
  (`A:/temp/t09-identidad-condicional/resultado.log`). Dos funciones nuevas
  comprueban la selección con brazos iguales, conjuntos iguales en otro orden, y los
  rechazos por conjunto distinto, otra referencia, otro USR, brazo no resuelto,
  condicional anidado, reasignación desde condicional con otro propietario, escape y
  sombra. La prueba real `checkGenericInstanceFactories` sigue pasando.
- `validar-metodo-instancia.py --precompilado`, O0–O3, con y sin encabezado:
  variantes `condicional` (inicializador) y `condicional-asignacion` (reasignación),
  con los dos brazos y resultados distintos (22 y 12). Estática:
  `A:/temp/nsc-t09-instance-object-size-fkz6pf99`; DLL:
  `A:/temp/nsc-t09-instance-object-size-o7g1fljb`. En las mismas corridas pasaron el
  control Clang, la muestra directa con el condicional (código 66) y las variantes
  previas: reasignación, receptor no resuelto rechazado, fábricas con argumento,
  sobrecargas, aridad, variádica, cadena, empates rechazados, símbolo explícito y
  convenciones.

## No verificado y límites

- La variante `condicional-asignacion` no se ejecutó con el compilador anterior; su
  fallo previo se infiere del código, no se observó.
- Un receptor directo `(c ? f() : g())->m()`, condicionales anidados, brazos con
  propietarios distintos y brazos que no son fábricas C++ siguen sin selección.
- No se ejecutaron la batería completa ni el gate de la tanda. T09-003 y la tanda 9
  permanecen abiertos.
