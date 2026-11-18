# Copia, paso o retorno de una referencia destruida guardada en un arreglo

Rama `development-main-fixes`. Campaña: mejora "referencias de clase guardadas en arreglos" de
`X:\Examples-NSC\Problemas\mejoras.md`. Evidencia: `A:\nsc-main-fixes-evidencia\claude\probes`
(`SUMMARY-despues-copia.txt`, `check-*.log`) y `A:\nsc-main-fixes-evidencia\claude\CAUSA-copia-destruida.txt`.

## SCOPE LOCK

- Objetivo: diagnosticar el uso de un elemento de arreglo cuyo objeto está demostradamente destruido
  cuando el valor se copia a un local, se asigna, se pasa como argumento o se retorna.
- Naturaleza: correctiva (seguridad de vida), dentro del mecanismo de replay ya existente.
- Debe permanecer idéntico: diagnósticos existentes de receptor, lectura y escritura de campo;
  comparaciones del elemento; lecturas con dueño vivo o después de sobrescribir con un objeto vivo;
  todo programa sin el patrón.
- Revisión de partida: e81d1b265. Base de campaña: 152654e5a0e2d84d4c2f245ea7590488f0497430.
- Archivos modificables: `src/Frontend/Bodies/MethodBodyCallableLifecycle.cpp`,
  `src/Frontend/Bodies/EscapeAnalyzer.cpp`, `include/nativesharp/Frontend/ProjectMethodBodyModel.h`,
  `tests/unit/NativeSharp3T06EscapeSummaryTests.cpp`, este documento.
- Protegidos y prohibiciones: los del documento 01 de esta carpeta.

## Reproducción (antes del cambio, compilador de la copia en e81d1b265)

Sondas en `claude\probes`, analizadas con `--check-project-constructions` (nunca se ejecutaron):

- `P04-copia-local-tras-delete`: `MyClass copy = references[0]; copy.GetValue();` después de
  `DeleteObject(move(first))` — aceptado sin diagnóstico.
- `P22-paso-a-funcion-tras-delete`: `Reader.Read(references[0])` después del `delete` — aceptado.

## Causa

1. `diagnoseDeadArrayReferenceReceivers` solo reportaba observaciones `Dead` cuyo uso era receptor de
   un método de instancia (o campo, en la comprobación tardía). Una copia o argumento no tenía bandera.
2. `arrayReferenceLinearPublicationCoverage` (requisito para cualquier diagnóstico inmediato) no
   reconocía un local de referencia inicializado con un elemento del arreglo ni con `null`, un
   argumento que es un elemento del arreglo, la asignación `local = refs[k]` ni `return refs[k]`.

## Cambio

- `ArrayReferenceReplayObservation::valueCopy`: el elemento es inicializador de un local, valor de una
  asignación `=`, argumento de una llamada resuelta (método o función de namespace) o expresión
  retornada (`collectArrayReferenceValueCopies`, recorrido iterativo sin entrar en lambdas).
- Diagnóstico nuevo, con las mismas condiciones que el de receptor (estado `Dead`, sin validación
  tardía pendiente, cobertura lineal, `stopsBeforeRead` falso):
  `reference to a destroyed object cannot be copied, passed or returned`.
- Cobertura lineal: lectura `simpleIndexRead` (arreglo nativo local simple, índice literal o local
  estable, elemento de tipo referencia que no es arreglo) admitida en inicializador de local ordinario
  de referencia, argumento de llamada simple, asignación a local ordinario de referencia y último
  `return` de un callable que devuelve referencia; inicializador `null` admitido para locales de
  referencia. Leer un elemento no escribe ranuras ni publica el arreglo; una escritura posterior con
  ese valor sigue volviendo `Unknown` el estado en el replay (fuente que no es dueño).

## Contrato

Un elemento cuyo objeto está demostradamente destruido no puede copiarse, pasarse ni retornarse.
Comparar el elemento (por ejemplo con `null`) sigue siendo válido. El estado `Unknown` o `MaybeDead`
no produce este diagnóstico.

## Pruebas

- `nsc.ns3.t06.escape-summaries` (`valueCopyControl`): rechazo de copia a local, argumento, asignación
  y retorno mediante alias; aceptación de copia y argumento con dueño vivo, comparación con `null`
  después de destruir y copia tras sobrescribir con objeto vivo. Pasan además todas las expectativas
  previas de cobertura de ese archivo (sin modificarlas).
- `nsc.stage50_7.borrow-escape-analysis` y `nsc.class-value-origins` pasan.
- Sondas: P04 y P22 pasan a rechazarse con el diagnóstico nuevo; P05, P06, P07, P08b, P09b y P20
  (válidos) siguen aceptados; P01, P02, P13 y P21 conservan su diagnóstico.

## Límites (pendientes reales, evidencia en `SUMMARY-despues-copia.txt`)

Siguen aceptados sin diagnóstico, aunque son inválidos o posiblemente inválidos:

- P19: objeto `raii` destruido al final de un bloque interno mientras el arreglo sigue vivo (el replay
  no modela la limpieza de fin de ámbito y el callable no es candidato).
- P14: escritura en el arreglo hecha por una función intermedia (resumen de un solo nivel).
- P16: destrucción mediante un método virtual; P17: destrucción precedida de una llamada en el callee.
- P18: arreglo guardado en un campo.
- P10: índice calculado por una llamada; P11: destrucción condicionada por un valor de runtime.
- La validación tardía (`ArrayReferenceLatePathProof`) sigue cubriendo solo receptores de método.
- Copias como argumento de construcción (`new T(refs[0])`) no se marcan.

P03, P08, P09 y P12 son sondas mal formadas para este propósito (el compilador las rechaza por otra
regla: copiar un dueño RAII o mover dentro de un ciclo); P08b y P09b las sustituyen.
