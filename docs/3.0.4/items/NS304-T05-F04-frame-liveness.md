# NS304-T05-F04 — Frame layout, discriminante, liveness y storage de locals suspendidos

- **Estado**: HECHO (liveness precisa def-use por suspensión implementada y verificada)
- **Rama**: development

## Resultado (liveness precisa — 2026-10-14)

- **Liveness precisa**: `ProjectOopMirModel` calcula por DFS def-use qué locals están vivos a través
  de cada `SuspendPoint` (definidos antes, usados después); solo esos reciben slot en el frame.
  Un local cuyo último uso precede toda suspensión queda **fuera** del frame (frame más pequeño).
- **Prueba O0** (`TL-LANG-433`, exit 10): `async Read(){ int live=10; int dead=5; yield; return live; }`
  — `live` sobrevive a la suspensión y `dead` se omite. Guard `TL-LANG-373` sigue PASS.

## Nota

F04 depende de F09 para ejercer la suspensión real vía await (la prueba usa `yield`).
