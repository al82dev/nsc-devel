# Tanda de validación 244 — restauración de compatibilidad LLVM

Fecha: 2026-10-10

## Estado

IMPLEMENTADA. VALIDACIÓN DE CONFIGURACIÓN PENDIENTE.

## SCOPE LOCK

Esta tanda corrige únicamente `INFRA|1` de la corrida
`20261010-205025-25b2ecce`. `INFRA|2` permanece registrado como bloqueo
derivado de `Configurar NSC Release`.

## Evidencia

Builder 3 pasó explícitamente el paquete persistido por el usuario:

`-DLLVM_DIR=A:/apps/msys64/mingw64/lib/cmake/llvm`

CMake encontró LLVM `22.1.8`, pero una restricción añadida posteriormente
aceptaba exclusivamente la línea `23.x`. El mismo bloque documenta que el
requisito técnico real es `LLVMRunPassesOnFunction`, disponible desde LLVM 20,
y el header instalado de LLVM 22 declara esa función.

El historial de `CMakeLists.txt` confirma que el comportamiento compatible
anterior aceptaba LLVM 20 o posterior. La restricción `[23,24)` fue una
limitación adicional, no una necesidad de la API usada por el compilador.

## Corrección causal

Se restaura exclusivamente la compuerta anterior: rechazar versiones menores
que LLVM 20 y aceptar las posteriores. El diagnóstico vuelve a expresar el
floor de API que motiva la validación.

No se modifican rutas, autodetección, toolchain, Builder 3, selección del
paquete ni código del compilador.

## Validación

No se configuró, compiló ni ejecutó por orden del usuario. La siguiente
corrida debe confirmar `INFRA|1`; sólo entonces puede reintentarse el paso
`INFRA|2` que dependía de esa configuración.
