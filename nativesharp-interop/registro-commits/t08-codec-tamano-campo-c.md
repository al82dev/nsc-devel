# Límite del tamaño de campo C

Para campos que no son bitfields, el codec verifica que el tamaño observado del
tipo no exceda el espacio restante del record. Los campos de tamaño desconocido
se mantienen representables; esta regla no sustituye la validación completa.
