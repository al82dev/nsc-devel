# Fábrica con fallo y cuatro campos compartidos

## Reproducción

`A:/nsc-vistas-evidencia/shared-four-fields-failure/Main.ns` conserva un
payload `Failed` cuyo constructor inicializa, en orden, campos reales
`Rc<Payload>`, `WeakRc<Payload>`, `Arc<Payload>` y `WeakArc<Payload>`, y luego
lanza. El consumidor lo crea con `SharedPointers.MakeRcThrowing<Failed>()` y
exige capturar la excepción, destruir exactamente dos payloads y no llamar el
destructor de `Failed`, cuyo constructor no terminó. SHA-256 de la fuente:
`776a1d9f12aeea844dc213d2011f55219e3cba4e22759a671aac69cbc3ec78ac`.
El proyecto consume el Core binario de vistas en
`A:/nsc-vistas-evidencia/sdk/NativeSharpCore.lib` con su metadata.

Con `vistas-env.cmd`, `nsc.exe --check-project-method-bodies` devolvió 0
(`shared-four-fields-check.log`); `--emit-project-oop-exe` con
`--no-artifact-cache` falló en O0 y O2 con
`shared throwing construction cannot safely clean this payload shape`.
Los logs son `shared-four-fields-O0.log` y `shared-four-fields-O2.log` en
`A:/nsc-vistas-evidencia/`. O0 reporta build hash `41a895fb15b8e4c3`; O2
`42cd45eca877e2a3`: hubo un build concurrente entre las dos ejecuciones, por
lo que no se trata de una comparación del mismo ejecutable. No se ejecutaron
binarios para la fábrica rechazada.

Un proyecto separado, `shared-four-fields-direct`, cambia únicamente la
construcción exterior a `owned Failed value = new Failed()`. Compiló con build
hash `41a895fb15b8e4c3` y devolvió 42 en O0 y O2. Logs de compilación:
`shared-four-fields-direct-O0.log` y `shared-four-fields-direct-O2.log`.
Esto verifica el resultado observable del ejemplo directo, no el de la fábrica
ni la liberación física del bloque compartido de la fábrica rechazada.

## Causa probable y frontera

En `src/LLVM/Lowering/OopLlvmConstruction.cpp`, la construcción compartida con
constructor que lanza exige `safePartialCleanup`. Las líneas 1306–1314
rechazan un campo que requiere destrucción de valor cuando no es un puntero
`owned`, y las líneas 1343–1344 emiten el diagnóstico. Los campos de las
familias tienen destructores de valor; esta lectura identifica una causa
probable que necesita instrumentación o una prueba adicional para atribuir el
campo concreto. La barrera es deliberadamente conservadora y no se retiró.

El backend es responsabilidad del integrador. Una posible corrección mínima
debe poder destruir solo campos efectivamente inicializados, en orden inverso,
mantener la excepción y liberar una sola vez el bloque de la fábrica. Antes de
aceptarla se necesitan controles por cada posición de fallo, campos vacíos,
movidos y reemplazados, destructor que lanza, ambas fábricas, clases y
estructuras, O0/O2 y Core binario; las expectativas deben respetar las
restricciones de `Send` y `Sync`. No basta desactivar la condición actual.
El caso permanece bloqueado por esa frontera y no se considera resuelto.
