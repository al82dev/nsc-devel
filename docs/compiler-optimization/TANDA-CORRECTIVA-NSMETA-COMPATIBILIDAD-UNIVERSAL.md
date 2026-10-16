# Tanda correctiva: restauración de compatibilidad universal NSMETA

## SCOPE LOCK

- Objetivo exacto autorizado: corregir la regresión que hace que el compilador actual rechace metadata histórica
  cuyos contenidos, lenguaje y ABI sí puede interpretar, y restablecer la política ya aprobada de compatibilidad
  basada en contenido y capacidades reales, nunca en etiquetas de versión o identidad del productor.
- Naturaleza: corrección funcional de compatibilidad; no es una feature, migración ni refactor.
- Comportamiento que debe permanecer idéntico: checksum del payload, autenticación de la proyección ABI real,
  validación estructural, target Windows x64, layouts, firmas, ownership, lifetime, convenciones de llamada,
  contratos ABI explícitos y rechazo de características o estructuras realmente no representables.
- Revisión conocida como correcta: `2c4c2c4e9`, que preservó la proyección ABI histórica neutral. Las decisiones
  normativas relacionadas están en `fd25ae369`, `0bea623fb`, `848dff141` y `2c4c2c4e9`.
- Regresión confirmada: `cc31cdf7a` añadió la sección NSMETA 1.40 `traitImpls` al payload sin condicionar su
  escritura al formato que la introdujo. Como `encodeAbiProjection` proyecta la base a 1.24, el lector actual
  agrega cuatro bytes inexistentes al autenticar archivos anteriores y emite `ABI fingerprint mismatch`.
- Rango evaluado: `2c4c2c4e9..a96572eaf`, con atribución causal específica a `cc31cdf7a`.
- Repositorio autorizado: únicamente `A:\nsc-devel`, rama `development`.
- Componente autorizado: codec y verificación de `LibraryMetadata`.
- Archivos de implementación autorizados: `src/Interop/LibraryMetadata.cpp`.
- Archivos de prueba autorizados: `tests/unit/Stage48_2LibraryMetadataTests.cpp`,
  `tests/unit/LibraryMetadataVersionToleranceTests.cpp` y `tests/unit/Item407OfficialPromotionTests.cpp`.
- Evidencia histórica protegida: `tests/fixtures/nsmeta/*.nsmeta`; no se reescribe ni regenera ningún golden.
- Este documento está autorizado para registrar alcance, causa, diferencial y validación.
- Símbolos autorizados: `encodePayload`, `encodeAbiProjection`, `verifyCurrentCompiler` y las expectativas focales
  que todavía convierten `producerVersion` en una frontera funcional.
- Símbolos protegidos: parser, frontend, HIR, MIR, LLVM, runtime, NSLink, NSDBG, bindings, bibliotecas, manifests,
  ABI constants y toda ruta no demostrada por la reproducción.
- Pruebas focales sin modificación de propósito: `nsc.library-metadata-version-tolerance` y
  `nsc.item407.official-promotion`. `nsc.stage48_2.library-metadata` conserva sus expectativas históricas corregidas,
  pero no se usa como gate de cierre porque el ejecutable de la configuración disponible presenta fallos ajenos
  previos a esta corrección. La segunda prueba focal se limita a retirar una expectativa residual que exigía rechazar
  `producerVersion=1.0.1`; las reglas de promoción permanecen protegidas.
- Configuración autorizada: build dirigido existente y ejecución focal O0 cuando aplique.
- Acciones prohibidas: baseline, batería completa, O1/O2/O3, deploy, instalación, recompilar bindings, reescribir
  goldens, cambiar ABI, agregar sintaxis, reintroducir ediciones o versionado NSMETA como feature independiente,
  usar MSYS/MSYS2/MinGW/Cygwin, modificar `PATH`, usar herramientas fuera del `PATH`, escribir fuera de `A:`,
  push, pull o fetch.
- Criterio exacto de terminación: los bytes NSMETA históricos archivados vuelven a autenticarse; el formato 1.40
  conserva su sección de conformidades; alterar realmente la proyección ABI sigue fallando; una etiqueta de formato
  o productor por sí sola no rechaza contenido compatible; las dos pruebas focales ejecutables pasan y el diferencial
  no contiene trabajo ajeno. La validación integral con los bindings instalados y los juegos corresponde al usuario.

## Ruta causal

1. Entrada: `.nsmeta` producido antes de `cc31cdf7a`, incluido el artefacto Assimp reportado.
2. Operación fallida: `LibraryMetadataCodec::read` recalcula la huella mediante `encodeAbiProjection`.
3. Invariante violado: una sección introducida en NSMETA 1.40 no puede participar en la codificación histórica de
   formatos que no la contenían.
4. Símbolo responsable: `encodePayload`, invocado por `encodeAbiProjection` después de proyectar el minor a 24.
5. Consumidor afectado: carga de `LibraryReference` durante `precompiled-metadata`.
6. Comportamiento esperado: autenticar la proyección exacta que firmó el productor y validar después el contenido
   y los contratos reales.
7. Efectos que no deben cambiar: integridad criptográfica, validaciones de estructura, target y ABI reales.

## Presupuesto de cambio

| Dimensión | Autorizado |
|---|---|
| Repositorios | `A:\nsc-devel` |
| Componentes | `Interop::LibraryMetadata` |
| Archivos de producto | `src/Interop/LibraryMetadata.cpp` |
| Archivos de prueba | Las tres unidades focales indicadas arriba |
| Comportamientos | Proyección histórica, sección 1.40 y neutralidad de etiquetas de productor |
| Pruebas | Dos targets/casos focales ejecutables; Stage48 sólo como expectativa corregida; ninguna batería general |
| Configuraciones | La ya configurada en `A:`; O0 focal |
| Perfiles | Correctivo local; sin baseline ni deploy |

## Implementación aplicada

- `encodePayload` sólo escribe la sección `traitImpls` cuando el formato que se está codificando la contiene.
- `encodeAbiProjection` conserva explícitamente esa sección para metadata 1.40 aun después de proyectar el payload
  común a la representación histórica neutral 1.24.
- Una metadata anterior a 1.40 ya no recibe el contador de cuatro bytes inexistente al recalcular su huella.
- `verifyCurrentCompiler` dejó de rechazar por las etiquetas de productor `0.9.0` y `1.0.1`; el contenido decodificado,
  el target y los contratos ABI reales continúan siendo las fronteras funcionales.
- Los goldens históricos permanecieron inmutables.

## Validación ejecutada

- Compilación dirigida: `nsc_library_metadata_version_tolerance_tests`, correcta. Esta compilación recompiló
  `src/Interop/LibraryMetadata.cpp` y enlazó la unidad focal.
- `nsc.library-metadata-version-tolerance`: `PASSED`, 1/1. La prueba leyó físicamente el golden inmutable
  `historical-1.37.nsmeta`, aceptó etiquetas de productor históricas y comprobó que una conformidad 1.40 sigue
  serializada, legible y participando en la huella ABI.
- Compilación dirigida: `nsc_item407_official_promotion_tests`, correcta.
- `nsc.item407.official-promotion`: `PASSED`, 1/1.
- `git diff --check`: sin errores.
- `nsc.stage48_2.library-metadata` se intentó antes del parche, pero el ejecutable de esa configuración llegó con
  quince fallos de productos DLL/archive y terminó en `SegFault` antes de servir como oráculo aislado de esta causa.
  No se corrigieron ni investigaron esos fallos ajenos y, por instrucción posterior del usuario, no se repetirá esa
  prueba ni se ejecutará otra compilación en esta tanda.
- No se ejecutaron baseline, batería completa, deploy, instalación, bindings ni juegos. La validación del artefacto
  Assimp instalado y de los juegos queda a cargo del usuario después de compilar e instalar NSC.
