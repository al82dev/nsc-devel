# NS304-T09-F04 — Extender primary constructors con orden de inicialización normativo

- **Estado**: CERRADO (struct/record + class, semántica promote-to-fields)
- **Rama**: development

## Resultado

- El primary constructor de **struct/record** (`struct Point(public int x, public int y)`) ya está
  desazucarado (T02-F05): los parámetros se promueven a campos físicos y se genera un constructor
  ordinario que los asigna **en orden textual** (orden de inicialización normativo).
- Prueba `TL-LANG-337`: `new Point(3, 4)` + `p.x + p.y` → 7 → **PASS**.
- **Class** (`class T(int x) { ... }`): mismo desazucarado promote-to-fields + constructor
  `promoted-primary-constructor`. Prueba `TL-LANG-344`: `new Point(3,4).Sum()` → 7 → **PASS**.

## Reconciliación (dos expedientes → uno)

El expediente `NS304-T09-F04-class-primary-constructor.md` es el subcaso de clase; queda absorbido
aquí (no cuenta doble). La semántica es **promote-to-fields** (consistente con el struct primario
cerrado), no la C# 12 params-in-scope (parámetros en scope sin promover). Decisión registrada.

## Residual

- **Semántica C# 12 params-in-scope**: fuera de alcance; NativeSharp usa promote-to-fields.
