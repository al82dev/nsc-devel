# NativeSharp 2.0 self-hosting — tanda 35: firmas e interoperabilidad base

## Identidad

| Campo | Valor |
|---|---|
| Estado | `DIRECTED-PASSED` — baseline acumulado pendiente |
| Gate | `SH33` |
| Fecha | 2026-09-20 |
| Rama | `development-claude` |
| Commit de entrada | `fcbc18f14` |
| Commit de cierre | este commit (`selfhost(tanda-35): lower native interop signatures`) |

## Implementación

El parser conserva `ref`, `out` e `in` en `ParameterDeclaration.Auxiliary`, representa su ABI como
puntero y envuelve cada argumento en una toma de dirección. `ParseParameters` acepta un `...`
terminal y lo fija en `SyntaxNode.IsVariadic`; `ScalarFunctionType` transmite ese bit a LLVM.

`BackendDeclarableFunction` separa declaración de emisión de cuerpo. Una función `extern C` sin
cuerpo entra al módulo, usa su nombre ABI exacto y puede resolverse desde una import library. Las
funciones NativeSharp mantienen el mangling estable. Una llamada variádica exige el prefijo fijo y
acepta argumentos adicionales; las conversiones declaradas sólo se aplican al prefijo.

La binding LLVM-C incorpora `LLVMSetFunctionCallConv` y `LLVMSetInstructionCallConv`; el lowering
puede fijar la misma convención en ambos extremos. La certificación ABI extendida de aggregates,
callbacks, DLL y vectorcall completo permanece en SH46.

## Gate SH33

La fixture modifica valores a través de `ref/out/in`, define y llama una función `extern C`
variádica, importa `GetProcessHeap` y comprueba que el handle no sea nulo. AST y COFF prueban los
modos, el bit variádico y los nombres exactos; O0–O3 enlazan y devuelven 42 con LLD y NSLink.

- [x] ABI por dirección para `ref`, `out` e `in`.
- [x] declaración/import `extern C` sin decorar.
- [x] firma y llamada variádica de prefijo fijo.
- [x] O0–O3 con ambos linkers.
- [ ] baseline acumulado final.
