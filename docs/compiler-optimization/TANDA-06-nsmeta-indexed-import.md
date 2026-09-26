# Tanda 6: importación NSMETA indexada

## Objetivo

Evitar búsquedas lineales repetidas al materializar metadatos de bibliotecas precompiladas.

## Cambios realizados

- La búsqueda de tipos por clave usa `lower_bound` sobre una tabla ordenada.
- La resolución de padres de alcance público usa la clave canónica indexada.
- Las búsquedas de miembros, campos de respaldo y eventos comienzan en el grupo del tipo propietario y recorren solo ese grupo.
- La resolución de portadores anónimos y propietarios genéricos reutiliza el índice de tipos.
- El decodificador ordena establemente tipos por identidad y miembros por tipo declarante, manteniendo compatibilidad con metadatos antiguos cuyos constructores sintetizados fueron agregados al final.

## Archivos modificados

- `src/Interop/LibraryMetadata.cpp`

## Archivos nuevos

- Este informe. No se creó un índice persistido adicional porque las tablas NSMETA ya pueden ordenarse una vez al decodificar.

## Mejora esperada

La materialización deja de recorrer tablas completas para cada relación entre tipos y miembros. El beneficio aumenta con el tamaño de la biblioteca referenciada y con el número de referencias precompiladas.

## Verificación de la tanda

- Construcción de `nsc` en configuración Debug/O0: correcta.
- Prueba dirigida `nsc.stage48_2.library-metadata`: 290 de 292 comprobaciones correctas.
- Las dos comprobaciones restantes corresponden al golden archivado NSMETA 1.11 y fallan antes de las búsquedas modificadas por una discrepancia de huella ABI; quedan registradas para la evaluación final.
- No se ejecutó baseline completo.
