# Tanda de Validación 25: grafo CMake y registros C++

Fecha: 10 de octubre de 2026
Estado: CERRADA CON EVIDENCIA ESTÁTICA

## Fronteras auditadas

La suite C++ exige mantener sincronizadas cuatro superficies manuales:

1. archivos `*Module.cpp`;
2. fuentes de `nsc_component_tests` en CMake;
3. declaraciones de registro en `TestRegistry.h`;
4. llamadas de registro en `main.cpp`.

Además, cada controlador `run_*.py` debe estar conectado a CTest y cada prueba
CTest debe conservar una identidad única.

## Prueba permanente

`test_cpp_module_graph.py` deriva los conjuntos desde el árbol actual y exige:

- exactamente una definición `register*Module` por archivo de módulo;
- identidad biyectiva entre definiciones, declaraciones y llamadas;
- identidad entre módulos del filesystem y módulos del target;
- existencia y unicidad de todas las fuentes C++ del target;
- referencia exacta una vez de cada controlador `run_*.py`;
- nombres CTest no repetidos.

La prueba no contiene listas de módulos, scripts ni cantidades esperadas.

## Evidencia de esta revisión

- Archivos de módulo: `307`.
- Definiciones, declaraciones y llamadas de registro: `307/307/307`.
- Fuentes C++ del target: `535`, existentes y únicas.
- Controladores: `115`; referencias CTest: `115`.
- Nombres CTest: `117`, únicos.
- Prueba dirigida del grafo: `2/2` PASS.

No se modificaron CMake, módulos, registros, casos, oráculos, producto ni
batería histórica. No se configuró, compiló ni ejecutó producto; no hubo
baseline ni despliegue.
