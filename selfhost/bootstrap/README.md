# Infraestructura de bootstrap

SH00 prepara el entorno sin portar todavía código del compilador.

## Archivos normativos

- `bootstrap.lock`: identidades y hashes de la semilla y herramientas externas.
- `generations.json`: identidad y reglas de aislamiento de las cuatro generaciones.
- `dependencies.md`: inventario de dependencias permitidas.
- `bootstrap.config.example.json`: ejemplo de configuración independiente de la máquina.
- `verify_environment.py`: verificador sin dependencias Python externas.
- `clean_selfhost.py`: limpieza dirigida, seca por omisión y protegida por marcador.
- `recovery.md`: recuperación de la última semilla válida.
- `schemas/`: contratos JSON versionados para lock, reportes y gates.

Las rutas físicas no se guardan en el lock. Se resuelven mediante variables de configuración y la
evidencia registra las rutas absolutas efectivamente utilizadas.
