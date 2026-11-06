# Captura estructurada de records C

La captura conserva identidad, destino, USR, clase struct/union, completitud,
dimensiones en bits y campos con sus tipos recursivos. Acepta forward-only sin
inventar tamaño y exige una definición conciliada para records completos.
Conserva un USR de campo observado posteriormente. No serializa ni reconstruye
todavía el tipo y queda pendiente de validación agrupada.
