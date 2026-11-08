# Captura de procedencia de enums

El catálogo actual de enums no conserva por separado la procedencia física.
Se prepara su captura desde el buffer analizado por Clang: ruta, SHA-256 y
desplazamiento, con caché de hash por archivo. Las ubicaciones de macro o
fuentes no disponibles quedan sin evidencia. Todavía no se serializa, no se
usa para conciliar y no se han ejecutado las pruebas agrupadas.
