# Tanda universal 48: selección explícita de perfiles self-host

## SCOPE LOCK

- Objetivo: hacer que la batería CTest respete literalmente los perfiles seleccionados en NSC
  Builder, comenzando por O0.
- Naturaleza: corrección de infraestructura y trazabilidad de ejecución.
- Comportamiento protegido: una certificación sin filtro continúa validando O0/O1/O2/O3; los
  contratos funcionales y sus expectativas no se reducen.
- Archivos protegidos: compilador, parser, semántica, lowering, runtime, bibliotecas, instalación
  `C:` y fixtures del lenguaje.
- Prohibido: baseline certificado, deploy, ejecución O1/O2/O3 en la tarea vigente, ocultar perfiles
  no ejecutados o presentar una selección O0 como certificación completa.
- Terminación: el selector se propaga al proceso CTest, forma parte de la identidad de caché y cada
  verificador self-host consume exactamente el subconjunto pedido.

## Causa raíz

`RunPlan.optimizations` conserva la selección de la GUI para fixtures y bug triage, pero `_ctest`
no la incorporaba al entorno. Los gates SH40–SH50 volvían a leer listas fijas de cuatro perfiles en
sus contratos. Por eso una corrida solicitada como O0 ejecutaba silenciosamente O1/O2/O3 y no podía
distinguir un defecto O0 de un cierre perteneciente a otra matriz.

## Primera parte

1. `profile_selection.py` define una selección ordenada, sin duplicados y validada contra el
   contrato. Sin variable de entorno devuelve el conjunto completo.
2. NSC Builder publica `NSC_SELFHOST_PROFILES` desde `RunPlan.optimizations`.
3. La selección entra en la identidad de caché CTest; un resultado de cuatro perfiles no puede
   reutilizarse como resultado O0 ni al revés.
4. Las pruebas unitarias cubren el valor predeterminado, O0, rechazo de perfiles inválidos y la
   propagación desde NSC Builder.

## Archivos

- Nuevo: `selfhost/bootstrap/profile_selection.py`.
- Modificado: `nscbuilder.py`.
- Modificado: `tests/selfhost/test_bootstrap_infrastructure.py`.
- Nuevo: este documento.

## Complejidad

La selección recorre una vez el conjunto de perfiles, como máximo cuatro elementos. Tiempo y
memoria son `O(P)`; no existe recursión.

## Estado

- Selector y propagación: implementados.
- Consumo por los verificadores SH40–SH52: pendiente dentro de esta misma tanda.
- Validación dirigida posterior al commit `31378cec1`: `3/3` pruebas unitarias aprobaron en
  `0.006 s`; cubren el valor completo predeterminado, la selección O0, el rechazo de O4 y la
  exportación desde NSC Builder.
- Inventario integral vigente: diez fallos self-host abiertos.
