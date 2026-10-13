# NS304-T10-F04 — Validar formato consteval e interpolación constante pura

- **Estado**: CERRADO (literal holes consteval + validación de formato)
- **Rama**: development

## Resultado

- El handler estático T01-F06 (`parseStaticInterpolation` + `renderBuiltinStaticInterpolation` en
  `src/Frontend/ProjectMethodBodyModel.cpp`) internó el template completo cuando todo hueco es un
  escalar de tiempo de compilación, validando el formato en el camino. Quedó cubierto por el test.
- Prueba `TL-LANG-336`: `$"answer={42}"` → "answer=42" (longitud 9) → **PASS**.

## Nota

- Huecos con **expresiones constantes** (`{40 + 2}`) requieren evaluación consteval de expresiones
  (no solo literales); es una capacidad aparte, no exigida por este ítem.
