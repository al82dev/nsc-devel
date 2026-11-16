# Tanda correctiva 02: inferencia de edicion en grafos fuente

> **Vigencia:** NativeSharp es un lenguaje único y acumulativo; el producto es 1.0. Las referencias históricas a ediciones o versiones de lenguaje y NSMeta que aparezcan a continuación no definen compatibilidad ni deben reintroducirse.

## Problema

En la implementación histórica, los proyectos sin `<Language>` recibían un valor
predeterminado y perdían el hecho de que era implícito. Al importar un proyecto
con otra etiqueta, el cargador trataba la omisión como incompatible y emitía
`NS3001` antes de analizar el programa.

## Cambio

La corrección de entonces conservó la procedencia de la etiqueta e infirió una
etiqueta común en el grafo; mantenía el rechazo de etiquetas explícitas distintas.
También preservó paquete, módulos amigos y política de pánico. Se agregó una
prueba, sin modificar las existentes.

Ese mecanismo fue retirado: hoy hay un solo lenguaje y las etiquetas históricas
no seleccionan reglas ni causan rechazo. La validez del grafo depende de sus
referencias y contratos reales.

## Archivos

- `include/nativesharp/Compilation/Project.h`
- `src/Compilation/Project.cpp`
- `src/Compilation/ProjectLoader.cpp`
- `src/Compilation/ProjectGraphLoader.cpp`
- `src/Compilation/ProjectGraph.cpp`
- `tests/unit/LegacyLanguageLabelTests.cpp`
- `CMakeLists.txt`

## Mejora

La campaña eliminó un rechazo prematuro. El contrato vigente elimina por completo
la selección por etiqueta y conserva las comprobaciones reales de las dependencias.
