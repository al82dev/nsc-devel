# Correcciones del baseline 01

## Contexto

La primera ejecución completa de CTest con `lld-link` después de las catorce tandas reportó
25 entradas fallidas. Los logs demostraron que varias entradas self-host repetían subconjuntos
ya fallidos y que la mayor parte de los casos se concentraba en tres regresiones comunes.

## Cambios

- El descubrimiento de thunks para callbacks de una partición LLVM ahora inspecciona únicamente
  las funciones definidas por esa partición. El módulo coordinador conserva el descubrimiento
  global y la propiedad de los cuerpos de los thunks.
- También se consideran acciones de cleanup al descubrir callbacks de una función.
- La lectura de NSMETA verifica la huella ABI contra el orden original codificado por el productor
  y normaliza las tablas para búsquedas indexadas solamente después de autenticarla.
- Los logs automáticos `error.log`, `error1.log`, etc. continúan publicándose, pero su creación ya
  no añade texto a `stdout` ni `stderr`; los diagnósticos públicos permanecen compatibles con el
  compilador self-hosted.
- El índice de miembros se mantiene desde el momento en que se registra cada símbolo. Esto
  permite que un inicializador enum resuelva miembros anteriores durante la construcción del tipo
  sin volver a búsquedas lineales ni admitir referencias hacia adelante.

## Archivos modificados

- `src/LLVM/ProjectOopLlvmLowerer.cpp`
- `src/Interop/LibraryMetadata.cpp`
- `src/nsc/main.cpp`
- `src/Frontend/ProjectMemberModel.cpp`

## Archivo nuevo

- `docs/compiler-optimization/BASELINE-CORRECTIONS-01.md`: registra la causa, alcance y efecto de
  esta corrección posterior a las tandas.

## Mejora y estabilidad

Se conserva la reducción de declaraciones LLVM y la indexación de metadata sin sacrificar los
callbacks entre particiones ni la compatibilidad histórica de NSMETA. Los logs automáticos dejan
de modificar el contrato observable de diagnósticos, requisito de los gates diferenciales de
self-hosting.

## Verificación dirigida

- `nsc` y los targets dirigidos se construyeron correctamente en `build-enhancement-o0`.
- `nsc_stage51_enum_oop_support_tests.exe`: 26/26 comprobaciones correctas.
- El fixture `debug_pdb` emitió y verificó su objeto COFF en O0 con 12 unidades LLVM; desapareció
  `C1.3 callback boundary target is unavailable`.
- El golden `historical-1.11.nsmeta` atravesó checksum y fingerprint y alcanzó correctamente la
  frontera incompatible `object ABI expected 2, found 1`.
- Dos fallos consecutivos publicaron `error.log` y `error1.log`; el diagnóstico de
  `--definitely-unknown` no incluyó texto adicional.
- SH42 superó la comparación que antes fallaba, pero el verificador completo se detuvo después de
  un minuto para no convertir la validación dirigida en una ejecución extensa. No se declara SH42
  completo como aprobado.
- No se ejecutó el baseline ni una matriz de optimizaciones.
