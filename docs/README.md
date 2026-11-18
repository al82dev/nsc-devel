# Documentación de NativeSharp

NativeSharp es un único lenguaje de sintaxis acumulativa: los proyectos no seleccionan una
versión del lenguaje. La versión del producto es **1.0**; la identidad de cada compilación se
consulta con `nsc --version`.

- [Contrato vigente de trabajo y consulta](CONTRATO-VIGENTE.md)
- [Contrato vigente de NSMETA](NSMETA-CONTRATO-ACTUAL.md)
- [Especificación del lenguaje](../spec/NativeSharp_Language_Specification_1_0.md)
- [Contrato del compilador](../spec/NativeSharp_Compiler_Contract_1_0.md)

Los demás directorios conservan expedientes y nombres históricos de desarrollo. Sus etiquetas
anteriores no definen ediciones del lenguaje ni requisitos para compilar. Los contratos y la
implementación actuales deben cotejarse con el código correspondiente.

## Archivo de etapas anteriores

- [Etapa 2.0](2.0/README.md) — documentación histórica; no selecciona otro compilador.
- [Etapa 1.0.1](1.0.1/README.md) — documentación histórica; no ordena una migración actual.

Las carpetas `3.0` y `3.0.4` también contienen expedientes de sus etapas. Sus instrucciones antiguas
no autorizan cambiar la instalación ni definen contratos vigentes.

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

- [Current working contract](CONTRATO-VIGENTE.md)
- [Current NSMETA contract](NSMETA-CONTRATO-ACTUAL.md)
- [Language specification](../spec/NativeSharp_Language_Specification_1_0.md)
- [Compiler contract](../spec/NativeSharp_Compiler_Contract_1_0.md)

Other directories preserve historical development records and names. Their earlier labels do not
define language editions or compilation requirements. Check current contracts against the relevant
implementation. Version-named directories such as [2.0](2.0/README.md) and [1.0.1](1.0.1/README.md)
preserve engineering history; they do not select alternative compilers, dialects, or ABIs and do not
authorize migration or reinstallation.

Catalogue and tracking tools default to `A:/DOCUMENTO-MAESTRO-NATIVESHARP.md` and
`A:/NATIVESHARP-TRACKING.md`. Depending on the tool, `--master`, `--tracking`, or `--output`
can select other paths. Renaming the defaults does not create or move documents: pass an
existing historical file explicitly when needed. The inventory retains historical names as
provenance, not as language versions.
