# Tanda 02.02 — validación integrada de Host.Lexer

## Alcance

Se validó exclusivamente el componente nuevo `Host.Lexer` desde el commit
`3a03d9348`. No se ejecutaron la batería histórica, el baseline, cobertura,
instalación, despliegue ni herramientas externas al repositorio.

## Release

1. CMake reconfiguró `build-compiler-suite-infra` como Release.
2. `nsc_component_tests` compiló y enlazó con advertencias tratadas como error.
3. La selección ordinaria `--component Host.Lexer` ejecutó `109` casos:
   `109 PASS`, salida `0`.
4. `nsc.compiler-suite.host-lexer-controller` ejecutó los `110` manifiestos:
   `1/1` gate PASS en `6.47 s`.

El gate sólo puede pasar con `109 PASS`, `1 KNOWN_FAILURE` y cero estados
inesperados; también comprueba identidad, contrato y componente de cada resultado.

## Debug

1. CMake creó `build-compiler-suite-infra-debug` como Debug.
2. `nsc_component_tests` compiló y enlazó desde cero con advertencias tratadas
   como error.
3. La selección ordinaria ejecutó `109` casos: `109 PASS`, salida `0`.
4. El mismo gate de `110` manifiestos pasó `1/1` en `111.67 s`.

## Resultado

| Estado | Cantidad |
|---|---:|
| Implementados | 110 |
| Verificados `PASS` | 109 |
| `KNOWN_FAILURE` ejecutable | 1 |
| Fallos inesperados | 0 |
| Limitaciones nuevas | 0 |

`TL-LEX-075` continúa rojo por el defecto documentado del delimitador raw de
seis comillas. La ola 2 permanece `EN CURSO` y continúa con `Host.AST`.
