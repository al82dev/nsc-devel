# NS304-T04-F06 — Funciones locales genéricas, recursión y autoreferencia segura

- **Estado**: EN IMPLEMENTACIÓN (parser ✅, Sema pendiente)
- **Rama**: development

## Resultado parcial

- Parser: `type name (...)` parsea y baja a lambda natural (`parseLocalFunctionAsLambda`). El return
  unico se extrae como cuerpo de expresion. `TL-LANG-325` (no recursivo) PASA (exit 42).
- **Recursion (auto-referencia)**: el lowering conserva el tipo de retorno + marca `local-function`.
  El registro anticipado resuelve el nombre, pero el lambda trata `Fact` como CAPTURA del entorno y
  un lambda natural sin conversion es zero-size (no captura): "unconverted lambda ... cannot capture
  state". La auto-referencia exige que el cuerpo use el PROPIO puntero del lambda (self-reference
  constante), no una captura — feature aparte del registro anticipado.

## Plan (recursion)

1. Hacer que el lambda natural pueda referirse a si mismo (su propio `fn*` como constante), o
   permitir la captura del propio nombre sin closure.
2. Prueba `TL-LANG-326` (ya redactada; se re-registra al pasar).
