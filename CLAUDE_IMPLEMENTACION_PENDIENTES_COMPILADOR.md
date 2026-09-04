# Orden de trabajo para Claude: pendientes del compilador NativeSharp

Fecha del mapa: 2026-09-04. Repositorio y única superficie de escritura:
`A:\nsc-devel`.

## Mandato y límites

Implementa en el compilador únicamente estos pendientes:

- `NSC-LIMIT-004`: igualdad y desigualdad de structs agregados.
- `NSC-LIMIT-005`: observabilidad y diagnóstico de la ruta optimizada.
- `NSC-LIMIT-007`: constraints genéricos `where T : class, Base, IInterface`.
- `NSC-LIMIT-008`: método de struct invocado sobre un valor devuelto por propiedad.
- `NSC-LIMIT-009`: tipos anidados dentro de clases y structs.
- `NSC-LIMIT-010`: `move(...)` hacia parámetros `owned` de constructores.

No modifiques Saru, sus bindings, sus scripts ni sus workarounds. Esos proyectos reportan
carencias; sus autores decidirán después cómo adaptar o restaurar su código. Los repros externos
pueden leerse como especificación, pero toda corrección y toda prueba permanente debe quedar en
`A:\nsc-devel`.

Reglas de ejecución:

1. El estado actual en disco es la fuente de verdad. No uses `HEAD`, un commit o un diff de Git
   como sustituto del contenido real.
2. Conserva cambios ajenos que ya estén en el árbol. No limpies ni reviertas archivos.
3. No hagas commits.
4. No ejecutes `all.bat`.
5. Sí puedes construir y ejecutar pruebas dirigidas del compilador, incluso en O2 cuando la prueba
   pertenezca a `A:\nsc-devel`.
6. No compiles el port de Saru ni bindings. `NSC-LIMIT-005` debe adquirir primero observabilidad
   dentro del compilador; no uses Saru como banco O2 dentro de esta entrega.
7. Trabaja un ID por vez. Antes de pasar al siguiente, deja verdes sus pruebas dirigidas.
8. Los números de línea siguientes son una fotografía. Si se desplazan, localiza la función o el
   símbolo nombrado; no apliques cambios por número de línea a ciegas.

Orden recomendado: `008`, `010`, `004`, `007`, `009`, `005`. Los tres primeros corrigen caminos
que ya existen sin cambiar el modelo del lenguaje. `007` y `009` amplían sintaxis, identidad y
semántica. `005` conserva al final la investigación específica del pipeline optimizado.

## Protocolo común de cada entrega

Para cada ID:

1. Añade primero una prueba mínima que falle por la causa reportada.
2. Implementa la semántica en frontend antes de tocar el backend.
3. Añade una defensa en el límite LLVM para que un error de frontend produzca un diagnóstico de
   `nsc`, nunca IR inválido.
4. Construye sólo los targets afectados.
5. Ejecuta sólo sus pruebas con `ctest -R` y `--output-on-failure`.
6. Comprueba O0 y O2 cuando el defecto tenga comportamiento optimizado o lowering LLVM, indicando
   qué significa la medición:
   - pipeline LLVM ejecutado en proceso por un unit test;
   - compilación de un proyecto con el `nsc.exe` instalado;
   - ejecución del PE producido.
   Esas tres mediciones no son equivalentes y no deben reportarse como si lo fueran.
7. Reporta archivos y líneas modificadas, pruebas ejecutadas y cualquier parte no medida.

Comandos permitidos como plantilla desde `cmd.exe`:

```bat
cmake --build A:\nsc-devel\build-clang --target NOMBRE_TARGET --parallel
ctest --test-dir A:\nsc-devel\build-clang -R "^NOMBRE_CTEST$" --output-on-failure
```

No sustituyas una prueba dirigida por `all.bat`.

### Regla para unit tests frente a fixtures

Los targets enumerados en este documento son unit tests enlazados directamente con las librerías
del frontend/LLVM. Para ellos basta reconstruir su target.

En Windows, los CTest que invocan el CLI y las fixtures `tests\cc1\*` no usan el `nsc.exe` del
árbol de build: `CMakeLists.txt:385-430` fija `NSC_TEST_COMPILER` a
`C:\native-sharp\bin\nsc.exe`. Por tanto:

1. Si añades o ejecutas una fixture/CTest que invoque el compilador, ejecuta primero
   `A:\nsc-devel\build_nsc.bat` para construir e instalar el binario medido.
2. Después construye el target de pruebas afectado y ejecuta su `ctest -R`.
3. Si sólo cambias/ejecutas un unit test enlazado estáticamente, no necesitas instalar `nsc.exe`.
4. No declares verde una fixture si no registraste qué binario instalado midió.

---

## NSC-LIMIT-008 — receiver struct producido por una propiedad

### Resultado requerido

Una expresión como `source.Current.Read()`, donde `Current` es un getter que devuelve un struct
por valor, debe materializar ese valor una sola vez en un temporal y pasar su dirección como
`this`. La propiedad no se convierte en almacenamiento asignable.

La semántica ya elegida por el compilador para un método mutante sobre un temporal es: copiar el
valor, mutar la copia y descartar la copia. No introduzcas en esta entrega análisis de `readonly` ni
de efectos.

### Causa comprobada

- `src\Frontend\ProjectOopHirModel.cpp:1997-2010`, dentro de
  `ProjectOopHirModel::lowerExpression`: un getter rechaza correctamente `addressContext`.
- `src\Frontend\ProjectOopHirModel.cpp:4090-4106`,
  `ProjectOopHirModel::isNaturallyAddressable`: devuelve `true` para todo
  `MemberExpression`, incluyendo propiedades.
- `src\Frontend\ProjectOopHirModel.cpp:4109-4164`,
  `ProjectOopHirModel::lowerReceiver`: ya sabe crear `TemporaryAddress`, pero no entra en ese
  camino porque el getter fue clasificado erróneamente como addressable.
- `src\LLVM\ProjectOopLlvmLowerer.cpp:5817-5845`: el lowering de `TemporaryAddress` ya crea el
  `alloca` y el `store` necesarios.

### Cambios exactos

1. En `include\nativesharp\Frontend\ProjectOopHirModel.h`, junto a la declaración actual de
   `isNaturallyAddressable`, conserva la firma o añade un helper privado específico para saber si
   un nodo es un accessor getter resuelto. No dupliques resolución de miembros.
2. En `src\Frontend\ProjectOopHirModel.cpp:4090`, cambia el caso `MemberExpression`:
   - consulta `callBinding(expression)`;
   - si el binding está resuelto y su miembro seleccionado tiene
     `isPropertyAccessor && isGetter`, devuelve `false`;
   - si es un campo real, devuelve `true` como hoy.
3. Trata `IndexExpression` con la misma distinción si el índice puede representar un indexer
   getter. Un elemento real de array/fixed-array sigue siendo addressable; un accessor que devuelve
   struct por valor no lo es.
4. No elimines el rechazo de `addressContext` en `ProjectOopHirModel.cpp:2005-2008`. Sigue siendo
   necesario para impedir asignación, `ref` u `out` sobre una propiedad.
5. No cambies `lowerReceiver`: al corregir la clasificación, su camino existente
   `TemporaryAddress` debe activarse sin una segunda implementación.
6. Verifica que el getter se baje exactamente una vez. No evalúes dos veces el receiver para
   obtener tipo y valor.

### Pruebas

Extiende `tests\unit\Stage40StructValueAbiTests.cpp`, cerca de la prueba existente de receivers
temporales (`TemporaryReceiver`, aproximadamente líneas 396-434):

- getter que devuelve struct pequeño y llamada que devuelve valor;
- getter que devuelve struct mayor de 8 bytes;
- getter encadenado;
- método `void` y método mutante sobre el temporal;
- contador/IR que pruebe una sola llamada al getter;
- campo struct equivalente que no recibe un temporal adicional;
- asignación o `out` sobre la propiedad continúa rechazado.

Target y prueba:

```bat
cmake --build A:\nsc-devel\build-clang --target nsc_stage40_struct_value_abi_tests --parallel
ctest --test-dir A:\nsc-devel\build-clang -R "^nsc.stage40.struct-value-abi$" --output-on-failure
```

### Cierre

- HIR contiene un `TemporaryAddress` para el getter y ninguno extra para un campo.
- LLVM verifica en O0 y O2.
- El getter se ejecuta una sola vez.
- La propiedad sigue sin ser lvalue.

---

## NSC-LIMIT-010 — `move(...)` hacia constructor `owned`

### Resultado requerido

`new Box(move(owner))` debe seleccionar un constructor cuyo parámetro correspondiente sea
`owned`, registrar el movimiento y dejar la fuente como moved-from. Debe usar exactamente las
mismas reglas de ownership que una llamada ordinaria.

### Causa comprobada

- `src\Frontend\ProjectMethodBodyModel.cpp:4973-5000`: `ObjectCreationExpression` analiza sus
  argumentos con `allowMove == false`; el comentario de líneas 4981-4992 documenta el hueco.
- `src\Frontend\ProjectMethodBodyModel.cpp:6390` (`analyzeCall`) y el bloque de argumentos
  `6606-6629`: las llamadas normales sí permiten move. En `6621-6622` el valor se pasa como el
  último `true` posicional de `analyzeExpression(..., false, false, true)`; no puede verificarse
  buscando el texto `allowMove`.
- `src\Frontend\ProjectMethodBodyModel.cpp:6815-6925`: la viabilidad de candidatos normales ya
  aplica compatibilidad de ownership.
- `src\Frontend\ProjectMethodBodyModel.cpp:7194-7223`: la llamada seleccionada registra las
  transferencias.
- `src\Frontend\ProjectConstructionModel.cpp:734-1033`, `bindConstruction`: la selección de
  constructores sólo usa tipos y conversiones físicas.
- `include\nativesharp\Frontend\ProjectMethodBodyModel.h:151-157`:
  `MethodExpressionTypeBinding` no conserva la información de ownership de `ExpressionResult`.
- `src\Frontend\ProjectMethodBodyModel.cpp:11555-11570`, `recordExpressionType`: al copiar el
  resultado semántico descarta ownership, transferencia y origen.

### Cambios exactos

1. En `include\nativesharp\Frontend\ProjectMethodBodyModel.h`, amplía
   `MethodExpressionTypeBinding` con valores predeterminados para:
   - `ownership`;
   - `ownershipTransfer`;
   - `sourceLocalOrdinal`;
   - `sourceParameterOrdinal`.
   Usa los mismos tipos que `ExpressionResult`; no inventes una representación paralela.
2. En `ProjectMethodBodyModel::recordExpressionType`, copia esos cuatro datos desde
   `ExpressionResult`. Actualiza inicializadores agregados afectados usando campos nombrados o
   defaults seguros.
3. En el caso `ObjectCreationExpression` de `ProjectMethodBodyModel.cpp:4973`, analiza cada
   argumento con `allowMove = true`, igual que el bucle de `CallExpression`. Escribe el argumento
   con comentario nombrado (`/*allowMove=*/true`) para que la intención pueda auditarse; el camino
   actual de llamadas usa un `true` posicional pelado en 6622. Este paso es el que debe crear el
   `MethodMoveBinding`; sin él, HIR llega a `MoveExpression` con `move == nullptr`. Conserva los
   diagnósticos de movimiento inválido, uso posterior y borrow activo.
4. Extrae de `analyzeCall` la decisión de compatibilidad entre un argumento y un parámetro a un
   helper compartido. El helper debe decidir, como mínimo:
   - `owned` exige una transferencia o una expresión que ya produzca un owner nuevo;
   - `move(...)` no es viable para parámetro prestado o sin ownership;
   - valor prestado no satisface `owned`;
   - tipos valor genéricos no ownables conservan la excepción que ya existe alrededor de
     `ProjectMethodBodyModel.cpp:6854-6889`.
5. Sustituye la lógica original de llamadas por ese helper y úsalo también en
   `ProjectConstructionModel::bindConstruction`. No mantengas dos copias de la regla.
6. En `include\nativesharp\Frontend\ProjectConstructionModel.h`, amplía
   `ConstructorCandidateBinding` para guardar la viabilidad/conversiones de ownership por
   argumento. Amplía `ObjectConstructionBinding` con las transferencias elegidas: constructor,
   parámetro destino y local/parámetro fuente.
7. En `ProjectConstructionModel.cpp:810-826`, lee la información ampliada de cada
   `MethodExpressionTypeBinding`.
8. En el bucle de candidatos de `bindConstruction` (aproximadamente líneas 891-924), aplica primero
   compatibilidad física y después compatibilidad de ownership. Un candidato físicamente válido
   pero con contrato de ownership incompatible no es viable.
9. En `ProjectConstructionModel.cpp:1029-1033`, copia al binding final las transferencias del
   candidato seleccionado.
10. La ruta real del move en HIR es ésta:
    - `ProjectOopHirModel.cpp:707-710` indexa los `MethodMoveBinding` en
      `moveBindingBySyntax_`;
    - `ProjectOopHirModel.cpp:2844-2900`, `case MoveExpression`, exige ese binding y construye
      `MoveArrayElement`, `MoveOwnedField` o `MoveValue`;
    - si falta, 2851-2855 produce exactamente
      `Stage 49 move expression has no semantic move binding`.
    El `case ObjectCreationExpression` de `ProjectOopHirModel.cpp:3842-3897` no contiene lógica de
    move: sólo recorre argumentos y llama `lowerExpression`. Mantenlo genérico; esa llamada debe
    entrar al `case MoveExpression` con el binding ya creado por el paso 3. No añadas un segundo
    `store null` en ConstructionModel ni en el caso de construcción.
11. Aplica el mismo helper a argumentos de inicializadores `this(...)` y `base(...)` en
    `ProjectConstructionModel.cpp` (camino previo a `bindConstruction`). Marca la fuente moved-from
    una sola vez y sólo después de seleccionar un candidato válido.
12. No hagas auto-move. La transferencia de una local owner existente sigue requiriendo
    `move(local)`. En cambio, `new T(...)` y un método que retorna `owned T` deben poder alimentar
    directamente un parámetro `owned`, como en llamadas normales.

### Diagnósticos obligatorios

Los mensajes deben distinguir:

- falta `move(...)` al pasar una local owner;
- valor borrowed enviado a `owned`;
- `move(...)` enviado a parámetro borrowed o sin calificador;
- doble movimiento o uso después de mover;
- movimiento con borrow activo;
- ningún constructor viable por ownership, nombrando el argumento y el parámetro.

### Pruebas

Añade casos a:

- `tests\unit\ProjectConstructionModelTests.cpp`;
- `tests\unit\Stage50_3OwnedParametersReturnsTests.cpp`;
- `tests\unit\Stage50_8OwnershipIrLoweringTests.cpp`;
- `tests\unit\Stage50_11OwnershipExecutionTests.cpp`.

Matriz mínima:

- positivo: local RAII movida a constructor `owned`;
- positivo: reenvío de parámetro `owned`;
- positivo: `new T(...)` y factory que retorna owner;
- positivo: constructor genérico;
- positivo: `this(...)` y `base(...)`;
- negativo: copia sin `move`;
- negativo: move hacia parámetro borrowed/sin calificador;
- negativo: doble move, uso posterior y borrow activo;
- runtime: construcción normal destruye exactamente una vez;
- runtime: constructor que lanza no duplica ni pierde destrucción.

Targets/CTest:

```bat
cmake --build A:\nsc-devel\build-clang --target nsc_project_construction_model_tests nsc_stage50_3_owned_parameters_returns_tests nsc_stage50_8_ownership_ir_lowering_tests nsc_stage50_11_ownership_execution_tests --parallel
ctest --test-dir A:\nsc-devel\build-clang -R "^nsc.project-construction-model$|^nsc.stage50_3.owned-parameters-returns$|^nsc.stage50_8.ownership-ir-lowering$|^nsc.stage50-11.ownership-execution$" --output-on-failure
```

### Cierre

- La resolución del constructor considera tipos y ownership.
- El HIR muestra un solo movimiento y la fuente queda moved-from.
- Los destructores se cuentan exactamente una vez en éxito y excepción.
- Llamadas ordinarias no cambian de comportamiento.

---

## NSC-LIMIT-004 — igualdad de structs agregados

### Resultado requerido

`left == right` y `left != right` sobre dos valores del mismo struct deben tener una semántica
definida y nunca llegar a `LLVMBuildICmp` como un agregado. Usa el contrato ya existente de
`Struct.Equals`: comparación bit a bit de cada hoja escalar declarada, recursiva, excluyendo
padding. No uses `memcmp` sobre el carrier completo.

### Causa comprobada

- `src\Frontend\ProjectMethodBodyModel.cpp:8874-9051`, `analyzeBinary`: después de operadores
  definidos por el usuario, cualquier comparación de operandos con el mismo `typeKey` se acepta.
- `src\Frontend\ProjectOopHirModel.cpp:2371-2479`: `BinaryExpression` termina creando un nodo HIR
  `Binary` genérico.
- `src\LLVM\ProjectOopLlvmLowerer.cpp:7321-7351`: `lowerBinary` llama
  `applyBinaryOperation`; todo no-flotante acaba en `LLVMBuildICmp`.
- `src\Frontend\ProjectOopHirModel.cpp:3418-3462`: `.Equals` de struct ya crea
  `AggregateEquals`.
- `src\Frontend\ProjectOopHirModel.cpp:4559-4624`, `appendAggregateLeaves`: ya recorre fields,
  structs anidados y fixed arrays sin incluir padding.
- `src\LLVM\ProjectOopLlvmLowerer.cpp:6464-6490`, `lowerAggregateEquals`: ya compara las hojas
  escalares.

### Cambios exactos

1. En `include\nativesharp\Frontend\ProjectMethodBodyModel.h`, declara un helper privado exacto
   para detectar `typeKey` de struct. No reutilices sin más `typeKeyHasDefinedDefaultStorage`, pues
   también cubre fixed arrays.
2. Implementa el helper junto a `aggregateIsUnion` en
   `src\Frontend\ProjectMethodBodyModel.cpp:482-518`, consultando el modelo de tipos.
3. En `analyzeBinary`, después de intentar un operador definido por el usuario y antes del bloque
   genérico `isComparison`:
   - si ninguno de los operandos es struct, continúa como hoy;
   - si sólo uno es struct o los `typeKey` difieren, emite `InvalidOperand`;
   - si el operador no es `==` ni `!=`, emite un diagnóstico específico;
   - si el agregado es union, recházalo de acuerdo con el contrato actual de `Struct.Equals`;
   - si ambos son el mismo struct y el operador es `==`/`!=`, devuelve `bool` y registra un
     `MethodCoreBinding` con `MethodCoreOperationKind::StructEquals` sobre el `BinaryExpression`.
4. No agregues un segundo algoritmo semántico. Reutiliza `StructEquals`; la clase de AST distingue
   una llamada `.Equals` de un operador binario.
5. En `ProjectOopHirModel.cpp`, dentro de `BinaryExpression` y antes de construir `Binary`, detecta
   ese core binding:
   - baja izquierda y derecha exactamente una vez;
   - crea `ProjectOopHirNodeKind::AggregateEquals` con `argumentCount = 2`;
   - llama `appendAggregateLeaves` para ambos operandos usando la misma forma que el camino de
     `.Equals`;
   - para `!=`, envuelve el resultado de igualdad en el nodo `Unary` existente con operación
     `Bang`.
6. Extrae un helper HIR compartido si de otro modo duplicarías el bloque de
   `ProjectOopHirModel.cpp:3418-3462`.
7. No cambies MIR ni `lowerAggregateEquals` salvo que una prueba demuestre una carencia: el camino
   `ProjectOopMirModel.cpp:1210-1232` ya transporta el nodo y su layout.
8. En `ProjectOopLlvmLowerer::applyBinaryOperation`, añade una defensa antes de FCmp/ICmp: si el
   tipo del operando es struct/agregado, devuelve error interno accionable, por ejemplo
   `aggregate comparison reached scalar binary lowering`. Esta defensa no sustituye el HIR.
9. No compares `[N x i8]`, no uses padding y no soluciones el reporte dejando de materializar
   métodos genéricos. La alcanzabilidad genérica es otra entrega.

### Pruebas

Extiende `tests\unit\Stage56C8TypeRootTests.cpp`, junto a las pruebas actuales de
`Struct.Equals` y su contrato de bits (aproximadamente líneas 390-520):

- `==` y `!=` fuera de un genérico;
- ambos operadores dentro de un genérico materializado;
- tamaños 1, 8, 16 y 32 bytes;
- struct con padding;
- struct anidado y fixed array;
- floats con igualdad de bits consistente con `Struct.Equals`;
- rechazo de union;
- rechazo de `<`, `<=`, `>` y `>=` sobre structs;
- aserción de IR: existe `aggregate-equals` y nunca aparece `icmp eq [`, `icmp ne [`.

Target/CTest:

```bat
cmake --build A:\nsc-devel\build-clang --target nsc_stage56_c8_type_root_tests --parallel
ctest --test-dir A:\nsc-devel\build-clang -R "^nsc.stage56.c8-type-root$" --output-on-failure
```

En este target, “O2” puede significar únicamente que el unit test ejecutó el pipeline LLVM dentro
del proceso; eso demuestra forma/validez del IR optimizado, pero no demuestra que un `.exe`
compilado por el CLI se ejecute correctamente. Reporta por separado:

1. unit test de frontend/HIR/LLVM sin optimización;
2. unit test que llama al pipeline LLVM O2 en proceso;
3. fixture de ejecución que compila y corre un PE en O0 y O2.

El cierre completo exige las tres. Si añades la fixture bajo `tests\cc1`, ejecuta primero
`build_nsc.bat`, porque CTest la compilará con `C:\native-sharp\bin\nsc.exe`, no con el ejecutable
del árbol de build.

---

## NSC-LIMIT-007 — constraints genéricos

### Alcance exacto

Implementa:

```text
where T : class
where T : BaseClass
where T : IInterface
where T : class, IInterface
where T : BaseClass, IInterface1, IInterface2
```

Permite varias cláusulas `where`, una por parámetro. En esta entrega no implementes `struct`,
`new()`, `unmanaged` ni `notnull`; si aparecen, produce un diagnóstico explícito de feature no
soportada, no un parseo parcial.

### Causa comprobada

- `include\nativesharp\AST\AstNode.h:43-45,75`: el AST sólo conserva nombres de parámetros.
- `src\Parser\Parser.cpp:603-628`: `parseGenericParameterList` no tiene constraints.
- Los rechazos actuales de `where` están en `Parser.cpp:1453,1517,1635,3365,3615` y varios están
  en una posición gramatical incorrecta.
- `include\nativesharp\Frontend\ProjectGenericModel.h:28-40`: el template genérico sólo conserva
  parámetros.
- `src\Frontend\ProjectGenericModel.cpp:240-276`: recolección de templates.
- `ProjectGenericModel.cpp:620-740`: especialización y sustitución.
- `ProjectGenericModel.cpp:844-878`: `cloneNode`; hoy sólo copia nombres genéricos.
- `src\Frontend\ProjectNameLookup.cpp:465-468,824-827`: los nodos genéricos originales se omiten.
- `src\Frontend\ProjectCompilation.cpp:294-324`: la materialización genérica ocurre antes de que
  lookup, herencia e interfaces estén completamente disponibles.

### Cambios exactos: AST y parser

1. En `include\nativesharp\AST\AstNode.h`, define una estructura, por ejemplo
   `GenericConstraintClause`, con:
   - nombre del parámetro;
   - rango del nombre;
   - `bool requiresClass`;
   - `std::vector<TypeSyntax*> typeConstraints`;
   - rango total de la cláusula.
2. Añade a `AstNode` `addGenericConstraint`, `genericConstraints` y
   `clearGenericConstraints`, más su vector privado.
3. Implementa esos accessors en `src\AST\AstNode.cpp`.
4. Actualiza `src\AST\AstDumper.cpp` para mostrar cada cláusula, la bandera `class` y los tipos.
5. Declara en `include\nativesharp\Parser\Parser.h` un helper
   `parseGenericConstraintClauses(AstNode*)` y helpers pequeños de recuperación si hacen falta.
6. Implementa el helper después de `parseGenericParameterList` en `Parser.cpp`:
   - consume cero o más `where`;
   - exige que el nombre pertenezca a `genericParameters()`;
   - exige `:`;
   - acepta `class` sólo en primera posición;
   - acepta uno o más tipos separados por coma;
   - detecta cláusula duplicada, constraint duplicado y parámetros desconocidos;
   - conserva todos los rangos para diagnósticos.
7. Mueve la lectura de constraints a la posición correcta de cada producción:
   - delegate: después de la lista de parámetros y antes de `;`;
   - class/struct: después de base/interfaces y antes de `{`;
   - método: después de `)` y antes del cuerpo/`;`;
   - función global: después de `)` y antes del cuerpo/`;`.
8. Elimina los cinco rechazos `generic constraints are deferred...` sólo cuando la producción
   correspondiente ya invoque el helper nuevo.

### Cambios exactos: modelo y validación

9. Amplía `GenericTemplateRecord` en
   `include\nativesharp\Frontend\ProjectGenericModel.h` para conservar constraints del template.
10. Amplía `GenericSpecializationRecord` para conservar:
    - template origen;
    - argumentos concretos;
    - rango/sitio que pidió la instanciación;
    - estado de validación.
11. En la recolección de `ProjectGenericModel.cpp:240-276`, copia las cláusulas AST al record sin
    resolver todavía los tipos.
12. En `cloneNode` (`ProjectGenericModel.cpp:844-878`), clona también los `TypeSyntax` de cada
    constraint aplicando el mapa de sustitución. No dejes punteros a sintaxis temporal.
13. No intentes validar jerarquías en el parser ni con una copia privada del name lookup. En la
    arquitectura actual, la información necesaria aún no existe cuando se materializa.
14. Crea una segunda fase semántica, preferiblemente `ProjectGenericConstraintModel`, con accessor
    propio en `ProjectCompilation`. No escondas la validación dentro de method bodies.
15. La cadena de fases es perezosa: cada fase llama a la anterior. Añade
    `ProjectCompilation::compileProjectGenericConstraints(projectFile, loadError)`, que primero
    llama `compileProjectInterfaces` y después construye/valida el modelo nuevo. En
    `src\Frontend\ProjectCompilation.cpp:354-365`, cambia
    `compileProjectMethodBodies` para llamar esa fase nueva en lugar de llamar directamente a
    `compileProjectInterfaces`. El punto exacto de inserción es entre el cierre de la comprobación
    actual en la línea 361 y la construcción de `methodBodyModel_` en la línea 363.
16. Para `class`, acepta tipos referencia; no cambies su calificador de ownership.
17. Para constraint de clase base, usa `ProjectInheritanceModel::derivesFrom` y acepta identidad.
18. Para interfaz, usa `ProjectInterfaceModel::classImplementsInterface` o
    `interfaceDerivesFrom`, según el argumento concreto.
19. Cada diagnóstico debe nombrar parámetro genérico, tipo argumento, constraint incumplido y sitio
    de instanciación.
20. El cuerpo especializado debe seguir resolviendo miembros mediante la sustitución concreta que
    ya hace la monomorfización. No añadas un segundo lookup de miembros “por constraint” salvo que
    una prueba positiva demuestre que la sustitución actual es insuficiente.
21. Nunca traduzcas `where T : class` a `owned T`; ownership permanece exactamente como fue escrito
    en cada uso.
22. Añade entrada de driver para la fase. Hoy hay 14 pares `--check-project-*` /
    `--dump-project-*`; una fase sin acciones propias queda sin entrada CLI y no participa de la
    misma cobertura que las demás. Implementa:
    - `--check-project-generic-constraints`;
    - `--dump-project-generic-constraints`.
23. Modifica `src\Driver\Driver.cpp` en los cuatro lugares de la cadena:
    - reconocimiento de acciones, líneas 230-243;
    - texto de ayuda, líneas 318-344;
    - booleanos de dependencia, líneas 769-791: inserta `genericConstraintAction` entre
      `methodBodyAction` e `interfaceAction`;
    - selección de `ProjectCompilation::compileProject...`, desde la línea 792, y salida de éxito/
      dump en el bloque que comienza alrededor de 972.
24. Añade dumper estable para el modelo nuevo: parámetro, argumento concreto, constraints resueltos,
    sitio de instanciación y resultado. El check debe informar al menos especializaciones validadas
    y fallidas.
25. Extiende `tests\unit\DriverTests.cpp` con ambos flags, salida de ayuda, check positivo,
    diagnóstico negativo y dump determinista.

### Pruebas

En `tests\unit\Stage44GenericsMonomorphizationTests.cpp`, reemplaza la prueba que hoy espera
`generic constraints are deferred beyond Stage 44` y añade:

- positivos para `class`, base, interfaz, interfaz transitiva y combinación;
- varias cláusulas para varios parámetros;
- uso de miembros de interfaz/base dentro del cuerpo;
- negativos por tipo valor contra `class`, base incorrecta e interfaz no implementada;
- parser: parámetro inexistente, cláusula duplicada, orden inválido y feature aún no soportada;
- especialización cruzando proyectos;
- `owned`, borrowed y referencia sin calificador conservan su semántica.

Añade pruebas específicas de herencia/interfaz sólo si la validación requiere nuevas consultas
públicas; no dupliques la matriz en tres targets.

```bat
cmake --build A:\nsc-devel\build-clang --target nsc_stage44_generics_monomorphization_tests nsc_driver_tests --parallel
ctest --test-dir A:\nsc-devel\build-clang -R "^nsc.stage44.generics-monomorphization$|^nsc.driver$" --output-on-failure
```

### Cierre

- El parser consume `where` en todas las posiciones admitidas sin diagnósticos derivados falsos.
- Una especialización inválida falla antes del análisis de cuerpos/HIR.
- Una especialización válida resuelve miembros y baja hasta LLVM/PE.
- Los constraints no alteran ownership.
- La fase puede ejecutarse y volcarse directamente desde el driver, sin depender de pedir una fase
  posterior.

---

## NSC-LIMIT-009 — tipos anidados

### Resultado requerido

Permite `class`, `struct`, `interface`, `enum` y `delegate` dentro de una clase o struct. El nombre
fuente es `Outer.Item`; la identidad canónica interna conserva `Outer+Item` mediante
`TypeIdentity`. Deben funcionar lookup, accesibilidad, metadata, mangling, HIR, LLVM y PE.

### Causa comprobada

- `src\Parser\Parser.cpp:1848`, `parseClassMember`: después de modificadores intenta parsear un
  tipo de miembro; `class`/`struct`/etc. caen al diagnóstico de líneas 2274-2282.
- `src\Parser\Parser.cpp:1067`: el gate de declaraciones top-level sólo consume `public` e
  `internal`.
- `src\Parser\Parser.cpp:1904-1905`: `parseClassMember` sí consume `public`, `internal`,
  `protected` y `private`.
- Los parsers reutilizables ya existen:
  `parseDelegateDeclaration` (1418), `parseStructDeclaration` (1476),
  `parseClassDeclaration` (1608), `parseInterfaceDeclaration` (1699) y
  `parseEnumDeclaration` (3425).
- `src\Frontend\ProjectCompilation.cpp:94-155`, `collectTypeSeedsInScope`: registra el tipo exterior
  y salta su bloque completo; nunca siembra tipos internos.
- `src\Frontend\ProjectNameLookup.cpp:454-520`, `predeclareNode`: construye `TypeIdentity` con
  `containingTypes = {}` y retorna sin recorrer hijos del tipo.
- `include\nativesharp\Compilation\TypeIdentity.h:10-26` y
  `src\Compilation\TypeIdentity.cpp:58-89` ya modelan `containingTypes` y separan anidados con `+`.
- `include\nativesharp\Frontend\ProjectNameLookup.h:35-39` sólo posee accesibilidad
  `Internal/Public`; no alcanza para nested private/protected.
- `ProjectNameLookup.cpp:897-1290` resuelve nombres por namespace, no por cadena de tipos
  contenedores.

### Cambios exactos: parser

1. Declara en `include\nativesharp\Parser\Parser.h` un helper que detecte y despache declaración
   anidada después de leer modificadores de miembro.
2. En `parseClassMember` (`Parser.cpp:1848`), antes de `canStartType`, reconoce
   `KwClass`, `KwStruct`, `KwInterface`, `KwEnum` y `KwDelegate`.
3. Reutiliza los cinco parsers existentes; no copies sus gramáticas. Aplica al nodo devuelto la
   accesibilidad/modificadores capturados por `parseClassMember`.
4. No reenvíes la declaración anidada al parser de declaraciones top-level: su gate de
   `Parser.cpp:1067` acepta deliberadamente sólo `public/internal` y rechazaría
   `private/protected`. El despacho nested debe partir de los modificadores ya consumidos en
   `parseClassMember` (1904-1905) y llamar directamente al parser de clase/struct/interfaz/enum/
   delegate correspondiente. Tampoco amplíes el gate top-level para aceptar private/protected;
   esos modificadores continúan siendo inválidos fuera de un contenedor.
5. Permite lo mismo en el bucle de miembros de struct (`Parser.cpp:1529-1535`, que hoy llama
   `parseClassMember`).
6. El default de un tipo anidado es `private`, no `internal` de módulo.
7. Rechaza modificadores incompatibles con diagnóstico exacto. No los ignores silenciosamente.

### Cambios exactos: identidad y lookup

8. Cambia `collectTypeSeedsInScope` para recibir además una cadena de tipos contenedores. Al ver un
   tipo, registra el exterior y recorre su cuerpo buscando sólo declaraciones de tipo anidadas; no
   confunda métodos o bloques con scopes de tipos.
9. Conserva dos formas:
   - forma fuente para lookup: `Outer.Item`;
   - identidad canónica/mangling: `TypeIdentity` con `containingTypes`, que produce `Outer+Item`.
10. Cambia las firmas privadas `ProjectNameLookup::predeclareNode`, `bindScope`, `bindNode`,
   `bindTypeSyntax` y los resolvers necesarios para transportar
   `std::vector<TypeNameSegment> containingTypes`.
11. En `predeclareNode`, al registrar un tipo construye `TypeIdentity` con la cadena recibida;
    después recorre solamente declaraciones de tipo hijas con `chain + outer(name, arity)`.
12. No uses siempre aridad cero: toma `genericParameters().size()` para cada segmento y tipo
    final. Si los tipos anidados genéricos no pueden soportarse correctamente en esta misma
    entrega, recházalos explícitamente en parser; no generes identidades ambiguas.
13. Añade un índice de nombres fuente anidados o una clave equivalente. No reemplaces el key
    canónico existente de `typesByModule_`.
14. Implementa este orden de búsqueda dentro de `Outer`:
    - tipo anidado directo de `Outer`;
    - tipos de contenedores exteriores, de dentro hacia fuera;
    - namespace actual;
    - imports/aliases existentes.
15. Para un consumidor externo, resuelve `Outer.Item` como tipo anidado antes de interpretarlo
    únicamente como namespace calificado. Mantén la detección de ambigüedad con namespaces.
16. Amplía `NamedTypeAccessibility` a `Private`, `Protected`, `Internal`, `ProtectedInternal` si la
    gramática ya ofrece esa combinación, y `Public`.
17. Aplica accesibilidad usando la relación entre tipo consumidor y tipo contenedor: private es
    visible en el contenedor y sus tipos anidados; public externamente; protected según herencia.
    Nunca conviertas private por defecto en internal.
18. Revisa `ProjectMemberModel.cpp:190-225` (`appendExpandedMemberNodes`) y el recorrido de partes
    alrededor de líneas 746-768: un nested type no es campo ni método. Debe registrarse por lookup
    y omitirse explícitamente como miembro ejecutable.
19. Verifica todos los lugares que imprimen o construyen nombres. Deben consumir `TypeIdentity`; no
    concatenes `Outer + "." + Item` manualmente para símbolos LLVM.

### Pruebas

Extiende:

- `tests\unit\ClassMemberSyntaxTests.cpp`;
- `tests\unit\ProjectNameLookupTests.cpp`;
- `tests\unit\CompilationModelTests.cpp`;
- `tests\unit\ProjectMemberModelTests.cpp`;
- una prueba de HIR/LLVM/PE existente apropiada.

Matriz mínima:

- nested class/struct/interface/enum/delegate;
- declaración dentro de clase y dentro de struct;
- `Item` dentro del contenedor y `Outer.Item` fuera;
- mismo nombre nested bajo dos outers;
- colisión entre namespace y tipo exterior;
- uso como field, parámetro, retorno y argumento genérico;
- public visible entre proyectos;
- private rechazado desde fuera y aceptado desde el contenedor;
- identidad esperada `Namespace.Outer+Item` y aridades correctas;
- metadata/mangling estable y módulo LLVM verificable.

```bat
cmake --build A:\nsc-devel\build-clang --target nsc_class_member_syntax_tests nsc_project_name_lookup_tests nsc_project_member_model_tests nsc_compilation_model_tests nsc_project_compilation_tests --parallel
ctest --test-dir A:\nsc-devel\build-clang -R "^nsc.class-member-syntax$|^nsc.project-name-lookup$|^nsc.project-member-model$|^nsc.compilation-model$|^nsc.project-compilation$" --output-on-failure
```

### Cierre

- AST y lookup representan nesting; no es sólo azúcar del parser.
- Los nombres fuente usan punto y los nombres canónicos usan `TypeIdentity`/`+`.
- Accesibilidad se aplica según el contenedor.
- HIR, LLVM y PE funcionan con un nested struct usado en una API real del test.

---

## NSC-LIMIT-005 — miscompile O2 de c38/b9 en `LLVMRunPasses`

### Resultado requerido

Diagnostica y cierra la diferencia entre el pipeline LLVM invocado por la API C de `nsc` y
`opt.exe` sobre el mismo IR. El defecto medido es una compilación silenciosamente incorrecta, no
una falta de progreso. La instrumentación debe responder qué configuración/pase produce la divergencia y la
regresión debe demostrar el resultado ejecutable correcto.

### Hechos comprobados

- `tests\cc1\c38_enum_ancho` devuelve 42 en O0 y devolvió 39 en O2: el nombre del enum estrecho con
  signo y valor `-3` tomó el literal de reserva.
- `tests\bug_triage\b9_enum_estrecho_o2` aisló el mismo caso. El valor `i8` seguía siendo `-3`
  antes de `EnumName`; no era ABI de retorno ni comparación de strings.
- El frontend emitía una comparación correcta contra `i8 -3` y el receptor se evaluaba una sola
  vez.
- `LLVMRunPasses` de LLVM 23 y `opt.exe` tomaron decisiones distintas sobre el mismo IR
  serializado: la API usada por `nsc` plegó hacia reserva y `opt.exe` lo plegó correctamente.
- El volcado pre-opt ya se hizo manualmente. No lo presentes como información aún desconocida.
- `src\LLVM\ProjectOopLlvmLowerer.cpp:6627-6660` documenta la medición y la mitigación actual:
  normalizar la decisión a `i64` y deduplicar aliases antes del `switch`.
- `src\LLVM\LlvmOptimizer.cpp:83-174` configura target, crea `LLVMPassBuilderOptions`, activa
  `VerifyEach` y llama `LLVMRunPasses` en 143-149.
- `src\Passes\PassManager.cpp` ya tiene límite de iteraciones. No hay evidencia que lo relacione
  con este miscompile.

### Cambios exactos

1. En `tests\unit\LlvmOptimizationTests.cpp`, añade un repro mínimo en memoria con la forma
   original problemática: enum/entero `i8`, constante negativa `-3` y selección del literal de
   reserva. Debe ejecutar `LlvmOptimizer::optimize(..., O2, ...)` —la misma API C que usa `nsc`— y
   comprobar el resultado optimizado, no sólo que el módulo verifica.
2. Conserva además c38 y b9 como pruebas end-to-end. El unit test reducido localiza LLVM; las
   fixtures prueban el compilador instalado y el PE. Ninguna sustituye a la otra.
3. Introduce una estructura opt-in de diagnóstico en `LlvmOptimizer`, por ejemplo
   `LlvmOptimizationDiagnostics`, con:
   - stream/sink de trace;
   - directorio opcional de artifacts;
   - `debugPasses`;
   - `dumpBefore` y `dumpAfter`.
   Los defaults deben mantener comportamiento y salida actuales.
4. En `src\LLVM\LlvmOptimizer.cpp:103-148`, vuelca el IR “before” después de
   `configureModuleForTarget` y justo antes de `LLVMRunPasses`. Así el artifact contiene el triple,
   data layout y cambios exactos que vio la API C.
5. Junto al IR, escribe un manifest determinista con:
   - versión de LLVM cargada;
   - triple y data layout;
   - pipeline textual (`default<O2>`);
   - nivel del TargetMachine;
   - cada opción de `LLVMPassBuilderOptions`, incluido `VerifyEach`;
   - identidad de programa/runtime.
6. Si LLVM 23 expone `LLVMPassBuilderOptionsSetDebugLogging`, decláralo en
   `src\LLVM\LlvmCApi.h` y actívalo sólo con `debugPasses`. Captura o enruta su salida sin ruido por
   defecto. Si esa API no permite un sink, documenta que sale por stderr y prueba sólo el flag.
7. Expón las opciones por driver con nombres inequívocos, por ejemplo:
   - `--trace-llvm-passes`;
   - `--dump-llvm-optimization <directorio>`.
   Modifica reconocimiento, ayuda y `DriverTests.cpp`. No reutilices `--dump-project-oop-llvm`,
   porque ese dump ocurre en otro límite y no registra las opciones reales del PassBuilder.
8. Pasa las opciones `Driver -> WindowsExecutableBuilder -> LlvmOptimizer` sin globales. Distingue
   artifacts `program` y `runtime`.
9. Con el repro mínimo, compara explícitamente API C y `opt.exe` usando el mismo binario LLVM 23,
   IR, triple, data layout y pipeline. Registra toda diferencia de opciones. No aceptes como
   “mismo pipeline” dos ejecuciones con TargetMachine o PassBuilder options diferentes.
10. Bisecta el pipeline sólo después de igualar esas entradas. La salida buscada es el primer pase
    tras el cual el valor cambia de la rama correcta a la reserva.
11. Si la causa es una opción/configuración de `nsc`, corrígela en `LlvmOptimizer`. Si es un bug
    confirmado de LLVM 23, conserva la normalización `i64` como workaround localizado, documenta
    versión y patrón, y deja un unit test del patrón original para detectar cuándo LLVM deja de
    necesitarlo.
12. No generalices la mitigación a todos los compares ni desactives optimizaciones globales. No
    cambies `default<O2>` para poner verde c38.

### Medición obligatoria

1. Unit test O2 en proceso: demuestra qué hace `LLVMRunPasses` con el IR mínimo.
2. Después de cualquier cambio al compilador, ejecuta `build_nsc.bat`: c38/b9 usan el
   `C:\native-sharp\bin\nsc.exe` instalado.
3. Compila y ejecuta c38 y b9 en O0 y O2. El resultado de las cuatro ejecuciones debe ser 42.
4. Conserva c39/aliases como control para no invertir la regla “primer nombre declarado”.
5. Reporta por separado IR pre-opt, IR post-opt y exit code del PE. Un módulo que verifica no
   demuestra semántica correcta.

Targets/CTest de la parte unitaria:

```bat
cmake --build A:\nsc-devel\build-clang --target nsc_driver_tests nsc_llvm_optimization_tests --parallel
ctest --test-dir A:\nsc-devel\build-clang -R "^nsc.driver$|^nsc.llvm-optimization$" --output-on-failure
```

Para c38/b9, instala primero con `build_nsc.bat` y usa sus runners dirigidos; no ejecutes
`all.bat`.

### Cierre

- El reporte describe el defecto como miscompile y conserva los exit codes observados.
- El repro mínimo identifica si la divergencia vive en configuración de `nsc` o en LLVM 23.
- Los artifacts permiten repetir exactamente la llamada de `LLVMRunPasses` en `opt.exe`.
- c38, b9 y controles de aliases devuelven 42 en O0 y O2 con el compilador instalado actualizado.
- La salida normal del compilador permanece sin trace ni artifacts.

---

## Definición final de terminado

El conjunto no está terminado sólo porque compile. Para cada ID debe existir:

- prueba roja previa que representa la feature;
- implementación en el nivel semántico correcto;
- defensa de lowering cuando corresponda;
- pruebas positivas y negativas;
- verificación O0 y O2 dentro de los tests del compilador cuando aplique;
- reporte de líneas realmente modificadas y resultados ejecutados.

No incluyas en el cierre trabajo en Saru o bindings. Cuando el compilador cumpla estos contratos,
los responsables de esos proyectos reescribirán o retirarán sus adaptaciones por separado.
