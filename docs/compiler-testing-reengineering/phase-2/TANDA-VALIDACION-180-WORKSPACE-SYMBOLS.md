# Tanda de validación 180 — `workspaceSymbolsContract`

## Alcance

- Componente: `Host.LanguageService`.
- Contrato: `CF-LANGUAGE-SERVICE-031`.
- Casos afectados: `TF-LANGUAGE-SERVICE-241`, `242`, `247` y `248`.
- Archivo: `tests/compiler-suite/host/modules/LanguageServiceQueries233To312Module.cpp`.

## Causa

El oráculo suponía dos resultados para una búsqueda normal, aunque el índice
publica tanto los métodos coincidentes como sus parámetros asociados. Tampoco
comprobaba orden, identidad calificada, contenido del límite unitario,
determinismo ni la variante con varios documentos.

## Corrección

La expectativa canónica exige cuatro símbolos en orden de fuente: método
`Add`, parámetro `delta`, y el mismo par para la segunda declaración indexada.
Se validan tipo e identidad calificada, el límite cero, el límite uno, búsqueda
sin distinción de mayúsculas, repetición determinista de todos los campos y la
entrada de ocho documentos. No se modificó el producto.

## Estado

Implementado. **PENDIENTE DE VALIDACIÓN FINAL** junto con el resto de la tanda;
no se compiló ni se ejecutó ninguna prueba durante este cambio.
