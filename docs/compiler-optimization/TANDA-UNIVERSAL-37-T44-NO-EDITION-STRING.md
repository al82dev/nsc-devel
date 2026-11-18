# Tanda universal 37: cierre T44 sin edición ni texto frágil

> **Vigencia:** NativeSharp es un lenguaje único y acumulativo; el producto es 1.0. Las referencias históricas a ediciones o versiones de lenguaje y NSMeta que aparezcan a continuación no definen compatibilidad ni deben reintroducirse.

## Problema confirmado

T44 compilaba correctamente el consumidor y generaba el ejecutable, pero el verificador exigía una
etiqueta histórica de lenguaje en stdout. Al retirar la etiqueta del mensaje, una compilación con
código 0 y artefacto válido era reportada como fallo.

El mismo verificador fabricaba tres proyectos con la propiedad histórica `<Language>`.

## Cambio

- Se eliminan las etiquetas de edición de los proyectos temporales.
- El éxito del consumidor se prueba mediante código de salida 0 y existencia del ejecutable.
- Se conserva la ejecución real del artefacto y el rechazo causal del fingerprint incompatible.

No se acepta una compilación fallida y no se depende del formato de un mensaje informativo.

## Archivos

- Modificado: `cmake/VerifyNativeSharpT44MetadataClosure.cmake`.
- Nuevo: este documento.

## Validación requerida

1. Ejecutar `nsc.ns3.t44.metadata-closure` de forma dirigida.
2. Incluirlo en la única batería integral al cerrar el inventario.

