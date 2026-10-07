# Tanda universal 15: excepciones tipadas optativas

## Objetivo

Conservar `throws E`, propagación, filtros y catches como capacidades disponibles sin
obligar a todo programa a declarar o capturar excepciones tipadas.

## Cambios

- T07 ya no exige `NS3071` para llamadas, `throw`, getters o setters no manejados.
- Un catch no relacionado y un catch no exhaustivo pueden compilar; el programador
  decide si desea ese contrato dinámico.
- Permanecen los rechazos estructurales independientes de obligatoriedad: incompatibilidad
  de delegates, ampliación ilegal en overrides, throws en frontera C y filtros inválidos.
- `throws` y `match` se verifican como identificadores contextuales sin etiqueta de edición.
- Los proyectos del archivo dejaron de emitir `<Language>2</Language>` y
  `<Language>3</Language>`.
- La evidencia MIR comprueba identidad y orden de filtros sin exigir un valor textual
  específico del atributo derivado `exhaustive`.

## Archivo

- `tests/unit/NativeSharp3T07ErrorTests.cpp`

El nombre histórico de la prueba CTest se conserva. El cambio no elimina la semántica
de excepciones ni sus pruebas positivas; elimina únicamente su uso forzado.

## Validación dirigida

`nsc.ns3.t07.errors`: **PASSED**, 58/58 comprobaciones en O0. Las rutas positivas
de propagación, filtros, cleanup, metadata y ejecución continuaron funcionando; los
seis escenarios sin manejo explícito compilaron sin imponer el feature.

No se ejecutó baseline, certificación, promoción ni matriz O1/O2/O3.
