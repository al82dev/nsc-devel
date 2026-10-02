# Tanda baseline 06F: snapshot determinista para SH43 y SH44

## Objetivo

Evitar que SH43 y SH44 mezclen revisiones distintas del árbol fuente cuando
una corrida larga coincide con una modificación local.

## Diagnóstico

Los objetos que el baseline reportó como no deterministas no contenían una
variación aleatoria de LLVM ni de NSLink. La primera mitad de la evidencia
incluía las cadenas semánticas antiguas `lvalue/rvalue`; la segunda incluía las
cadenas 3.0 `place/value`.

El objeto divergente de SH43 O2 y el objeto de la variante Unicode de SH44
tenían el mismo SHA-256 (`3231eaed49de5411f185564aa25a915707acf6cf6e56cdd8c3c093a7cdd8aeab`).
Ambos se generaron después de la corrección SH42, mientras que sus referencias
se habían generado antes. Por tanto, el gate estaba comparando fuentes
distintas leídas desde el árbol vivo.

## Cambios

- SH43 declara de forma explícita las cinco raíces que forman el compilador
  autoalojado.
- SH43 copia esas raíces una sola vez a un snapshot privado y todas las
  generaciones/repeticiones consumen exclusivamente esa fotografía.
- SH44 crea también una fotografía canónica única; sus variantes de ruta,
  locale, zona horaria, orden de creación y concurrencia se derivan de ella.
- Ambos gates comparan manifiesto, tamaño y SHA-256 del snapshot contra el
  árbol inicial.
- Ambos gates vuelven a medir el árbol al final. Si cambió durante la prueba,
  fallan con `source tree changed during SH43/SH44` en vez de diagnosticar una
  falsa no determinación del compilador.
- La atestación SH44 y el informe SH43 registran el hash de la fotografía.

## Archivos modificados

- `selfhost/contracts/repeated-fixed-point.contract.json`
- `selfhost/bootstrap/verify_repeated_fixed_point.py`
- `selfhost/bootstrap/verify_cross_machine_determinism.py`
- `tests/selfhost/test_bootstrap_infrastructure.py`

## Archivos nuevos

- Este documento de trazabilidad.

## Validación dirigida

- Helpers de snapshot SH43/SH44 y contrato de matriz SH44: 2/2 unitarias
  correctas en 0.028 s.
- Reproducción mínima O2: el objeto emitido por nsc2, el objeto emitido por
  nsc3 y la evidencia posterior a SH42 fueron idénticos con SHA-256
  `3231eaed49de5411f185564aa25a915707acf6cf6e56cdd8c3c093a7cdd8aeab`.
- SH07/IR, cuyo resultado no apareció antes de cancelar el baseline: 1/1
  correcto en 193.51 s con el `NS_LIB_ROOT` de A: usado por NSCBuilder.
- El probe temporal fue eliminado.

No se ejecutaron las matrices completas SH43/SH44 ni el baseline; quedarán
certificadas por la siguiente corrida del usuario.
