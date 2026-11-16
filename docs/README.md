# Documentación de NativeSharp

NativeSharp es un único lenguaje de sintaxis acumulativa: los proyectos no seleccionan una
versión del lenguaje. La versión del producto es **1.0**; la identidad de cada compilación se
consulta con `nsc --version`.

- [Especificación del lenguaje](../spec/NativeSharp_Language_Specification_1_0.md)
- [Contrato del compilador](../spec/NativeSharp_Compiler_Contract_1_0.md)

Los demás directorios conservan expedientes y nombres históricos de desarrollo. Sus etiquetas
anteriores no definen ediciones del lenguaje ni requisitos para compilar. Los contratos y la
implementación actuales deben cotejarse con el código correspondiente.

Las herramientas de catálogo y seguimiento usan por defecto
`A:/DOCUMENTO-MAESTRO-NATIVESHARP.md` y `A:/NATIVESHARP-TRACKING.md`. Esas rutas son
configurables con `--master` y `--tracking` o `--output`, según la herramienta. El cambio de
nombre no crea ni traslada documentos: para consultar un archivo histórico existente se debe
proporcionar su ruta explícita. El inventario conserva los nombres históricos como procedencia,
no como versiones del lenguaje.

---

# NativeSharp documentation

NativeSharp is one cumulative language; projects do not select a language version. The product
version is **1.0**. Use `nsc --version` to identify a compiler build.

- [Language specification](../spec/NativeSharp_Language_Specification_1_0.md)
- [Compiler contract](../spec/NativeSharp_Compiler_Contract_1_0.md)

Other directories preserve historical development records and names. Their earlier labels do not
define language editions or compilation requirements. Check current contracts against the relevant
implementation.

Catalogue and tracking tools default to `A:/DOCUMENTO-MAESTRO-NATIVESHARP.md` and
`A:/NATIVESHARP-TRACKING.md`. Depending on the tool, `--master`, `--tracking`, or `--output`
can select other paths. Renaming the defaults does not create or move documents: pass an
existing historical file explicitly when needed. The inventory retains historical names as
provenance, not as language versions.
