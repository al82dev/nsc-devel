# Tanda 06: fallo real de inicio de fuente

Una acción de prueba rechaza `BeginSourceFileAction`; se verifica que Clang
ejercite ese rechazo y que no se invoque `EndSourceFileAction` para una acción
no iniciada. Es independiente de los errores anteriores del controlador.
Pendiente de validación agrupada.
