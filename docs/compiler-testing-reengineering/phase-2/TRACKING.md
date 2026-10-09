# Tracking — Fase 2

| Ola | Alcance | Estado | Casos previstos | Implementados | Verificados |
|---:|---|---|---:|---:|---:|
| 0 | Infraestructura mínima | LISTA | 0 de producto / 3 sintéticos | 3 | 3 |
| 1 | Fundaciones host | EN CURSO | 747 | 736 | 736 |
| 2 | Sintaxis y semántica frontal | PENDIENTE | 1,197 | 0 | 0 |
| 3 | IR y optimización | PENDIENTE | 1,784 | 0 | 0 |
| 4 | Sesión, frontend y method bodies | PENDIENTE | 920 | 0 | 0 |
| 5 | Backend, ABI, runtime y enlace | PENDIENTE | 1,600 | 0 | 0 |
| 6 | Producto, driver y servicio | PENDIENTE | 1,112 | 0 | 0 |
| 7 | Compilador self-host | PENDIENTE | 1,027 | 0 | 0 |
| 8 | Infraestructura y oráculos self-host | PENDIENTE | 726 | 0 | 0 |
| 9 | Build, instalación y paquete de NSC | PENDIENTE | 474 | 0 | 0 |

Estados permitidos: `PENDIENTE`, `EN CURSO`, `LISTA`, `BLOQUEADA`.

Los conteos separan implementación de verificación: un caso rojo por defecto
conocido cuenta como implementado, se reporta `KNOWN_FAILURE` y no aumenta la
columna `Verificados`. No se elimina ni se convierte en `PASS`; tampoco bloquea el
flujo ordinario. Los casos que requieren infraestructura todavía inexistente no
cuentan como implementados.

| Último corte | Estado acumulado | Siguiente caso |
|---|---:|---|
| Tanda 00.11 — soporte no bloqueante de `KNOWN_FAILURE` | 736/747 | `TL-LANG-019` |
