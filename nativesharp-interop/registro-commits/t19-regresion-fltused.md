# Tanda 19: `_fltused` del runtime (T19-037)

La [línea base](t19-linea-base-nsc.md) encontró que `nsc.windows-runtime` pasa en `152654e5a` y falla ahora. Se corrige
en el compilador; la prueba no se modificó.

**Defecto.** `runtime defines the MSVC floating-point marker symbol`: la prueba busca `@_fltused = global i32 0`
en el módulo del runtime.

**Causa.** `c1cbdc5a2` (2026-11-06) emitía `_fltused` siempre como `weak_odr`, para que un enlace con el CRT
estático (`libcmt.lib` también lo define) no dé un símbolo duplicado con LLD
([registro](runtime-fltused-weak.md)).

**Intento descartado.** Una definición externa en un COMDAT `any` deja el texto `global` que pide la prueba,
pero LLD la rechaza junto a la de `libcmt.lib`:

- error: `duplicate symbol: _fltused`;
- falló en `pruebas-tanda-15-recursos` y `pruebas-tanda-18-stl` (`logs/regresion-rg1`).

**Cambio.** `declareRuntimeExceptionState` recibe `overridableFltused`, y `buildInternal` lo activa en dos
casos: la biblioteca de soporte (`buildSupport`, que comparten los enlaces con y sin CRT y las DLL) y un
runtime construido para `--crt-startup`. Esos dos conservan `weak_odr`, como desde `c1cbdc5a2`. El runtime
completo sin CRT (`build`) vuelve a la definición `global` de la base.

## Evidencia

- `ctest -R "windows-runtime|item401"`: las dos pasan (`logs/ctest-rg4.log`).
- `lista-rg1.txt` (tandas 15, 16, 18, 19 y `09-llamadas`, con el CRT estático y DLL): 21 de 21 en estática
  (`logs/regresion-rg4`) y 17 de 17 en DLL (`logs/regresion-dll-rg4d`).

No se modificó Clang ni NSLink. El contrato NSMeta vigente es el único.
