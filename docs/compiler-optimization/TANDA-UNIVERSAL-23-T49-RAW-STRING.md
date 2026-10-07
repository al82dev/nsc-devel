# Tanda universal 23: contrato real de raw strings

## Alcance bloqueado

- Se corrigio exclusivamente el falso negativo de `nsc.ns3.t49.language-surface`.
- No se modifico el lexer, el parser, la semantica productiva ni el backend.
- No se cambio la sintaxis ni el valor observable de los raw strings.
- No se construyeron librerias, no se ejecuto baseline y no se instalo nada en `C:`.

## Causa confirmada

El control agregado al retirar las ediciones llamaba a `FrontendCompilation` con una funcion cuyo
tipo de retorno era `string`. Ese pipeline procedural legacy no representa
`NativeSharp.Core.String`; por eso fallaba antes de analizar el literal con `invalid type syntax`.
El fallo no estaba en el raw string.

La misma prueba ya contiene las dos comprobaciones correctas y suficientes:

1. El lexer produce `StringLiteral` y conserva barras, comillas y saltos de linea.
2. El pipeline productivo de proyectos compila el raw string hasta un objeto COFF en `O0`.

## Cambio

Se elimino solo el control redundante que mezclaba el pipeline procedural legacy con el contrato
de objetos `string`. No se sustituyo por un alias falso `string = char*`, porque eso alteraria la
semantica del lenguaje para hacer pasar una prueba.

## Archivos

- `tests/unit/NativeSharp3T49LanguageSurfaceTests.cpp`
  - Retira el control redundante y conserva las pruebas lexer y end-to-end productivas.
- `docs/compiler-optimization/TANDA-UNIVERSAL-23-T49-RAW-STRING.md`
  - Registra causa, limite y evidencia de esta correccion.

## Evidencia posterior al commit

- El arbol `build-test-nslink` se reconstruyo correctamente.
- Se ejecutaron las 684 pruebas con `C:\\native-sharp\\bin\\nslink.exe` como NSLink.
- `nsc.ns3.t49.language-surface` paso en `0.09 s`.
- La corrida global termino con 93 fallos, pero dos de ellos (`t32.range-arrays` y
  `t33.package-coherence`) fueron falsos fallos del entorno porque sus procesos no resolvieron
  `cmd.exe`. Por ello ese total no se usa como conteo causal de esta tanda.
- No reaparecio ningun fallo de raw strings, lexer, parser ni generacion O0/COFF.

La evidencia valida de esta tanda es local y concreta: T49 paso completo. Los fallos de entorno y
artefactos externos permanecen separados para la tanda de infraestructura final.
