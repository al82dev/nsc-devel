# NS304-T04-F02 — Retorno explícito, atributos, calling convention y efectos en lambdas

- **Estado**: CERRADO
- **Rama**: development

## Resultado

1. **Retorno explícito** ✅ (`(int x) -> int => ...`).
2. **Efectos `noheap`/`unsafe`** ✅ (prefijos + `signature.isNoHeap/isUnsafe`).
3. **Calling convention** `extern C` ✅ (prefijo + `signature.callingConvention`).
4. **Atributos** ✅ (`[Attr] (x) => ...` se desambigua de la captura `[copy x]`; se guarda como
   `attribute:Attr` en el auxiliaryText). `TL-LANG-327` PASA (exit 42).
