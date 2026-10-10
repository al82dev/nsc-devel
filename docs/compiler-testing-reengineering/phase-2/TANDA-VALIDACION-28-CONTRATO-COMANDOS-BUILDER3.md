# Tanda de Validación 28: contrato de comandos de NSC Builder 3

Fecha: 10 de octubre de 2026
Estado: CERRADA CON EVIDENCIA ESTÁTICA

## Riesgo cubierto

La GUI podía conservar una apariencia correcta mientras sus funciones de
configuración mezclaban árboles Debug/Release, propagaban rutas Windows no
normalizadas al cache CMake, retenían entradas opcionales antiguas o construían
un target distinto del autorizado.

## Prueba permanente

`test_nscbuilder3_commands.py` sustituye el descubrimiento de herramientas por
un recibo sintético y ejercita únicamente las funciones puras que construyen
comandos. Comprueba:

1. árboles separados para Debug y Release;
2. rutas CMake con `/` en todos los argumentos posteriores al ejecutable;
3. propagación exacta del NSC, NSLink y LLVM seleccionados;
4. retiro de cache cuando archivador, NSC de comparación o BuildId faltan;
5. propagación de esas tres rutas cuando existen;
6. compilador host estático en `O0`, con Clang para C y C++;
7. target `nsc` para el compilador y `nsc_component_tests` para la suite;
8. paralelismo configurado sin targets adicionales.

## Evidencia

- Pruebas dirigidas: `4/4` PASS.
- Suite Python completa del controlador: `80/80` PASS.
- Defectos demostrados en `nscbuilder3.py`: `0`.
- Cambios funcionales en `nscbuilder3.py`: `0`.
- Configuraciones o builds ejecutados: `0`.
- Casos de producto ejecutados: `0`.

La validación demuestra la construcción de los argumentos, no el resultado de
CMake ni del linker. La configuración, compilación y ejecución completas
permanecen para el cierre offline. No hubo baseline, despliegue ni escritura en
`C:`.
