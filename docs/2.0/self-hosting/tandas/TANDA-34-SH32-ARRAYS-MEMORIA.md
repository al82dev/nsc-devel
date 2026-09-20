# NativeSharp 2.0 self-hosting — tanda 34: arrays y memoria nativa

## Identidad

| Campo | Valor |
|---|---|
| Estado | `DIRECTED-PASSED` — baseline acumulado pendiente |
| Gate | `SH32` |
| Fecha | 2026-09-20 |
| Rama | `development-claude` |
| Commit de entrada | `e7cbfca46` |
| Commit de cierre | este commit (`selfhost(tanda-34): lower heap and stack arrays`) |

## Implementación

`LlvmBackend.EmitScalarExpression` baja `NewExpression` calculando `cantidad * sizeof(T)` con el
layout semántico y solicita el bloque al heap de proceso mediante `GetProcessHeap` y `HeapAlloc`.
`DeleteExpression`, incluida la forma `delete[]`, libera el puntero con `HeapFree`. El camino
existente de `StackallocExpression` y el GEP tipado de indexación completan el flujo heap/pila.

Las tres funciones Win32 se declaran con firmas LLVM-C explícitas. Al ser declaraciones no generan
dependencias si un programa no usa heap. El objeto de la fixture sí debe contener relocations para
las tres imports; el gate construye una import library mínima con NSLink y la usa en ambas rutas.

## Gate SH32

`arrays-memory/Program.ns` reserva cuatro `int` en heap y cuatro en pila, escribe e indexa ambos
bloques, suma 42 y libera el heap. Para cada perfil O0–O3 el gate:

1. construye `nsc-next` desde fuente;
2. compara el MIR canónico y exige `alloc.array`, `stack.alloc`, `index.address` y `free.array`;
3. inspecciona el objeto y exige `GetProcessHeap`, `HeapAlloc` y `HeapFree`;
4. enlaza y ejecuta con LLD y NSLink, con salida 42 en las ocho rutas.

## Límite honesto

SH32 cubre la memoria nativa necesaria por el bootstrap: punteros, heap Win32, `stackalloc`,
indexación y liberación explícita. La representación de array administrado con cabecera,
comprobación dinámica de límites y spans forma parte de las bibliotecas/runtime reconstruidos y se
certifica acumulativamente en SH48. No se declara implementada por esta fixture nativa.

- [x] `new T[n]` con tamaño semántico del elemento.
- [x] `delete[]` mediante el heap de proceso.
- [x] `stackalloc` e indexación tipada.
- [x] O0–O3 con ambos linkers.
- [ ] baseline acumulado final.
