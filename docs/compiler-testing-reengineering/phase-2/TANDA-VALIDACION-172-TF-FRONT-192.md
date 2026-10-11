# Tanda de validación 172 — TF-FRONT-192

Fecha: 2026-10-10

## Estado

VALIDADA. `1/1 PASS` EN `Debug/NA`.

## SCOPE LOCK

Esta reapertura de Tanda 2 corrige exclusivamente `TF-FRONT-192|Debug|NA`,
único caso Frontend en `FAIL` dentro del gate
`20261010-181736-433daee9`.

Dentro del alcance:

- clasificar la causa contra el contrato `CF-FRONT-024`;
- corregir la fixture y el oráculo de la variante NIGHTLY de escala;
- conservar las identidades públicas del productor como autoridad;
- validar únicamente `TF-FRONT-192` después del commit causal.

Fuera del alcance:

- compilador, manifiesto, Builder 3 y `TRACKING.md`;
- `TF-FRONT-185..191` y cualquier otro caso;
- cambiar la gramática pública de identidades de tupla;
- promover esta prueba dirigida como gate o corrida integral.

## Evidencia observada

La fila terminal de `TF-FRONT-192|Debug|NA` registra:

- estado `FAIL`;
- contrato `CF-FRONT-024`;
- detalle `the selected canonical implicit conversion class must exist`;
- recibo
  `_nscbuilder3/history/20261010-181736-433daee9/cases/TF-FRONT-192-Debug-NA/host.log`.

El host publicó un envelope válido y una sola expectativa fallida. No hubo
crash, timeout, bloqueo de productor ni error de infraestructura.

## Causa confirmada

Clasificación: `DEFECTO_DE_PRUEBA_ORACULO`.

`FrontendGenericsFlowModule.cpp::conversionOracle` inventaba estas cadenas:

- `tuple:builtin:int,builtin:long`;
- `tuple:builtin:long,builtin:long`.

No son identidades de tupla válidas. El productor define la representación
canónica mediante `ProjectTypeConversion::tupleTypeKey`: prefijo `tuple:(`,
longitud decimal de cada identidad, separador `:`, comas y paréntesis de
cierre. `tupleElementTypeKeys` rechaza correctamente la escritura inventada y
`classifyImplicit` devuelve `ProjectConversionKind::None` antes de clasificar
elementos.

El comportamiento del producto coincide con su contrato: una forma
estructural incompatible o inválida no adquiere una conversión. No corresponde
modificar `ProjectTypeConversion.cpp`.

## Corrección mínima fiel

Sólo la variante 7 pasa a un oráculo dedicado que construye todas las claves
con `ProjectTypeConversion::tupleTypeKey`:

- fanout de 64 elementos, con conversiones `int/short -> long`;
- profundidad de 16 tuplas anidadas;
- `ProjectConversionKind::Tuple` y rank estructural exacto;
- payload exacto de source y target canónicos;
- round-trip de los 64 elementos mediante `tupleElementTypeKeys`;
- rechazo del primer target con aridad estructural incompatible.

El productor no expone presupuesto configurable ni cancelación para esta API
pura; por ello esas fronteras no aplican. La prueba sí exige terminación
acotada para profundidad y amplitud representativas sin reinterpretar el
contrato.

Las variantes `TF-FRONT-185..191` conservan sin cambios sus sources, targets y
oráculos.

## Archivos autorizados

- `tests/compiler-suite/host/modules/FrontendGenericsFlowModule.cpp`
- `docs/compiler-testing-reengineering/phase-2/TANDA-VALIDACION-172-TF-FRONT-192.md`

## Compuerta de validación

Después del commit causal se construirá únicamente `nsc_component_tests` en
Debug y se ejecutará exactamente `TF-FRONT-192|Debug|NA`, configuración
canónica host para la solicitud `Debug/O0`.

La tanda sólo podrá pasar a VALIDADA si el build termina correctamente y la
corrida publica un seleccionado, un resultado, `1 PASS`, cero faltantes,
extras o duplicados, y conserva sus recibos. No se ejecutará ningún otro caso.

## Resultado de validación

- Commit funcional validado:
  `022e3aaab8eb567d663f06537146a8cfc14eb3d0`.
- Build exclusivo:
  `cmake --build A:/nsc-devel/build-compiler-suite-v3/debug --config Debug --target nsc_component_tests --parallel 12`.
- Resultado del build: correcto; se recompiló
  `FrontendGenericsFlowModule.cpp` y se enlazó
  `nsc_component_tests.exe`.
- Corrida dirigida:
  `20261010-tanda172-tf-front-192-022e3aaab`.
- Configuración publicada: alcance `focused`, perfil `explicit`, build
  `Debug`, optimización canónica host `NA` para la solicitud `O0`.
- Identidad: un seleccionado, un resultado, cero faltantes, extras y
  duplicados.
- Resultado terminal: `1 PASS`; cero resultados en cualquier otro estado.
- Evidencia:
  `_nscbuilder3/directed/20261010-tanda172-tf-front-192-022e3aaab`.

## SHA-256

| Artefacto | SHA-256 |
|---|---|
| `build-compiler-suite-v3/debug/nsc_component_tests.exe` | `5efae89aeb548339e6b2a015efc93c8fe8d859bb34c26c00608763d3e6031f23` |
| `results.json` | `0a76fb763b333adcf438cd6a11fcc9fc43258071820f5d14c03b021513f8739d` |
| `summary.md` | `712631e759b678b63bf02ed83c7266dc63f9f20d633cadb735097e58c78eeef2` |
| `events.jsonl` | `d916172b6681cd42eeb18c9db778ee0d437d1bd17aeae1b1fc15fe258659cc3a` |
| `host.log` | `58681a1b9a852ee614bfee55737814d29d3e91194e005c7ef8044bd39400e7d1` |

Esta validación dirigida cierra el último caso Frontend que permanecía en
`FAIL` tras el primer gate de Tanda 2. No equivale al gate correctivo ni a la
corrida integral.
