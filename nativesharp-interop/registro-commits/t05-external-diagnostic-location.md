# Tanda 05: ubicación externa estructurada

Los diagnósticos de NSC pueden conservar una ubicación suministrada por un
compilador externo, sin fingir que pertenece a un buffer del SourceManager.
La ubicación sobrevive a `DiagnosticEngine::append`. La columna conserva la
unidad del compilador de origen; no se convierte a UTF-16 sin el texto correcto.
Este cambio requiere repetir la validación agrupada de la tanda.
