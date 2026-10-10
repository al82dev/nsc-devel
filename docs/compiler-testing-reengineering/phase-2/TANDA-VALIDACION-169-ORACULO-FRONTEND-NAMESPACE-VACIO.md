# Validación 169: oráculo Frontend de namespace vacío

## SCOPE LOCK

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\nsc-devel` |
| Naturaleza | Corrección puntual de prueba/oráculo |
| Componente | `Host.Frontend` |
| Caso | `TF-FRONT-043` |
| Contrato | `CF-FRONT-006` |
| Símbolo | `namespaceOracle()` |
| Código modificable | `tests/compiler-suite/host/modules/FrontendCoreModule.cpp` |
| Documentación modificable | Este documento |
| Configuración de validación | `Debug`; optimización canónica host `NA` para la solicitud `O0` |
| Prueba autorizada | Exclusivamente `TF-FRONT-043` |

- Objetivo exacto: conservar la frontera de entrada vacía y comprobar su
  resultado real, sin exigir una escalera de resolución poblada.
- Comportamiento preservado: las variantes no vacías de `CF-FRONT-006`
  continúan comprobando árbol, padres, nombre completo, ordinales, lookup y
  prioridad `EnclosingType` de la escalera poblada.
- Revisión conocida de entrada: `5ad58bf9e`.
- Rango de commits evaluado: no aplica; se corrige una causa ya confirmada en
  la corrida autoridad `20261010-142113-a514d35c`.
- Archivos protegidos: `FrontendContractSupport.h`,
  `FrontendGenericsFlowModule.cpp`, compilador, `nscbuilder3.py`,
  `TRACKING.md`, manifiestos y cualquier otro caso.
- Acciones prohibidas: generalizar el helper, cambiar el producto, alterar el
  inventario, ejecutar otra prueba o promover esta prueba puntual a corrida
  integral.
- Criterio de terminación: commit causal creado antes de probar y
  `TF-FRONT-043` ejecutado de forma dirigida con evidencia persistida.

## Evidencia y causa raíz

La corrida autoridad conserva para `TF-FRONT-043|Debug|NA` el único fallo:

```text
qualified name ladder must publish an ordered fallback sequence
```

`TF-FRONT-043` es la variante `2` de la matriz de ocho casos de
`CF-FRONT-006`. Su clase es `boundary` y el manifiesto exige cubrir las
representaciones vacías aplicables a los segmentos del namespace y al nombre
candidato.

`namespaceOracle()` pasaba deliberadamente `""` a
`qualifiedNameLadder()`, pero aplicaba después el oráculo de la ruta no vacía.
El contrato real del producto es explícito: un nombre escrito vacío retorna
una colección de escalones vacía. Por tanto, la causa es
`DEFECTO_DE_PRUEBA_ORACULO`, no un defecto del compilador.

## Corrección mínima

La variante `2` conserva su entrada vacía y ejecuta dos oráculos positivos:

1. `ensureNamespace()` con cero segmentos devuelve la raíz y no crea símbolos
   de namespace bajo ella.
2. `qualifiedNameLadder()` con sitio y nombre vacíos devuelve cero candidatos.

Después retorna, de modo que no cae en el oráculo incompatible de la ruta
poblada. Para las demás variantes, la fuente candidata permanece `Thing` y se
mantienen sin cambios las comprobaciones existentes.

## Estado previo a validación

- Auditoría causal: completa.
- Número documental `169`: libre al iniciar el cambio.
- Implementación mínima: aplicada.
- Commit previo a pruebas: pendiente.
- Única reejecución autorizada: pendiente.
