# NSC 3.0.5 — B09B: nombre de expresión iterativo

## SCOPE LOCK

- **Repositorio/rama/revisión base:** `A:\nsc-devel`, `development`, `5c1bd6cf2`.
- **Objetivo exacto:** eliminar la recursión restante de `expressionName` al reconstruir nombres
  calificados profundos dentro de `ProjectNamespaceFunctionModel`.
- **Naturaleza:** refactor correctivo no funcional de un único helper privado.
- **Archivos autorizados:** `src/Frontend/ProjectNamespaceFunctionModel.cpp`,
  `tests/unit/NamespaceFunctionLookupTests.cpp`, este expediente y tracking externo.
- **Prueba focal autorizada:** `nsc_namespace_function_lookup_tests` en A:.
- **Invariantes:** aceptar únicamente `IdentifierExpression` y cadenas de `MemberExpression` con
  receiver y texto no vacíos; producir el mismo nombre con puntos en el mismo orden; devolver
  `nullopt` para toda forma inválida; conservar bindings, candidatos, diagnósticos, sintaxis,
  semántica, API, ABI, NSMETA, CLI y artefactos 3.0.4.
- **Protegido:** worklist B09A ya cerrada, ProjectMemberModel B10, parser, method bodies, HIR, MIR,
  LLVM, driver, pruebas ajenas y consumidores.
- **Prohibido:** aumentar stack, imponer límite de segmentos, usar guard como solución, baseline,
  suite completa, deploy, instalación, push, pull o fetch.
- **Criterio de terminación:** una cadena calificada profunda pasa en el target focal; casos
  históricos conservan resultado; no queda self-call de `expressionName`; expediente y tracking
  coinciden con la evidencia.

## Diseño

El helper recorrerá iterativamente la espina izquierda, conservará cada segmento de miembro en
una pila y exigirá que la raíz sea un identificador no vacío. Después anexará los segmentos en
orden inverso. El tiempo y la memoria serán O(n), sin crecimiento del stack nativo.

## Estado

Terminado.

## Implementación

`expressionName` recorre la espina izquierda mediante un bucle. Conserva los segmentos como
`string_view`, calcula una sola vez el tamaño final, reserva el resultado y lo materializa desde
la raíz hacia la hoja. La implementación elimina tanto el crecimiento del stack como las
concatenaciones repetidas de prefijos crecientes.

Complejidad:

- tiempo O(n + bytes del resultado);
- memoria auxiliar O(n) en heap;
- una sola asignación prevista para el nombre final;
- cero self-calls.

## Evidencia

- El contrafactual de 2,048 segmentos ya alcanzaba B09B antes del cambio y quedó como gate de
  paridad profunda; después del cambio pasa dentro de `PASS: 12 checks`, exit `0`.
- Los casos históricos de nombre importado, calificado, alias y namespace actual pasan en el
  mismo target y conservan sus candidatos.
- La búsqueda estática confirma cero llamadas `expressionName(expression->first())`.
- Un contrafactual separado de 16,384 segmentos termina antes, durante `generics`, con
  `0xC00000FD`. No alcanza B09B y se transfirió a G10; no se presenta como fallo ni como prueba de
  esta corrección.
- No se ejecutó baseline, suite completa, deploy, instalación, push, pull ni fetch.
