# Tanda correctiva 02: inferencia de edicion en grafos fuente

## Problema

Los proyectos sin `<Language>` conservaban el valor predeterminado Language 2,
pero perdian el hecho de que ese valor era implicito. Al importar un proyecto
Language 3, el cargador trataba la omision como una declaracion incompatible y
emitia `NS3001` antes de analizar el programa.

## Cambio

- `Project` conserva si la edicion fue declarada en el manifiesto.
- Un grafo fuente con una sola edicion explicita la propaga a sus nodos
  implicitos antes de validar ciclos y aristas.
- Un grafo totalmente implicito conserva Language 2.
- Dos declaraciones explicitas incompatibles siguen rechazadas con `NS3001`.
- Las reconstrucciones internas de `Project` conservan tambien paquete, modulos
  amigos, politica de panico y procedencia de la edicion.
- Se agrego una prueba nueva; ninguna prueba o fixture existente fue modificado.

## Archivos

- `include/nativesharp/Compilation/Project.h`
- `src/Compilation/Project.cpp`
- `src/Compilation/ProjectLoader.cpp`
- `src/Compilation/ProjectGraphLoader.cpp`
- `src/Compilation/ProjectGraph.cpp`
- `tests/unit/LanguageEditionInferenceTests.cpp`
- `CMakeLists.txt`

## Mejora

Los proyectos historicos sin etiqueta pueden consumir una dependencia fuente
actual sin reescribirse. La frontera de seguridad permanece para quienes si
declararon ediciones contradictorias.
