# Tanda de validación 170 — Oráculos de ausencia para Frontend

Fecha: 2026-10-10

## Estado

IMPLEMENTADA. VALIDACIÓN DIRIGIDA PENDIENTE.

## SCOPE LOCK

Esta tanda corrige exclusivamente el defecto compartido de
`expectProjectStage` y sus 34 consumidores cuya variante 2 vacía la fuente,
pero cuyo oráculo positivo todavía exigía símbolos o modelos poblados.

Dentro del alcance:

- conservar la fuente vacía de la variante 2;
- exigir que la etapa solicitada complete sin error;
- exigir que el modelo correspondiente exista y esté construido;
- verificar ausencia de registros producidos por declaraciones;
- mantener sin cambios las variantes no vacías.

Fuera del alcance:

- compilador, Builder 3 y manifiestos;
- `TF-FRONT-043`, corregido separadamente;
- macros y los seis contratos genéricos cuya matriz necesita rediseño propio;
- omitir el oráculo o convertir el boundary vacío en otra variante nominal.

## Causa confirmada

`FrontendContractSupport.h::expectProjectStage` llama `source.clear()` para la
variante 2. La etapa vacía completa correctamente, pero después se ejecutaba el
mismo oráculo positivo que exige miembros, tipos, bindings, funciones o nodos.
El fallo pertenecía al contrato de prueba, no al compilador.

La corrección añade un `ProjectOracle` de ausencia opcional. Sólo la variante
2 de los consumidores que optan explícitamente entra a ese oráculo. Cada
oráculo negativo inspecciona las mismas colecciones públicas que su contraparte
positiva, comprobando modelo construido y colecciones vacías.

## Inventario exacto — 34

- Nombres/miembros: `TF-FRONT-091`, `115`, `123`, `131`, `139`.
- Genéricos: `TF-FRONT-155`, `163`, `179`.
- Cuerpos: `TF-FRONT-243`, `251`, `259`, `267`, `275`, `283`, `291`,
  `299`, `307`, `315`, `323`, `331`, `339`, `355`.
- Modelos auxiliares: `TF-FRONT-371`, `379`, `395`, `419`.
- Lowering: `TF-FRONT-443`, `451`, `459`, `467`, `475`, `483`, `491`,
  `499`.

## Archivos

- `tests/compiler-suite/host/modules/FrontendContractSupport.h`
- `tests/compiler-suite/host/modules/FrontendNamesMembersModule.cpp`
- `tests/compiler-suite/host/modules/FrontendGenericsFlowModule.cpp`
- `tests/compiler-suite/host/modules/FrontendMethodBodiesModule.cpp`
- `tests/compiler-suite/host/modules/FrontendAuxiliaryModelsModule.cpp`
- `tests/compiler-suite/host/modules/FrontendLoweringModule.cpp`

## Contrato de validación

Después del commit causal se construirá únicamente `nsc_component_tests`
Debug y se ejecutarán exactamente los 34 IDs anteriores. La tanda sólo cierra
si no existen IDs faltantes o adicionales y los 34 terminan en PASS. No
corresponde ejecutar todavía la batería integral.
