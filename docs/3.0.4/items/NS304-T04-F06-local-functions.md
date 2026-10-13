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

1. ✅ Sema: `bindLambdaToDelegate` registra `local-function:<name>` en el scope del lambda con su
   `fn*` + `lambdaOrdinal`, y el cuerpo ya se auto-referencia SIN captura (`4dfcbc1f`).
2. **Falta HIR**: el local auto-referencial no tiene valor. El lowering debe emitir el puntero
   propio del lambda (una `FunctionAddress`/`LambdaFunction`) para ese local cuando
   `local.lambdaOrdinal == currentLambda.lambdaOrdinal`. Hoy el local es un `MethodNameBinding`
   sin valor → "LLVM code unit #1 failed: lowering failed".
3. Prueba `TL-LANG-326` (ya redactada; se re-registra al pasar).
