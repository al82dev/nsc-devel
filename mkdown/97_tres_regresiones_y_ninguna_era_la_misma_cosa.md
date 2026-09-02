# 97 — Tres «regresiones de NSLink», y ninguna era la misma cosa

**Fecha:** 2026-09-02
**Resultado:** `nsc` + NSLink en **244/244**, cero regresiones frente a lld-link.
**Lo que hay que llevarse de aquí:** de las tres pruebas que el buzón le entregó a NSLink como
suyas, **una lo era**. Las otras dos eran nuestras, y cada una falsa por un motivo distinto.

---

## 1. El ciclo corto

`nslink_solo.bat` — para cuando cambió `nslink.exe` y no el compilador.

```
0. build INCREMENTAL del arbol existente     -> _logs_nslink_solo\00_build.log
1. ctest ENTERO enlazando con nslink.exe     -> _logs_nslink_solo\02_ctest.log
2. los 84 trabajos                           -> _logs_nslink_solo\cases\
3. comparacion contra _logs_lldlink\         -> COMPARACION_CON_LLD.txt + buzon
```

`nslink_solo.bat ctest` corre sólo el paso 1, que es el 90 % de las veces lo que hace falta.

**Por qué ctest y no sólo los 84.** Los 84 trabajos pasan con los dos enlazadores y han pasado
siempre. Lo que ejercita la superficie del enlazador —construir DLLs, bibliotecas de importación,
enlazar anfitriones de C, C++ y .NET— está en ctest. Un ciclo corto que mirara sólo los 84 diría
«todo bien» siempre.

**Sin referencia no hay ciclo.** Si falta `_logs_lldlink\`, el script se para. «84 OK» sin nada
contra qué compararlo no dice si algo *empeoró*, que es lo único que se quiere saber aquí.

**El build incremental no es opcional.** Se añadió tras medir con binarios de pruebas viejos: se
cambiaron seis pruebas, se lanzó el ciclo, y ctest corrió los ejecutables anteriores. Un ciclo corto
que mide binarios que no corresponden a las fuentes no es rápido, es inútil.

---

## 2. Las tres, una por una

### 2.1 `nsc.stage46.native-libraries` — era de NSLink, y ya está arreglada

*«DLL generation creates the matching COFF import .lib»*, y con ella caían dieciocho *checks*
repartidos por `stage47`, `stage48_1`, `stage48_3` y `stage48_4`: uno producía el `.lib` y los otros
intentaban enlazar contra él. Un solo defecto en el camino `/dll /noentry /def: /implib:`.

Lo arregló quien mantiene NSLink. Es la única de las tres que era suya.

### 2.2 `nsc.stage48_4.cpp-host` — la prueba medía el texto de lld-link

```cpp
tests.expect(mangledLog.find("undefined symbol") != std::string::npos &&
             mangledLog.find("NS484_Add") != std::string::npos, ...);
```

El enlace **fallaba correctamente** —la comprobación anterior, que exige el fallo, pasaba—. Lo que
caía era la siguiente: buscaba la cadena literal `"undefined symbol"`, que es **cómo lo dice
lld-link**. NSLink redacta su diagnóstico con otras palabras.

Una prueba así da rojo con cualquier enlazador que no sea aquel contra el que se escribió, y llama
defecto suyo a una diferencia de vocabulario. Ahora exige lo que de verdad importa: que el
diagnóstico **nombre el símbolo** que no resolvió. El nombre en el log es la evidencia de que la
causa es el decorado de nombres; la redacción no es contrato.

### 2.3 `nsc.stage48_5.csharp-pinvoke-host` — una variable mía pisando una propiedad de MSBuild

El check que fallaba era que existiera `bin\Release\net9.0\Stage48_5CSharpHost.dll` tras un
`dotnet build`. El `dotnet build` decía `Build succeeded`. El `.dll` no estaba ahí:

```
Stage48_5CSharpHost -> ...\ManagedBuild\_logs_nslink_solo\Stage48_5CSharpHost.dll
```

`nslink_solo.bat` hacía `set "OUTDIR=_logs_nslink_solo"`. **MSBuild convierte toda variable de
entorno en una propiedad suya, y los nombres son insensibles a mayúsculas**: `OUTDIR` llega como
`OutDir`, que es exactamente la propiedad que fija el directorio de salida. Cada `dotnet build`
lanzado desde ese script escribía ahí.

Encaja con todo lo observado: `stage48_5` **sólo** falló en las corridas de `nslink_solo.bat`. En
`all.bat`, que no define esa variable, nunca falló.

Renombradas a `SOLO_OUT` y `SOLO_BUILD`. **Un script que exporta nombres genéricos al entorno de sus
hijos rompe herramientas que no conoce**, y el prefijo es lo que impide volver a hacerlo.

---

## 3. Tres cosas de método que costaron tiempo hoy

### 3.1 Cambiar dos variables a la vez no demuestra nada

`stage48_5` se dio por contención con esta reproducción:

```
A:\nslink-devel>ctest --test-dir A:\nsc-devel\build-clang-lldlink -R stage48_5 --output-on-failure
1/1 Test #66: nsc.stage48_5.csharp-pinvoke-host ...   Passed
```

Pasa sola. Pero en esa ventana **`NSC_LLD_LINK` no estaba puesta**, así que esa corrida enlazó con
lld-link. Cambiaron el paralelismo *y* el enlazador, y el resultado se atribuyó al primero.

La comparación controlada —misma prueba, sola, sólo cambia el enlazador— falla. La conclusión era
falsa aunque el diagnóstico final (no es de NSLink) acabara siendo correcto por otro motivo.

### 3.2 Una regresión que sólo se reproduce en paralelo es un flake hasta que se demuestre

`--compare` ahora **relanza en serie** las pruebas que fallaron con `-j`, y separa:

```
REGRESIONES DE CTEST -- ... Y FALLAN TAMBIEN AL CORRERLAS SOLAS:
SOLO FALLAN EN PARALELO -- NO son del enlazador:
```

Las de contención no cuentan para el código de salida. Queda `02_ctest_serie.log` al lado del otro,
así que la confirmación es auditable y no una afirmación.

**Y la parte que se puede hacer mal sin notarlo:** la reverificación fija `NSC_LLD_LINK` ella misma
en vez de heredarla. `all.bat` limpia esa variable antes de llamar a la comparación, así que heredar
el entorno habría relanzado las pruebas **con lld-link**: pasarían todas y **todo** quedaría marcado
como contención. El error de 3.1, en la dirección contraria y automatizado.

### 3.3 Una prueba que borra su evidencia al fallar obliga a reproducir a mano

Las seis pruebas de host hacían `remove_all(root)` en la última línea, **pasaran o no**. Al ir a leer
el `dotnet_build.log` de `stage48_5` la respuesta fue *«The system cannot find the path
specified»*: la prueba se lo había llevado por delante.

`TestContext::resultKeeping(root)` lo cierra en el sitio común: en verde limpia, en rojo conserva y
**dice dónde**. Fue lo que permitió leer el log y encontrar lo de `OutDir` en un paso.

---

## 4. Qué se tocó

| Archivo | Qué |
|---|---|
| `nslink_solo.bat` | **Nuevo.** El ciclo corto. Build incremental, ctest, los 84, la comparación |
| `nslink_pipeline.py` | `--compare` funciona con sólo ctest; reverificación en serie; `NSLINK_OUT_DIR`; `LOGS_SKIP` con los directorios de temporales |
| `tests/support/TestContext.h` | `resultKeeping()` — no borrar la evidencia cuando la prueba falla |
| `tests/unit/Stage46…`, `47`, `48_1`, `48_3`, `48_4`, `48_5` | Usan `resultKeeping(root)` |
| `tests/unit/Stage48_4CppHostIntegrationTests.cpp` | El check negativo exige el símbolo, no la redacción de lld-link |
| `verify_manifest.py`, `.gitignore` | `_logs_nslink_solo` |

---

## 5. Estado

```
lld-link : 244/244        NSLink : 244/244        84 trabajos : 84 OK con los dos
```

Nada pendiente que bloquee el Bloque 4.
