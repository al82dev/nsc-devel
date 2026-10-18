# NSC 3.0.5 · R03C2C2C · declaraciones y valores locales

Fecha: 2026-10-18
Rama: `development`
Base: `0b2f89a78`
Estado: `CERRADO; VALIDACIÓN FOCAL PASS`

## SCOPE LOCK

### Objetivo autorizado

Extender el coordinador compartido de expression/block/statement para que las declaraciones locales cuyo valor
reingresa a expresión no consuman stack nativo por nivel. La hoja cubre únicamente:

- RHS de deconstruction local;
- expression statement contextual `await`;
- declaradores locales y sus inicializadores ordinarios o aggregate;
- listas de declaradores;
- aggregate initializers anidados, spreads y designadores;
- el borde función-local → body block → lowering lambda ya existente.

### Naturaleza y comportamiento protegido

Refactor interno no funcional y optimización algorítmica del parser. Deben permanecer idénticos gramática, AST,
ownership, orden de hijos, cursor, rangos, modifiers, auxiliary text, recovery, diagnósticos, API, ABI, NSMETA y
artefactos. No se autoriza cambiar sintaxis ni contratos de función local, inferencia `var`, references, `scope`,
`pin`, `raii`, `const`, `volatile`, `static`, aggregate o deconstruction.

### Archivos productivos autorizados

- `include/nativesharp/Parser/Parser.h`
- `src/Parser/Parser.cpp`
- `src/Parser/DeclarationParser.cpp`
- `src/Parser/MemberParser.cpp`
- `src/Parser/ParserContinuationCoordinator.cpp`

### Prueba, inventario y documentación autorizados

- `tests/compiler-suite/host/modules/ParserLambdaBlockContinuationCounterfactualModule.cpp`
- las tres filas canónicas de TP-PAR-555 en diseño, asignación y manifiesto;
- este expediente.

### Superficies protegidas

- `tests/unit/ProjectMethodBodyModelTests.cpp`, trabajo ajeno;
- MIR, LLVM, Driver, ABI y NSMETA;
- R03C2C2D–F, R03A, R03B y R03C3–C5;
- control estructurado, match/switch, try/using/defer, comprehensions e inline asm;
- fixtures y expectativas existentes.

### Criterio de cierre

Build de `nsc` y tres targets suite; TP-PAR-555 actualizado, Host.Parser y regresiones C; TL/CTest focales;
source registration, catálogo, umbral de funciones, símbolos y diff-check; tracking y commit aislado. Este
documento no declara esos gates hasta registrar evidencia reproducible.

## Causa y SCC cubierta

Después de R03C2C2B, una cadena como:

```text
handler0 => { int value0 = handler1 => { int value1 = ...; }; }
```

todavía salía de la worklist al entrar en `tryParseDeclarationStatement`, `parseVariableDeclaration`,
`parseVariableDeclarator` y `parseExpression`. Deconstruction repetía el mismo ciclo mediante su RHS. Los
aggregate initializers añadían otra SCC por `parseAggregateInitializer` → elemento aggregate anidado.

La función local tenía además un borde block profundo: su header y contratos no son recursivos, pero
`parseFunctionTail` consumía inmediatamente el body mediante `parseBlock`, regresando a statements y
expresiones. La conversión histórica a lambda debe ocurrir después de disponer del body completo.

## Diseño implementado

### Variables y deconstruction

`VariableFrame` conserva tipo, modifiers, primer declarador, wrapper `declaration-list`, índice y slot de
resultado. Cada declarador se crea en un paso acotado; su initializer se agenda como `Expression` o `Aggregate`
y sólo después se fija el range y se anexa en orden. El tipo se clona únicamente desde el segundo declarador,
igual que antes. `scope` continúa aplicándose sólo al primero y `static` a todos.

Deconstruction conserva el pattern síncrono existente; únicamente su RHS se convierte en child slot. El nodo,
bindings, `=`, `;` y range se ensamblan en el mismo orden observable.

`await` continúa siendo un expression statement. La worklist conserva literalmente el diagnóstico
`';' after await expression`.

### Aggregate initializer

`AggregateFrame` sustituye la llamada recursiva por `Aggregate`, `ContinueAggregate` y
`FinishAggregateElement`. Cada elemento conserva precedencia exacta:

1. spread `..expression`;
2. designador `name = value`;
3. elemento ordinario.

El valor es otro frame aggregate cuando comienza con `{`, o una expresión normal. El padre se ensambla después
del hijo y conserva token `DotDot`, argument name, source order, trailing comma, recovery por elemento nulo y
range delimitado por las llaves.

### Función local

Las firmas internas aceptan `deferBlockBody=false`, por lo que todos los consumidores históricos permanecen
idénticos. Sólo el coordinador solicita defer: parsea header, parámetros, genéricos, throws, FP, FFI y
constraints; agenda el block; restaura floating-point mode y range; y finalmente invoca el lowering existente
a declaración `var` con lambda. No se cambia la conversión, clonación, autoreferencia ni ownership histórico.

## Complejidad y límites de la afirmación

Para `T` tokens y profundidad `N` dentro de las rutas C enumeradas:

- tiempo: `O(T)`;
- espacio explícito heap: `O(N)`;
- stack nativo atribuible a esos bordes: `O(1)`.

No se declara stack constante para la clonación interna de una función local arbitrariamente profunda ni para
las familias D–F todavía pendientes. La prueba profunda de función local queda fuera de esta hoja; su borde se
verifica contractualmente a profundidad superficial.

## TP-PAR-555 ampliado

El mismo caso canónico pasa de cuatro a ocho probes aislados:

1. y 2. profundidad B por return y expression statement;
3. contratos de statements B;
4. recovery/defer shallow;
5. 32,768 inicializadores de variable enlazados por lambda-block;
6. 32,768 RHS de deconstruction enlazados por lambda-block;
7. 32,768 aggregate initializers anidados;
8. contratos de await, lista local static/scope y lowering de función local.

Los recorridos de los oráculos son iterativos. Las descripciones en diseño, asignación y manifiesto se amplían
sin crear otro case id ni modificar su contrato `CP-PAR-036`, componente, target o perfil.

## Validación

- Reconfiguración del producto y compiler-suite: PASS. Logs:
  `A:\temp\r03c2c2c-configure-nsc.log` y `A:\temp\r03c2c2c-configure-suite.log`.
- Build final de `nsc`: PASS, 19 pasos, incluidos todos los módulos Parser tocados y el contrato host runtime
  estático. Log: `A:\temp\r03c2c2c-build-nsc-final.log`.
- Build final de `nsc_component_tests`, `nsc_component_tests_destructive` y
  `nsc_component_tests_source_extended`: PASS, 93 pasos; sólo avisos preexistentes del shim privado.
  Log: `A:\temp\r03c2c2c-build-suite-final.log`.
- TP-PAR-555: PASS con ocho probes aislados. Log:
  `A:\temp\r03c2c2c-tp-par-555-final.log`.
- Host.Parser completo: PASS 555/555. Log:
  `A:\temp\r03c2c2c-host-parser-final.log`.
- Regresión dirigida TP-PAR-303…314: PASS 12/12. Log:
  `A:\temp\r03c2c2c-parser-regressions-final.log`.
- TL-LANG-299/325/326/351/352/370/377/484: PASS 8/8. El PATH se reconstruyó sólo para el
  proceso desde Machine+User y `where.exe nslink.exe` resolvió exactamente
  `A:\nslink-devel\bin\release\nslink.exe`. Log:
  `A:\temp\r03c2c2c-language-cases-final.log`.
- CTest `nsc.compiler-suite.host-parser-controller`: PASS 1/1. Log:
  `A:\temp\r03c2c2c-ctest-host-parser-final.log`.
- `validate_source_registration.py`: PASS. Log:
  `A:\temp\r03c2c2c-source-registration-final.log`.
- CanonicalCatalog: PASS 6,010 casos, 30 componentes y 682 contratos. Log:
  `A:\temp\r03c2c2c-canonical-catalog-final.log`.
- Umbral: toda función nueva o modificada queda en 60 líneas físicas o menos. El antiguo
  `parseFunctionOrGlobalVariable` tocado se subdividió mecánicamente en funciones de 29, 16 y 60 líneas;
  el resto queda por debajo. No se trasladó un monolito.
- Símbolos: declaración/definición/calls de las firmas diferidas y de
  `lowerLocalFunctionDeclaration` son coherentes; no hay definición duplicada.
- `git diff --check`: PASS, con sólo avisos esperados de normalización LF/CRLF.

Conclusión proporcional: R03C2C2C queda validado y cerrado. La afirmación de stack nativo O(1) se limita a las
rutas C enumeradas y a los tres contrafactuales profundos. R03C2C2D–F, R03A, R03B y R03C3–C5 permanecen
pendientes; no se atribuyen ni se ocultan en este cierre.
