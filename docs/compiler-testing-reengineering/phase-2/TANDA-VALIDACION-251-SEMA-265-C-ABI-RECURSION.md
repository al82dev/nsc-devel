# Validacion 251: terminacion de structs recursivos en C ABI

## SCOPE LOCK

- Objetivo: materializar unicamente `TS-SEMA-265`.
- Naturaleza: prueba dirigida del limite de terminacion de compatibilidad C ABI.
- Producto protegido: compilador, semantica, API, ABI y construccion de NSC.
- Pruebas protegidas: todos los casos distintos de `TS-SEMA-265`.
- Archivos autorizados: cuerpo de `TS-SEMA-265` y este documento.
- Acciones prohibidas: manifiesto, CMake, build, ejecucion y cambios de producto.
- Terminacion: rechazar structs vacios, incompletos y autorrecursivos; el ultimo
  debe regresar en vez de desbordar la pila o agotar el timeout.

## Caso

El caso conserva las entradas vacia e incompleta y exige `false` para ambas.
Agrega un struct completo cuyo unico campo tiene el mismo tipo del contenedor y
exige que `isCAbiCompatibleStruct` termine y regrese `false`.

La topologia ya existente ejecuta cada caso del manifiesto en un proceso hijo.
Por ello, un desbordamiento de pila, una terminacion anormal o un timeout del
producto quedan confinados a `TS-SEMA-265`; ninguno satisface el oraculo, que
requiere retorno normal con rechazo.

## Evidencia

- Caso: `tests/compiler-suite/host/modules/SemaAllocationType245To266Module.cpp`.
- Aislamiento: runner `HostExecutor` existente, sin soporte nuevo.
- Contrato: `CS-SEMA-025`.
- Validacion ejecutada: ninguna, por prohibicion expresa.
- Integracion pendiente: fila del manifiesto; el caso conserva
  `InfrastructurePending` y la etiqueta `process-isolation`.
