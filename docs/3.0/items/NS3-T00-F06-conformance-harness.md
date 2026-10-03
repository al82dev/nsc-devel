# NS3-T00-F06 — Harness de conformidad

## Resultado

El harness ejecuta manifests normativos o de regresión en directorios temporales aislados y
compara exit code, stdout, stderr, códigos `NS3xxx`, MIR y ABI. Cada repetición usa un directorio
nuevo; observaciones distintas producen `nondeterministic`, los timeouts producen `timeout` y los
contratos inválidos producen `invalid` con `NS3099`.

## Cambios

- IDs limitados a identificadores seguros de 1–128 caracteres.
- inputs y archivos MIR/ABI deben ser relativos y permanecer dentro del caso.
- inputs duplicados se rechazan.
- `expected.diagnostics` sólo admite códigos estables `NS3xxx`.
- `mirContains` y `abiContains` requieren el archivo de evidencia correspondiente.
- el autocheck cubre además traversal, diagnóstico inválido y evidencia ausente.

## Pruebas dirigidas

- `tools/ns3_conformance.py --self-test`: `PASS: 8 conformance harness checks`.
- manifest `t00.edition3.project-identity`: `PASSED`.
- manifest `t00.edition3.production-pipeline`: `PASSED`.
- compilador probado: `A:\nsc-devel\build-test-nslink\bin\nsc.exe`.
- optimización de los manifests: `O0`.
- baseline: no ejecutado.

## Archivos modificados

- `tools/ns3_conformance.py`

## Mejora

Una prueba de conformidad no puede leer o escribir evidencia fuera de su sandbox, aceptar un código
diagnóstico informal ni declarar checks MIR/ABI sin el artefacto que los sustenta.
