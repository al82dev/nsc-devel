# Generadores async de namespace y salida con `yield break`

Estado: implementación y validación dirigida parciales; queda pendiente el caso
excepcional descrito abajo. La rama de trabajo es
`development-fixes-limitaciones`; los cambios async empezaron sobre
`71cd86d35220f654140e5619e9f9db0020f0b158` y siguen sin registrar. Luego se
integraron pruebas de punteros y su evidencia hasta `715534134`.

## Cambio y diagnóstico

Las funciones de namespace con un cuerpo que contiene `yield` se clasifican
para generar la máquina async correspondiente. La detección excluye cuerpos
anidados y declaraciones precompiladas; una fábrica que simplemente devuelve
otro iterador sigue siendo una función ordinaria. La construcción HIR separa
la envoltura, el avance, el poll y la liberación de forma equivalente a las
funciones miembro.

Para `yield break`, el poll async termina la máquina y publica el fin del
iterador. Durante la emisión, el camino de salida consumía la pila de regiones
de limpieza sin restaurarla. Una restauración posterior por tamaño podía crear
ordinales cero espurios; el caso con `owned Guard` y `finally` de namespace
fallaba con `cannot resolve MIR cleanup action`. La reproducción instrumentada
identificó la referencia `#0` en un poll cuyas acciones disponibles eran `#1`
y `#2`. Evidencia: `A:\nsc-fixes-evidencia\namespace-cleanup-diagnostic-test-20261112.log`.

La emisión de `yield break` ahora guarda y restaura las pilas de limpieza y
funclets para que las ramas hermanas mantengan su contexto léxico. Para un
generador async, también intercala la liberación de cada excepción capturada y
las acciones `finally` según su profundidad. En el caso anidado, el `finally`
interior observa una excepción destruida y el exterior observa dos; antes
ambos observaban dos.

Archivos de implementación: `src/Frontend/ProjectNamespaceFunctionModel.cpp`,
`src/Frontend/HIR/HirBuildCoordinator.cpp` y
`src/LLVM/Lowering/OopLlvmStatements.cpp`. Las reproducciones se conservan en
`tests/integration/async_namespace_generator.py`.

## Evidencia dirigida

El compilador aislado utilizado fue
`A:\nsc-fixes-evidencia\build\bin\nsc.exe`, SHA-256
`223655d766ecd7309b67b08593ed2619da19c348da4fce894d9b83a042680545`.
El directorio de ejecución fue `A:\nsc-devel-fixes` y los temporales se
ubicaron en `A:\nsc-fixes-evidencia\temp`. Cada compilación de proyecto usó
`--emit-project-oop-exe`, `--no-artifact-cache` y `-O0` o `-O2`; el log
registra cada comando completo. La prueba binaria genera una biblioteca con
`--emit-project-oop-lib` y compila un consumidor separado.

En `A:\nsc-fixes-evidencia\namespace-yield-break-extended-20261112.log`:

- 48 ejecuciones de casos fuente devolvieron el código esperado 42: 24 casos
  en O0 y O2. Incluyen cleanup de clase y namespace, catch anidado, orden de
  finally, las dos ramas de una salida condicional, parámetros, genéricos,
  fábricas y suspensión.
- El productor/consumidor binario devolvió 42 en O0 y O2.
- La prueba `class_yield_break_catch_cleanup_throws` falló al compilar en
  ambos niveles; por ello el ejecutor completo de esta prueba dirigida no pasó.

El log `A:\nsc-fixes-evidencia\namespace-yield-break-intercalado-20261112.log`
registra el conjunto anterior a las dos regresiones adicionales y confirma
`class_yield_break_nested_finally` en ambos niveles. La ejecución ampliada
confirma también `namespace_yield_break_nested_finally` y
`yield_break_conditional_cleanup` en O0 y O2.

La regresión de cancelación y abandono registró 18 ejecuciones con salida 42,
nueve casos en O0 y O2, según
`A:\nsc-fixes-evidencia\cancel-drop-after-yield-break-20261112.log`. Incluye
abandono directo, cancelación seguida de abandono, repetición de cancelación,
reanudación tras cancelar y abandono antes de iniciar o después de terminar.

## Control adicional: generador síncrono

La revisión del mismo cambio identificó que `yield break` en un `catch` de un
`Iterator<int>` síncrono retorna antes de la ruta que libera la excepción.
El fixture independiente
`A:\nsc-fixes-evidencia\sync-yield-break-catch-20261112\Main.ns` devolvió 2,
en lugar de 42, en O0 y O2. La variante `Detail.ns` registró cero destrucciones
antes y después de `Drop()` en ambos niveles; su resultado no distingue si
el `finally` se ejecutó observando cero o si no se ejecutó. Los comandos y
resultados están en `result.log` y `detail.log` de esa misma carpeta.

El intercalado de excepciones y cleanups de `yield break` se extendió a
generadores síncronos. Tras el build incremental, el compilador aislado tuvo
SHA-256 `20fed376f444e724aba2a364f31896b7dcc8753daf90bdff5846f89c41f64432`.
`tests/integration/sync_yield_break_cleanup.py` ejecutó tres casos en O0 y O2:
seis resultados 42, incluido el orden 1 y 2 de los finally anidados y el
control de destrucción normal. El log con comandos es
`A:\nsc-fixes-evidencia\sync-yield-break-after-20261112.log`.

Con ese mismo build se repitieron los controles async: 50 resultados 42 y dos
fallos de compilación correspondientes al caso pendiente de funclet en
`A:\nsc-fixes-evidencia\namespace-after-sync-break-20261112.log`. La regresión
de cancelación y abandono volvió a dar 18 resultados 42 en
`A:\nsc-fixes-evidencia\cancel-drop-after-sync-break-20261112.log`.

## Pendiente: excepción reemplazante en funclet

`class_yield_break_catch_cleanup_throws` hace `yield break` dentro de un
`catch` y lanza otra excepción desde el `finally` exterior. En O0 y O2, el
compilador reporta `semantic invariant forbids structured exit across an active
cleanup funclet`. La prueba no llegó a ejecutarse y no demuestra que la
excepción original y la reemplazante se destruyan correctamente. Su corrección
requiere resolver la emisión excepcional SEH correspondiente y volver a correr
la reproducción; no se declara resuelta por los resultados de los demás casos.

Estos resultados son dirigidos y no acreditan el cierre de toda la campaña
async, de cancelación anidada ni de la batería general.
