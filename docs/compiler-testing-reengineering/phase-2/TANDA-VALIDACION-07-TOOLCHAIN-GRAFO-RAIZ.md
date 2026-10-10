# Validación 07 — Toolchain explícita del grafo raíz

## Propósito

Impedir que los seis casos `C12B-BLD-076` configuren el repositorio principal
con Visual Studio, un generador predeterminado o herramientas distintas de las
seleccionadas por Builder 3 y por la suite.

## SCOPE LOCK

- Repositorio: `A:\nsc-devel`.
- Contrato: `C12B-BLD-076`.
- Casos: `T12B-00451..T12B-00456`.
- Símbolos: `validate_root_config_arguments`, `run_case`,
  `ExecutionEngine._run_buildsystem` y la inscripción CTest de BuildSystem.
- Comportamiento autorizado: hacer explícito el contrato productor del CMake
  hijo antes de ejecutar los oráculos existentes.
- Protegidos: casos, oráculos, producto, suite histórica, demás controladores,
  seis archivos preexistentes, `C:`, despliegues y remotos.
- Validación funcional: pendiente de la siguiente corrida del usuario.

## Cadena causal

1. `run_root_graph` crea árboles nuevos del repositorio principal.
2. Un árbol nuevo sin `-G Ninja` permite que CMake seleccione otro generador.
3. La ruta CTest reenviaba sólo LLVM, NSLink y los compiladores C/C++.
4. La ruta directa de Builder 3 reenviaba únicamente LLVM.
5. Ninguna ruta fijaba conjuntamente generador, Ninja, modo, linker, manifest
   tool ni compilador de recursos.
6. El resultado podía depender de Visual Studio o de herramientas residuales,
   en lugar de medir el contrato NativeSharp seleccionado.

## Corrección

Las dos rutas proporcionan ahora el mismo conjunto mínimo:

- `-G Ninja`;
- `CMAKE_MAKE_PROGRAM`;
- `CMAKE_BUILD_TYPE`;
- `CMAKE_C_COMPILER`;
- `CMAKE_CXX_COMPILER`;
- `CMAKE_LINKER`;
- `CMAKE_MT`;
- `CMAKE_RC_COMPILER`;
- `LLVM_DIR`;
- `NSC_NSLINK_EXECUTABLE`.

El controlador valida el generador y los nueve argumentos con valor antes de
configurar. No busca ni reemplaza herramientas; sólo rechaza contratos
incompletos.

## Evidencia estática

- Casos del contrato: `6/6` inventariados.
- Rutas de ejecución: `2/2` corregidas.
- Generador permitido: `Ninja`.
- Argumentos obligatorios con valor: `9/9`.
- Dependencia implícita de Visual Studio: eliminada del contrato.
- Casos u oráculos modificados: `0`.
- Recursión añadida: `0`.
- Complejidad de validación: `O(a * p)`, con `a` y `p` acotados por diez
  argumentos de configuración.

## Estado

`CIERRE ESTÁTICO COMPLETO; VALIDACIÓN DE CONFIGURACIÓN PENDIENTE DEL USUARIO`.
