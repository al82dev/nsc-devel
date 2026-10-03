# NS3-T04-F01 — Declarative `where` constraints

## Estado

`TERMINADA`. NativeSharp 3 valida capacidades, contratos nominales e igualdades genéricas en la
declaración y en cada especialización, con una identidad canónica independiente del orden escrito.

## Qué se hizo

- Se amplió la cláusula `where` con `struct`, `new()`, `copy`, `move`, `lifetime`, `unmanaged` y
  `notnull`, conservando `class`, clase base e interfaz de NativeSharp 2.
- Se añadió `where T == U` para igualdad de tipos y `where N == valor` para igualdad de parámetros
  const. Los ciclos consistentes terminan y se validan sin recursión abierta.
- Cada obligación se comprueba sobre la especialización concreta. El diagnóstico nombra el
  parámetro, la plantilla, el argumento, la obligación incumplida y el módulo que declaró la
  plantilla.
- Las restricciones se deduplican, ordenan y codifican en una identidad canónica estable para
  caché e incrementalidad. Las combinaciones `class + struct` y `class + unmanaged` se rechazan
  como contradicciones de representación.
- La resolución de sobrecargas conserva los diagnósticos de constraints como diferidos: una
  candidata no elegida no contamina una llamada válida, pero la obligación causal aparece cuando
  no existe otra candidata aplicable.
- Los argumentos inferidos atraviesan exactamente la misma validación que los argumentos escritos.
- La clonación de especializaciones, resolución de nombres, dependencias y metadata preservan las
  nuevas cláusulas. Los marcadores const no se tratan como nombres de tipo.
- La edición 2 mantiene su contrato anterior; las nuevas capacidades exigen `Language 3`.

## Archivos modificados

- `include/nativesharp/AST/AstNode.h`: representación de capacidades e igualdad.
- `src/AST/AstDumper.cpp`: dump estable de las cláusulas nuevas.
- `src/Parser/Parser.cpp`: gramática, orden, duplicados y errores de declaración.
- `include/nativesharp/Frontend/ProjectGenericConstraintModel.h` y
  `src/Frontend/ProjectGenericConstraintModel.cpp`: normalización y validación semántica.
- `src/Frontend/ProjectGenericModel.cpp`: clonación, sustitución y dependencias.
- `src/Frontend/ProjectNameLookup.cpp`: resolución de tipos de constraint sin confundir constantes.
- `src/Language/GenericSemantics.cpp`: contradicciones de representación.
- `CMakeLists.txt`: registro de la prueba dirigida.
- `tests/unit/NativeSharp3T04GenericTests.cpp`: contrato puro de normalización.

## Archivos nuevos y por qué

- `tests/unit/NativeSharp3T04DeclarativeConstraintTests.cpp`: prueba productiva O0/NSLink de
  capacidades, igualdad, ciclos, inferencia, sobrecargas, diagnósticos y módulos separados.
- `docs/3.0/language/declarative-generic-constraints.md`: gramática y catálogo bilingüe.
- Este documento: evidencia del commit atómico.

## Mejora obtenida

Una API genérica puede expresar por adelantado la representación y las operaciones que necesita.
El error aparece en el sitio causal, no como un fallo posterior de lowering, y dos escrituras
equivalentes producen la misma identidad para la caché. El contrato también sobrevive al límite
de una biblioteca precompilada.

## Validación dirigida

- `nsc_ns3_t04_declarative_constraint_tests`: 14/14 con O0 y NSLink local.
- `nsc_ns3_t04_generic_tests`: 12/12.
- Fixture `c60_ns3_generic_solver`: compiló con el `nsc.exe` local, O0 y NSLink local; terminó con
  el valor esperado 42.

No se ejecutó baseline, CTest general, O1/O2/O3, instalación ni deploy.
