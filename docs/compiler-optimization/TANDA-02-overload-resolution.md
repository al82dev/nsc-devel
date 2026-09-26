# Tanda 2: resolución de sobrecargas

## Objetivo

Reducir asignaciones y comparaciones innecesarias en la resolución de llamadas, una ruta muy frecuente en código matemático y genérico.

## Cambios realizados

- Se añadió una ruta rápida para argumentos posicionales.
- Las aridades imposibles se descartan antes de reservar mapas por candidato.
- El análisis previo de tipos objetivo para lambdas solo se ejecuta cuando la llamada contiene lambdas.
- Se reservaron anticipadamente los vectores de candidatos evaluados, viables y sus ordinales.
- La selección de candidatos no dominados usa una frontera incremental en lugar de comparar siempre todos contra todos.

## Archivos modificados

- `src/Frontend/ProjectMethodBodyModel.cpp`

## Archivos nuevos

- Este informe. No fue necesario crear código nuevo; la optimización pertenece a la implementación existente de resolución de llamadas.

## Mejora esperada

Las llamadas posicionales comunes evitan estructuras auxiliares y el trabajo relacionado con lambdas. La frontera incremental reduce comparaciones de candidatos, especialmente en grupos grandes de sobrecargas.

## Verificación de la tanda

- Construcción de `nsc` en configuración Debug/O0: correcta.
- Prueba dirigida `nsc.project-method-body-model`: correcta.
- No se ejecutó baseline completo.
