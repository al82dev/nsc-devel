# NS304-T09-F04 — Extender primary constructors con orden de inicialización normativo

- **Estado**: PARCIAL (struct/record + orden textual) / residual (primary constructor de class)
- **Rama**: development

## Resultado

- El primary constructor de **struct/record** (`struct Point(public int x, public int y)`) ya está
  desazucarado (T02-F05): los parámetros se promueven a campos físicos y se genera un constructor
  ordinario que los asigna **en orden textual** (orden de inicialización normativo).
- Prueba `TL-LANG-337`: `new Point(3, 4)` + `p.x + p.y` → 7 → **PASS**.

## Residual

- **Primary constructor de class** (`class T(int x) { ... }`): el parser solo desazucara el caso
  `StructLayoutForm::Struct`; extenderlo a clases requiere decidir la semántica de captura (los
  parámetros quedan en alcance del cuerpo, no se promueven salvo uso).
