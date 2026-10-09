# Gate de integridad previo a Tanda 12

## Regla aplicada

El `SCOPE LOCK` exige tratar todo el compilador versionado y sus dependencias directas. La Tanda 01 inventarió
974 archivos y 57 componentes, y advirtió expresamente que enumerar no significaba leer, consolidar o auditar.

## Hallazgo comprobado

Al iniciar la consolidación de la arquitectura se comparó `components.csv` con los documentos y test plans
existentes:

| Universo | Componentes | Archivos | Estado al hallarlo |
|---|---:|---:|---|
| Compilador host `Host.*` | 24 | 524 | `AUDITADO` / `RECONSTRUIBLE` |
| Compilador `selfhost/nsc/**` | 16 | 29 | `NO LEÍDO` / `NO VERIFICADO` |
| Build, contratos self-host, dependencias y soporte | 17 | 421 | `NO LEÍDO` / `NO VERIFICADO` |
| Total canónico | 57 | 974 | tratamiento incompleto |

La matriz host contiene 5,294 símbolos, 870 contratos y 7,360 casos. Presentar esos valores como el conteo final
de todo NSC habría omitido 450 archivos y 33 componentes documentales.

## Primera decisión incorrecta

El plan original saltaba directamente de la auditoría de `Host.*` a la arquitectura de la suite. La enumeración
mecánica sí conservó los demás archivos, pero las Tandas 03 a 10 no les asignaron una lectura semántica ni una
justificación explícita de prueba indirecta. El error fue de partición del trabajo, no de pérdida del inventario.

## Corrección

1. Tanda 12A audita los 29 archivos y 16 componentes del compilador self-host.
2. Tanda 12B trata los 421 archivos y 17 componentes restantes, distinguiendo contrato directo, integración,
   verificación upstream o exclusión justificada.
3. Tanda 12C deriva ejecutores, frecuencias y conteos desde las matrices completas.
4. Tanda 13 reenumera los 974 archivos y rechaza el cierre ante cualquier estado sin tratamiento.

## Límite

Este gate no modifica código, pruebas ni infraestructura y no afirma que las Tandas 12A o 12B ya estén cerradas.
Conserva el objetivo original y evita una declaración de cobertura parcial.
