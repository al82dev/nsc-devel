# Huella ABI del catálogo C

La proyección ABI extrae el contrato C histórico de una envoltura válida, de
modo que ruta, huella del buffer y offset no cambien la ABI pública. La metadata
completa sigue protegida por el hash del payload. Si la envoltura no se puede
decodificar, la proyección conserva sus bytes y la verificación posterior debe
rechazarla. Pendiente de controles y validación agrupada.
