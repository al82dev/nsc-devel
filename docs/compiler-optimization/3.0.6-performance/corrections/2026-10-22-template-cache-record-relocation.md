# Corrección de relocalización de registros de plantillas en caché

## SCOPE LOCK

- Objetivo: corregir `TL-LANG-475` y `TL-LANG-494` restaurando el registro C1 contra el AST y los
  ordinales de la compilación actual.
- Naturaleza: corrección funcional de una regresión introducida al activar el salto de construcción
  de metadatos en hits C1/C2.
- Revisión fallida: `d0d24481bfa9f605496c0ab5da6acc703b174c5e`.
- Productor responsable: `ProjectGenericModel::tryRestoreTemplateRecord`.
- Archivos de producto autorizados: `ProjectGenericModel.h`, `TemplateCacheBridge.cpp` y
  `TemplateCollector.cpp`.
- Protegidos: pruebas, fixtures, CMake, NSC Builder, sintaxis, ABI, diagnósticos, NSMETA, HIR, MIR,
  LLVM IR y contratos congelados de 3.0.4.
- Validación autorizada: construir el target exacto `nsc` y ejecutar focalmente `TL-LANG-475` y
  `TL-LANG-494`.
- Prohibido: suites, baterías, minibaseline, baseline, pruebas externas, instalación y deploy.
- Terminación: ambos casos focales pasan sin modificar fuentes ni expectativas de prueba.

## Evidencia previa

- `TL-LANG-475` pasa en frío con `--no-artifact-cache`, pero su segundo proyecto termina con
  `0xC0000005` después de que el primer proyecto publica el caché.
- `TL-LANG-494` pierde plantillas genéricas visibles de Core al compilar el consumidor después del
  productor.
- `tryRestoreTemplateRecord` usa `deserializeTemplateRecord`, que no restaura
  `parameterSyntaxes`, `constraints`, `extensionSyntaxes` ni `externalTypeReferences`, y conserva
  los ordinales serializados por otra compilación.
- Las dos únicas llamadas a ese productor sustituyen por completo la construcción fría del
  registro cuando hay hit.

## Hipótesis causal

El payload C1/C2 conserva metadatos reutilizables, pero las referencias al AST y los ordinales son
propios de cada compilación. Al saltarse la construcción fría, el registro restaurado queda
incompleto o asociado al grafo anterior. La corrección mínima es relocalizar esos campos contra la
unidad y el nodo que produjeron el hit, conservando el contenido cacheable.

## Corrección

`tryRestoreTemplateRecord` recibe el namespace y el owner encontrados por el recorrido actual. Tras
deserializar el payload, vuelve a enlazar el nodo, los ordinales, el módulo, la identidad de
declaración, los parámetros, las restricciones y las extensiones contra esa compilación. Las
dependencias cacheables permanecen reutilizadas y el miss conserva la construcción fría anterior.

## Contratos conservados

- No cambian sintaxis, resolución fría, diagnósticos esperados ni ABI.
- No cambian formatos de caché ni NSMETA.
- No se modifican pruebas, fixtures ni expectativas.
- El cambio sólo altera un hit C1/C2 que antes publicaba referencias incompletas o pertenecientes a
  otra compilación.

## Validación focal

- Target `nsc`: 282/282 pasos, código 0.
- `TL-LANG-475`: 1 PASS / 0 FAIL; evidencia
  `A:\_codex-unit-cycle\20261022-focal-TL-LANG-475-05`.
- `TL-LANG-494`: 1 PASS / 0 FAIL; evidencia
  `A:\_codex-unit-cycle\20261022-focal-TL-LANG-494-04`.
- Confirmación combinada `TA-AST-016`: 1 PASS / 0 FAIL; evidencia
  `A:\_codex-unit-cycle\20261022-focal-TA-AST-016-04`.
- Confirmación combinada `TL-LANG-520`: 1 PASS / 0 FAIL; evidencia
  `A:\_codex-unit-cycle\20261022-focal-TL-LANG-520-05`.

## Estado

`CORRECCIÓN FOCAL VERIFICADA; BATERÍA UNITARIA COMPLETA Y CORRIDA EXTERNA PENDIENTES`.
