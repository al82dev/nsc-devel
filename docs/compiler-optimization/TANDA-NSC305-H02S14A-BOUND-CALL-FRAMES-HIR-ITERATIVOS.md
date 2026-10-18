# NSC 3.0.5 - H02S14A: frames HIR iterativos para bound calls

## SCOPE LOCK

- Objetivo exacto: introducir un frame comun, explicito e iterativo para las llamadas ligadas que
  ya estaban conectadas al worklist HIR antes de H02S14A.
- Naturaleza: refactor interno no funcional de Stage 34.
- Base correcta: `1e5eb8b9f0ec9afec4ee4344f7e30dd32171a8b4` (`H02S13T`).
- Rango evaluado: base anterior hasta el commit aislado de esta hoja.
- Archivos autorizados:
  - `src/Frontend/HIR/HirExpressionMachine.cpp`;
  - `tests/unit/ProjectOopLoweringTests.cpp`;
  - este expediente.
- Familias autorizadas: operador unario de usuario, operador binario de usuario, conversion de
  usuario, getter de propiedad y getter de indexer de usuario.
- Contratos inmutables: semantica, orden de evaluacion, HIR, diagnosticos, source ranges,
  conversiones, borrows, defaults, cleanup, retornos por referencia y validacion `noheap`.
- Archivos y subsistemas protegidos: ABI, NSMETA, parser, MIR, LLVM, driver y las familias
  H02S14B-L.
- Prueba puntual unica: selector `hir-bound-call-frame-depth` del target existente
  `nsc_project_oop_lowering_tests`.
- Acciones prohibidas: CTest, suites, minibaseline, baseline, deploy, instalacion, `push`, `pull`,
  `fetch`, escrituras fuera de `A:` y herramientas MSYS/MinGW/Cygwin.
- Terminacion: producto, selector e infraestructura focal documentados; target exacto compilado;
  selector unico aprobado; commit local aislado; auditoria estatica sin reentrada recursiva en la
  ruta ejercitada.

## Causa y frontera causal

`lowerBoundCall` conserva la semantica final correcta, pero las familias ya iterativas mantenian
protocolos distintos para visitar receptor y argumentos. Tres de ellas entregaban hijos pre-bajados
de vuelta al helper historico; las otras dos repetian parte de la preparacion y llamaban directamente
al finalizador. La fragmentacion impedia reutilizar una sola maquina de estados para las siguientes
hojas y hacia posible resolver targets, preparar argumentos o materializar receptores de forma
inconsistente.

La correccion se limita a un estado por frame que posee:

- binding y target resuelto una sola vez;
- etapa `Receiver`, `Arguments` o `Ready`;
- sintaxis estable de argumentos;
- indice monotono del siguiente argumento;
- receptor y argumentos HIR ya bajados;
- decision congelada de materializacion del receptor;
- acumulador de temporales owned para el cleanup final.

El worklist agenda exactamente un hijo cada vez. Nunca conserva referencias a elementos del vector
de frames a traves de un crecimiento del vector.

## Algoritmo e invariantes

1. Validar el binding con el diagnostico historico exacto.
2. Resolver `BoundCallTarget` una vez antes de visitar hijos.
3. Para una llamada de instancia, producir el receptor implicito sin descenso o agendar el receptor
   explicito con contexto de direccion solamente cuando corresponde.
4. Materializar un receptor struct temporal una sola vez al reanudar.
5. Visitar argumentos explicitos en orden fuente mediante un indice monotono.
6. Para operadores de usuario, solicitar direccion solamente para parametros no `Value`.
7. Aplicar conversion y borrow exactamente una vez al aceptar cada argumento.
8. Finalizar con `finishBoundCall`, unico propietario de defaults, metadata, cleanup y retorno por
   referencia.
9. Aplicar `noheap` y `flow-nonnull` en las mismas familias y fronteras historicas.

Para `R` receptores (cero o uno) y `A` argumentos, el recorrido cuesta `O(R + A)` tiempo, `O(A)`
estado explicito y `O(1)` pila nativa adicional por borde.

## Prueba focal

El selector `hir-bound-call-frame-depth` cubre exclusivamente la infraestructura comun real:

- 32,768 llamadas ligadas anidadas con destruccion iterativa del HIR;
- dos argumentos en orden fuente;
- contexto de direccion para parametros `In` de operador de usuario;
- conversion seleccionada antes de borrow y cleanup owned;
- default literal colocado despues de los argumentos explicitos;
- retorno por referencia observado como valor y como direccion;
- receptor struct temporal materializado exactamente una vez.

## Evidencia

- Configuracion aislada focal: `cmake -S . -B
  A:\NSC-devel-paralelo\builds\agente-1-s14a -G Ninja -DCMAKE_BUILD_TYPE=Release
  -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl`.
- Compilacion exacta: `cmake --build A:\NSC-devel-paralelo\builds\agente-1-s14a
  --target nsc_project_oop_lowering_tests -j 16`.
- Resultado de compilacion: `PASS`, `EXIT 0`.
- Ejecucion exacta: `NSC_PROJECT_OOP_LOWERING_FOCUS=hir-bound-call-frame-depth` sobre
  `nsc_project_oop_lowering_tests.exe`.
- Resultado focal: `PASS: 12 checks`, `EXIT 0`.
- Log de configuracion:
  `A:\NSC-devel-paralelo\logs\agente-1\h02s14a-configure.log`.
- Log de compilacion limpia:
  `A:\NSC-devel-paralelo\logs\agente-1\h02s14a-build-clean.log`.
- Log de prueba focal: `A:\NSC-devel-paralelo\logs\agente-1\h02s14a-focus.log`.
- Incidencia de infraestructura observada: el build incremental heredado mezclaba objetos
  `/MT` y `/MD`; se descarto como evidencia y no se limpio globalmente. La evidencia de cierre
  procede del directorio aislado anterior, construido completo para el unico target autorizado.

## Auditoria estatica de cierre

- Las cinco familias conectadas clasifican sin llamar a `resolveBoundCallTarget`.
- `initializeBoundCallFrame` es el unico punto de esas rutas que llama a
  `resolveBoundCallTarget`; el target queda poseido por el frame y no vuelve a resolverse.
- El protocolo comun agenda primero el receptor, despues cada argumento explicito por indice
  monotono y finalmente llama una vez a `finishBoundCall`.
- El bloque de reanudacion comun no llama a `lowerBoundCall`, `lowerExpression` ni a si mismo.
- `finishBoundCall` no llama a `lowerBoundCall`; consume los hijos ya bajados, agrega defaults y
  aplica cleanup/retorno por referencia. El default literal ejercitado se baja con el walker
  iterativo local de `lowerDefaultArgument`, sin reentrada al lowering de expresiones.
- Las referencias restantes a `lowerBoundCall` en `HirExpressionMachine.cpp` pertenecen a rutas
  fuera de H02S14A (setter y frontera historica ordinaria), por lo que no se anticiparon S14B-L.
- No se modificaron ABI, NSMETA, parser, MIR, LLVM, driver ni tracking global.
- No se ejecuto CTest, suite, minibaseline, baseline, deploy o instalacion.
