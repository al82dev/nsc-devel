# Tanda 02.05 — validación integrada de Host.AST

## Alcance

Se validó exclusivamente el componente nuevo `Host.AST` materializado en el
commit `3644ba798`, junto con la compatibilidad de la capa canónica
`robustness` incorporada en `c15136e8c`. No se ejecutaron la batería histórica,
el baseline, cobertura, instalación, despliegue ni componentes externos a NSC.

## Inventario

El plan canónico contiene `230` casos `TA-AST-001..230`. Se materializaron y
registraron `224`; los seis restantes conservan estado de infraestructura
especial pendiente:

- `TA-AST-014`: inyección determinista de fallo de asignación;
- `TA-AST-016`: death test por terminación desde destructor;
- `TA-AST-017`: detector de carreras;
- `TA-AST-215`: death test para argumento de tipo nulo;
- `TA-AST-223` y `TA-AST-224`: watchdog de subproceso para ciclos no terminantes.

El manifiesto integrado contiene exactamente los `224` identificadores
ejecutables y conserva sus capas canónicas `unit`, `component` o `robustness`.

## Release

1. CMake configuró el árbol `build-compiler-suite-infra` como Release.
2. `nsc_component_tests` compiló y enlazó sus `133` pasos con advertencias
   tratadas como error.
3. `nsc.compiler-suite.host-ast-controller` ejecutó el inventario exacto de
   `224` casos y pasó `1/1` en `20.07 s`.
4. El gate confirmó `224 PASS`, cero estados inesperados y cero casos ausentes.

## Debug

1. CMake configuró el árbol `build-compiler-suite-infra-debug` como Debug.
2. `nsc_component_tests` compiló y enlazó sus `133` pasos con advertencias
   tratadas como error.
3. El mismo gate ejecutó el inventario exacto de `224` casos y pasó `1/1` en
   `394.79 s`.
4. El gate confirmó `224 PASS`, cero estados inesperados y cero casos ausentes.

## Infraestructura de control

Las pruebas Python del controlador pasaron `56/56`, incluida la aceptación
explícita de la capa `robustness` para casos host. Esta compatibilidad no cambia
el producto ni reduce el inventario; únicamente permite representar la capa ya
definida por la Fase 1.

## Resultado

| Estado | Cantidad |
|---|---:|
| Casos canónicos | 230 |
| Implementados | 224 |
| Verificados `PASS` | 224 |
| Defectos de producto observados | 0 |
| Fallos inesperados | 0 |
| Infraestructura especial pendiente | 6 |

`Host.AST` queda validado en sus `224` casos ejecutables. La ola 2 permanece
`EN CURSO` y continúa con los `536` casos canónicos de `Host.Parser`.
