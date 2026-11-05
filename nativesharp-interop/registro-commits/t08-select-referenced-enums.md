# Tanda 08: selección inicial de enums usados

La observación conserva los enums encontrados en enlaces de tipo o candidatos
de expresión y el selector los entrega junto a los records seleccionados. Se
retira la instalación incondicional de todos los enums del inventario, que
podía causar colisiones ajenas al código. Las colisiones con tipos nativos y
otros casos de demanda siguen pendientes de prueba; no hay validación ejecutada.
