# Tanda de validación 170 — Oráculos de ausencia para Frontend

Fecha: 2026-10-10

## Estado

VALIDADA. `34/34 PASS` EN `Debug/NA`.

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

## Resultado de validación

- Commit funcional validado:
  `de719a9fe6f93fe3a0e8b4556491a96b2a24d90a`.
- Build exclusivo:
  `cmake --build A:/nsc-devel/build-compiler-suite-v3/debug --config Debug --target nsc_component_tests --parallel 12`.
- Resultado del build: correcto; se compilaron los cinco módulos consumidores
  modificados y se enlazó `nsc_component_tests.exe`.
- Corrida dirigida:
  `20261010-tanda170-frontend-absence-de719a9fe`.
- Configuración publicada: alcance `focused`, perfil `explicit`, build
  `Debug`, optimización canónica host `NA` para la solicitud `O0`.
- Identidad: `34` seleccionados, `34` resultados, `0` faltantes, `0` extras y
  `0` duplicados.
- Resultado terminal: `34 PASS`, `0 FAIL`, `0 KNOWN_FAILURE`, `0 BLOCKED`,
  `0 INFRASTRUCTURE_ERROR`, `0 CANCELLED`, `0 SKIPPED` y
  `0 UNEXPECTED_PASS`.
- Evidencia:
  `_nscbuilder3/directed/20261010-tanda170-frontend-absence-de719a9fe`.
- Recibos conservados: `34` directorios de caso, `34` logs `stdout.log`,
  `results.json`, `summary.md`, `events.jsonl` y `failures.log` vacío.

## SHA-256

| Artefacto | SHA-256 |
|---|---|
| `build-compiler-suite-v3/debug/nsc_component_tests.exe` | `cfab8e6cec3d7e84d63ee3a57b7af725d6544cb450b6340d0c1558a5fcc3c2a2` |
| `results.json` | `57b8128b3a00c08a7c0610690e7e70c0df7425332a99ad4b8a32a4e10a2bfc76` |
| `summary.md` | `a3541355505a9682696bad923461a58143081191a7ec6c318c8562450038c9a7` |
| `events.jsonl` | `e47ae30110a5012ecc76a87c91ff7bc43986d2d39f74935eee429411ae0cd522` |

Esta validación dirigida cierra únicamente los 34 boundaries de ausencia. No
se ejecutó ni se promueve como completada la batería integral de 5,695
variantes.
