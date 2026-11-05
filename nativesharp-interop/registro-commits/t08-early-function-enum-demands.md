# Tanda 08: observación temprana de demandas de funciones con enums

La observación preliminar construye modelos locales de miembros y funciones
cuando hay firmas con enums, recoge sus demandas y selecciona las dependencias
enum correspondientes antes del índice definitivo. Conserva errores ante
conflictos y no instala silenciosamente tipos homónimos. La interacción con
genéricos, snapshots, nombres no resueltos y compilaciones mixtas debe pasar la
validación agrupada; esta implementación aún no se ha ejecutado.
