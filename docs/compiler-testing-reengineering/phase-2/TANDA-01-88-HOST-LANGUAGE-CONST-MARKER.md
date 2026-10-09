# Tanda 01.88 — Host.Language: rechazo de const marker

Fecha: 2026-10-09

## SCOPE LOCK local

| Dimensión | Autorizado |
|---|---|
| Caso | `TL-LANG-116` |
| Contrato | `CL-LANG-009` |
| Producto | Protegido; no modificar |
| Suite nueva | Módulo de genéricos y manifiesto |
| Suite histórica | Intacta; no ejecutar |
| Excluido | Bibliotecas, SDK, bindings y otros repositorios |

## Contrato comprobado

El formato vigente es
`$nsc_const$<typeLength>:<type>:<normalizedValue>`. La longitud sólo delimita
`type`; el valor normalizado ocupa el sufijo completo.

La matriz verifica:

- prefijo incorrecto;
- separador de longitud ausente;
- longitud no decimal;
- overflow al acumular la longitud;
- longitud declarada incompatible con el tipo disponible;
- separador entre tipo y valor ausente;
- valor normalizado vacío;
- conservación exacta del estado de salida para cada rechazo;
- consumo completo de un valor válido que contiene `:`.

El último caso demuestra que `1:tail` es un valor completo, no un valor seguido
por bytes sobrantes. La prueba sigue el productor y parser existentes; no cambia
ni amplía el formato.

## Presupuesto de cambio

| Archivo | Cambio |
|---|---|
| `tests/compiler-suite/host/modules/LanguageGenericSemanticsModule.cpp` | Registrar y ejecutar `TL-LANG-116`. |
| `tests/compiler-suite/manifest/cases.jsonl` | Publicar el caso y su oráculo exacto. |
| Este documento | Conservar alcance, contrato y evidencia. |

## Comportamiento preservado

- No se modifican fuentes de producto bajo `include/` o `src/`.
- No se cambia el tratamiento de valores normalizados.
- No se modifica la suite histórica.
- No se incorpora ningún componente externo a NSC.

## Evidencia

- Build dirigido `nsc_component_tests` en configuración Release: correcto, con
  warnings tratados como errores.
- Ejecución exclusiva `--case TL-LANG-116`: 1/1, `PASS`, 902 microsegundos.
- Manifiesto parseado: 734 casos, 0 duplicados y una entrada exacta para
  `TL-LANG-116`.
- Diferencial bajo `include/` y `src/`: 0 archivos.
- No se ejecutaron la suite completa, la batería histórica, baseline, NSC,
  NSLink, cobertura, instalación ni despliegue.

## Estado

`TL-LANG-116` queda implementado y verificado contra el formato público vigente.
