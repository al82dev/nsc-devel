# Validación 167: fixtures Frontend de `move`, excepciones y resultado opaco

## SCOPE LOCK

- Objetivo: corregir exclusivamente las cinco fixtures compartidas que
  producen `25` fallos `Host.Frontend` por usar sintaxis o categorías de tipo
  que ya no pertenecen al contrato vigente del lenguaje.
- Productores autorizados:
  - `tests/compiler-suite/host/modules/FrontendNamesMembersModule.cpp`, contrato
    `CF-FRONT-013`;
  - `tests/compiler-suite/host/modules/FrontendMethodBodiesModule.cpp`,
    contratos `CF-FRONT-033`, `CF-FRONT-035` y `CF-FRONT-042`;
  - `tests/compiler-suite/host/modules/FrontendAuxiliaryModelsModule.cpp`,
    contrato `CF-FRONT-054`.
- Archivos autorizados: los tres productores anteriores y este documento.
- Fuera de alcance: compilador, `FrontendContractSupport.h`, `nscbuilder3.py`,
  `TRACKING.md`, manifiestos y los seis casos `TF-FRONT-161`, `TF-FRONT-162`,
  `TF-FRONT-163`, `TF-FRONT-164`, `TF-FRONT-167` y `TF-FRONT-168` auditados
  separadamente.
- Validación autorizada después del commit: sólo los `25` identificadores de
  este documento, mediante el runner dirigido de Builder 3, con build `Debug`;
  los ejecutores host publican la optimización canónica `NA` aunque la tanda se
  solicite como Debug/O0.

## Autoridad de entrada

La corrida integral conservada en
`_nscbuilder3/history/20261010-142113-a514d35c` registra tres causas de entrada
inválida antes de llegar a los modelos que los oráculos pretenden inspeccionar:

1. Los contratos `CF-FRONT-013`, `CF-FRONT-033` y `CF-FRONT-042` escriben
   `move value`, `move input` y `move source`; el parser vigente exige
   `move(<expresión>)`.
2. `CF-FRONT-035` ejecuta `throw;` fuera de un `catch`, aunque la forma sin
   operando sólo es válida para relanzar una excepción capturada.
3. `CF-FRONT-054` declara `IValue` como `interface`, pero `some IValue` exige
   que el tipo opaco visible sea un `trait`.

En los cinco contratos, `expectProjectStage()` sustituye o evita la fuente en
las variantes `2`, `4` y `5`. Las variantes que consumen cada fixture son `0`,
`1`, `3`, `6` y `7`: cinco casos por contrato, `5 * 5 = 25`.

## Inventario exacto

- `CF-FRONT-013`: `TF-FRONT-097`, `TF-FRONT-098`, `TF-FRONT-100`,
  `TF-FRONT-103`, `TF-FRONT-104`.
- `CF-FRONT-033`: `TF-FRONT-257`, `TF-FRONT-258`, `TF-FRONT-260`,
  `TF-FRONT-263`, `TF-FRONT-264`.
- `CF-FRONT-035`: `TF-FRONT-273`, `TF-FRONT-274`, `TF-FRONT-276`,
  `TF-FRONT-279`, `TF-FRONT-280`.
- `CF-FRONT-042`: `TF-FRONT-329`, `TF-FRONT-330`, `TF-FRONT-332`,
  `TF-FRONT-335`, `TF-FRONT-336`.
- `CF-FRONT-054`: `TF-FRONT-425`, `TF-FRONT-426`, `TF-FRONT-428`,
  `TF-FRONT-431`, `TF-FRONT-432`.

## Causa raíz confirmada

Clasificación común: `DEFECTO_DE_PRUEBA_ORACULO`. Las fixtures conservaban
formas anteriores o incompatibles con el contrato real del lenguaje. Los
diagnósticos del compilador son correctos y, al detener el pipeline antes del
modelo esperado, no prueban un defecto de producto.

Las tres subcausas son independientes en la superficie sintáctica, pero
comparten productor causal: fuentes supuestamente válidas que quedaron
desalineadas respecto del lenguaje vigente.

## Correcciones semánticamente fieles

- Se aplican paréntesis a las tres transferencias de propiedad:
  `move(value)`, `move(input)` y `move(source)`. No cambia el objeto movido, el
  destino, la evaluación ni el orden de uso.
- La fixture de flujo excepcional declara una excepción `Problem` derivada de
  `NativeSharp.Core.Exception`, lanza una instancia dentro del `try`, la
  captura mediante `catch (Problem problem)` y conserva un `finally`. Así se
  ejercitan explícitamente `throw`, `catch` y `finally` sin un relanzamiento
  inválido; la ruta negativa normaliza `x`, el `finally` mantiene su efecto y
  el método retorna después de completar el flujo protegido.
- `IValue` pasa de `interface` a `trait`; se conservan el miembro `Read`, la
  implementación estructural, el contribuyente concreto y el retorno
  `some IValue` que el contrato pretende validar.

## Contrato de validación

Los cuatro archivos de esta tanda se commitean antes de ejecutar pruebas.
Después del commit se compila el host consolidado en `Debug` y se ejecutan
exclusivamente los `25` casos inventariados mediante selección explícita. La
envolvente canónica del host registra `optimization: NA`; `O0` no constituye
una variante adicional para esta familia. La evidencia debe conservar
`results.json`, `summary.md`, `events.jsonl` y logs individuales bajo
`_nscbuilder3/directed`.

## Estado previo a validación

- Auditoría causal e inventario: completos.
- Correcciones de fixtures: implementadas.
- Commit funcional previo a pruebas: pendiente de crear.
- Reejecución exacta: pendiente.
- Cambios en compilador, helper compartido, Builder 3, `TRACKING.md` o
  manifiestos: ninguno.
