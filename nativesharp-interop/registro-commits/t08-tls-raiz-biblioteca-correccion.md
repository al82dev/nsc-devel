# Raíces de biblioteca en la selección anticipada

La inspección anticipada busca cada dependencia NS junto a la metadata, luego
en `LibraryReferenceRoot` y en la raíz de respaldo, como la preparación normal.
Si no logra resolver los archivos sigue eligiendo LLD conservadoramente; la
validación existente sigue verificando contratos y emitiendo diagnósticos.
Se aplica a `nsc build` y emisión directa. Pendiente de validación agrupada.
