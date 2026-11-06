# Validación agrupada de la tanda 08 — 2026-11-06

Se ejecutó `nativesharp-interop\validar-tanda-08.cmd` con el NSC de desarrollo
en A:, build `a02f3209efef196f`. Las construcciones incrementales ON y OFF
terminaron. La sesión Clang, restauración de sintaxis, MIR y los controles de
identidad de código MIR reportaron éxito. El callback con enum C precompilado
pasó de O0 a O3 con header explícito y sin él; el caso sin header ocultó solo
la copia de salida. TLS encapsulado en DLL pasó O0–O3.

La batería terminó con código 1 y no cierra la tanda. Se observaron al menos:

- `pruebas-tanda-08-tls-c`: NSLink reportó `NSL1020` para `_tls_index` al
  enlazar `api.lib`. Diagnóstico en `A:/nsc-interop-build/tanda08-tls-c/link.log`.
- `pruebas-tanda-08-tls-c/directo`: el consumidor C de control no enlazó;
  LLD reportó `undefined symbol: tls_exported`. Diagnóstico en
  `A:/nsc-interop-build/tanda08-tls-directo/probe-link.log`.

No se ha demostrado que estas sean las únicas causas de fallo: parte de la
salida inicial se truncó y algunos validadores informan errores en archivos
individuales. Deben revisarse antes de afirmar cobertura. NSLink continúa
protegido y no se modificó. No se avanzó a la tanda 09.
