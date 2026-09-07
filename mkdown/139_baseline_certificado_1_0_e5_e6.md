# 139 — Baseline certificado de 1.0-E5 y 1.0-E6

**Fecha:** 2026-09-07  
**Gate:** G00  
**Run:** `20260907-131523-7eda857d`  
**Estado:** `LANDED`  
**Certificado:** sí  
**Fallos:** 0

## Resultado

E5 y E6 dejaron de ser únicamente un cierre técnico del árbol de desarrollo. El compilador que las
contiene fue reconstruido, instalado y sometido al gate completo sin que las fuentes cambiaran
durante la corrida.

Este baseline certifica dos capacidades conectadas:

- E5 explica cada incompatibilidad ABI con el componente, valor esperado y valor encontrado, y
  rechaza modelos NSMETA estructuralmente incoherentes antes de materializar un consumidor;
- E6 verifica que `.nsmeta`, archivo COFF y DLL opcional pertenecen físicamente al mismo producto,
  comparando la superficie publicada con los símbolos y exports reales.

No se certificó una copia anterior del compilador ni un árbol distinto al observado. La identidad
del ejecutable construido coincide byte por byte con la del ejecutable instalado.

## Evidencia exacta

| Dato | Valor |
|---|---|
| Directorio de corrida | `A:\nsc-devel\_nscbuilder\history\20260907-131523-7eda857d` |
| Hora registrada | `2026-09-07T13:25:55-06:00` |
| Estado | `LANDED` |
| Certificado | `true` |
| Fallos | 0 |
| Etapas | 11 |
| Casos | 324 |
| Archivos bajo huella | 1013 |
| Huella S0 | `7917ff88ca8774c624f1c7c7880de95b07e6062ec8a9b56b1f971e133dfd8b9b` |
| Huella S1 | `7917ff88ca8774c624f1c7c7880de95b07e6062ec8a9b56b1f971e133dfd8b9b` |
| SHA-256 construido | `58c44dd8a966d526d4f83fa9e97bdcd223e3e992af0d61cce84e2018d3ba7d5c` |
| SHA-256 instalado | `58c44dd8a966d526d4f83fa9e97bdcd223e3e992af0d61cce84e2018d3ba7d5c` |

La identidad del producto instalado es:

```text
nsc 0.9.0+build.20260907T191524Z.400a252ae1c4b36a
NativeSharp 0.1, stage 57
SHA-256 58C44DD8A966D526D4F83FA9E97BDCD223E3E992AF0D61CCE84E2018D3BA7D5C
```

## Cómo leer las dos igualdades

`S0 == S1` demuestra que los 1013 archivos incluidos en la huella no cambiaron entre la captura
inicial y la final. Evita llamar baseline a una corrida que empezó con una revisión y terminó con
otra.

`built_nsc == installed_nsc` demuestra que el ejecutable probado como producto instalado contiene
exactamente los mismos bytes que el producido por la reconstrucción de esta corrida. Evita
certificar accidentalmente una instalación anterior.

Ambas igualdades son necesarias. La primera fija el código fuente; la segunda fija el producto que
el usuario invoca fuera del directorio de construcción.

## Recorrido de las 11 etapas

El gate recorrió, en orden, la producción del compilador, su instalación, las dos construcciones de
pruebas, la sincronización de bibliotecas y las tres familias de validación para cada enlazador:

1. construir el compilador;
2. instalar el producto;
3. construir CTest con lld-link;
4. construir CTest con NSLink;
5. sincronizar las bibliotecas instaladas;
6. ejecutar CTest con lld-link;
7. ejecutar fixtures con lld-link;
8. ejecutar bug triage con lld-link;
9. ejecutar CTest con NSLink;
10. ejecutar fixtures con NSLink;
11. ejecutar bug triage con NSLink.

Los 324 casos agregados por el acta terminaron sin fallos. Así, E5 y E6 no dependen de una sola ruta
de enlace ni de una validación aislada de metadata.

## De la evidencia focal al producto instalado

Durante la implementación, los documentos 137 y 138 registraron este binario focal:

```text
nsc 0.9.0+build.20260907T182919Z.ceeacff631a7c23c
SHA-256 F3E13714F564D35EE494E71FB8CAE5732EC6459902481E492C4E0E0C7723E748
```

Esa identidad sigue siendo evidencia válida del cierre local y de sus suites focales. No tenía que
coincidir con el binario instalado posterior: la reconstrucción genera una identidad nueva. La
condición de certificación es que el binario construido por G00 coincida con el instalado por esa
misma corrida, condición que se cumple con el SHA-256 `58c44d...d5c`.

La secuencia histórica queda entonces:

1. E1–E4 fueron instaladas y certificadas por `20260907-112232-d023cfe0`;
2. E5–E6 se implementaron y cerraron primero con validación focal;
3. `20260907-131523-7eda857d` reconstruyó el árbol posterior y certificó E5–E6;
4. el baseline anterior conserva su significado: no se le atribuyen cambios que aún no contenía.

## Archivos y funciones que este baseline promueve

La implementación ya explicada en 137 y 138 queda ahora respaldada como producto instalado:

- `src/Interop/LibraryMetadata.cpp`: `versionPair`, `verifyFrozenAbiContract`, verificación del
  contrato físico y coherencia estructural;
- `src/Driver/Driver.cpp`: `printMetadataAbiContract`, `metadataRequiredSymbols`, `missingSymbols`,
  `describeSymbols` y `Driver::verifyLibraryArtifacts`;
- `include/nativesharp/Driver/Driver.h`: superficie pública del verificador de artefactos;
- `tests/unit/Stage48_2LibraryMetadataTests.cpp`: contrato, estructura, archive y DLL;
- `tests/unit/Stage50_9MetadataHostCompatibilityTests.cpp`: compatibilidad histórica y contrato
  estructural actual.

## Estado de cierre

E5 y E6 están implementadas, reconstruidas, instaladas y certificadas. Ya no existe una tarea de
recertificación pendiente para estas dos entregas. El trabajo siguiente puede partir de este G00
como baseline sin reabrir sus contratos salvo que aparezca un repro nuevo y autocontenido.

