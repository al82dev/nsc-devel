# TL-LANG-085: la liberación entre módulos debe coincidir con el contrato que registró el objeto

Corrida de NSC Builder `20261115-210204-df82b231`, caso `TL-LANG-085` (CL-LANG-006), Debug y Release.

## Alcance

- Objetivo: que `ModuleLifetimeRegistry::unregisterObject` rechace una liberación cuyo contrato no
  coincide con el de un objeto vivo, sin perder el registro de varios tipos por módulo (LKF-416).
- Naturaleza: corrección de una regresión del producto.
- Archivos: `src/Language/CrossModuleLifetime.cpp`, `include/nativesharp/Language/ModuleLifetimeRegistry.h`.
- Protegidos: la prueba `TL-LANG-085` y las demás de CL-LANG-006, `ModuleLifetimeState` (TL-LANG-084 lo
  construye por agregado), mensajes de error, la huella de módulo.

## Cadena causal

- Entrada: registrar `Producer::Payload` y liberar el mismo contrato con `destroyThunk` distinto.
- Esperado: rechazo "cross-module object must be destroyed and deallocated by its producer contract"
  sin cambiar el conteo; después, la liberación correcta deja el conteo en 0.
- Observado: la liberación incompatible decrementaba el conteo a 0 y la correcta fallaba después.
- Causa: `bb603c7ed` (LKF-416) cambió la comprobación de liberación de la huella completa
  (`crossModuleLifetimeFingerprint`, que incluye tipo y thunks) a la huella de módulo, que no los
  incluye. La huella de módulo es correcta para registrar varios tipos, no para identificar qué
  contrato se libera.
- Corrección: el registro cuenta además los objetos vivos por huella completa de contrato
  (`typeContracts_`, miembro privado del registro). `registerObject` lo incrementa después de todas
  sus comprobaciones; `unregisterObject` exige una entrada viva para la huella completa, decrementa
  ambos conteos y borra la entrada al llegar a 0. La comprobación de módulo en la liberación queda
  cubierta: una huella completa registrada implica el mismo productor, ABI, asignador y política.

## Validación

- `nsc_component_tests` Debug y Release, los 58 casos de CL-LANG-006, CL-LANG-021, CA-AST-003,
  CF-DRIVER-010/011 y CF-INTEROP-043: todos PASS (`A:/nsc-fixes-evidencia/b3fix/host.txt`).
- Sin niveles O0–O3: el caso es de host (`NA`).
- `nsc` en `build-clang` (`NSC_WARNINGS_AS_ERRORS=ON`) compiló sin errores con el cambio.
- `nsc.ns3.t13.cross-module-lifetime` (dos tipos por módulo, segundo asignador rechazado, ruta de
  destrucción O0 con NSLink) pasó en `A:/nsc-fixes-evidencia/build` (`b3fix/ctest-t13.txt`).
