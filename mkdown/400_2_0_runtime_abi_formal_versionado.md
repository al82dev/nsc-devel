# Item 400 — Runtime ABI formal y versionado

## Resultado

- `RuntimeAbiContract.h` es la autoridad única de Runtime ABI `1.0` y symbol set `2`.
- El contrato congela 97 símbolos NativeSharp con clase de símbolo y tipo LLVM exacto; incorpora las variantes nombradas de mutex, semaphore y event que el inventario anterior omitía.
- El constructor Win32 verifica cada módulo en ambas direcciones antes de devolverlo: rechaza símbolos faltantes, firmas distintas, clases función/dato equivocadas y exports NativeSharp no contratados.
- El manifiesto determinista `NSRUNTIMEABI 1.0` fija target, modelo de punteros, calling convention, inventario ordenado y fingerprint `1605720e027fb252`.
- `nsc` imprime, escribe y verifica el manifiesto, y la instalación lo entrega en `share/nativesharp/runtime/NativeSharpRuntime.abi` para la biblioteca independiente del item 401.

## Validación

- Construcción limpia completa e instalación del compilador.
- Prueba dedicada `nsc.item400.runtime-abi-contract`.
- Regresiones de runtime Win32, contrato del toolchain y baseline de migración.
- Verificación directa del artefacto instalado con el compilador recién construido.
