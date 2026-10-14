# NS304-T08-F07 — NSMETA y cero layout mutation

- **Estado**: AUDITADO (cero layout ✅; NSMETA de extension instancia ✅ / static+operator deuda)
- **Rama**: development

## Auditoría (línea base)

Contrato (tracking): *Implementar NSMETA de extension y cero layout mutation*.
Plan T08-F07 (DeepSeekDocs): *versionar properties/static/operators/conformance con receiver, passing
mode, constraints y witness; producer y consumer separados, sin fuentes; comparar layout antes/después
y demostrar igualdad; inspeccionar HIR/MIR/LLVM para cero field injection, boxing o sidecar oculto*.

Estado real:

- **Cero layout mutation**: la extensión es un namespace function (F01/F02/F03); no agrega campos ni
  desplaza el layout del tipo extendido (no hay field injection ni sidecar).

## Auditoría NSMETA (2026-10-14, post-T09-F05)

Hallazgos reales del plumbing de metadata (no los supuestos de la auditoría inicial):

1. **Extensión de instancia (F01/F02)**: el productor publica cada función `ns3-extension` como
   `LibraryMetadataNamespaceFunction` con el flag `kMetadataNamespaceFunctionExtension` (NSMETA 1.32,
   `LibraryMetadata.cpp:1475`); el consumidor la reconstruye como bloque `extension` con el receptor
   implícito (`Driver.cpp:2627-2663`). **Publicado + consumible**.
2. **Extensión estática (F03 `T.Miembro()`) y operador (F04 `operator+`)**: `isExtensionDeclaration`
   acepta `ns3-extension-static` (`ProjectNamespaceFunctionModel.cpp:220`), así que también marcan
   `kMetadataNamespaceFunctionExtension`. **Deuda**: no hay bit "static" en
   `LibraryMetadataNamespaceFunction::flags`; el consumidor las reconstruye como extensión de
   instancia (trata el primer parámetro como receptor y lo descarta), lo que rompe el operador
   `op_Addition(a, b)` intermodular (pierde `a`).
3. **Tipos opacos (T05 future / T06 iterator / T07 async-iterator)**: `kMetadataTypeOpaqueResult`
   (NSMETA 1.30) los publica como struct de un campo. **Publicado**.
4. **`operatorToken` (T08-F04)**: no se serializa (el nombre `op_Addition` es la identidad del
   operador y es derivable por `operatorTraitContractForMethod`). No es un gap funcional, solo que el
   token no viaja en NSMETA.
5. **Collection builder (T09-F05)**: `CreateBuilder`/`Add`/`ToCollection` son miembros ordinarios y se
   publican como tales; el protocolo es una convención de source, no un concepto de metadata.

## Gap (deuda concreta)

- **Static extension + operador intermodular**: falta un marcador "static" en el descriptor de la
  función de extensión, o que `isExtension` no se marque para `ns3-extension-static`, para que el
  consumidor reconstruya el operador/estático sin descartar el primer parámetro.
- El resto (receiver, passing mode, constraints de instancia) ya viaja en los records de parámetro.

## Nota

F07 es el cola de NSMETA de T08 (compartido con T05-F09/T06-F08/T07-F08). La prueba de layout inmutable
y el consumo intermodular de la extensión estática quedan para cerrar esta deuda.
