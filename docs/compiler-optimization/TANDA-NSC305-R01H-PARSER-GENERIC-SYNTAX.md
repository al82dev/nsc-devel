# NSC 3.0.5 - R01H: extraccion fisica del parser de sintaxis generica

## Proposito

R01H separa fisicamente de `Parser.cpp` la familia que reconoce parametros, restricciones,
argumentos y lookahead de sintaxis generica. `Parser` permanece como la fachada unica y la nueva
unidad comparte sin intermediarios el mismo cursor, arena, diagnosticos y estado de cierre de
tokens genericos.

La tanda es un refactor no funcional. No agrega sintaxis, no cambia gramatica, AST, rangos,
recovery, diagnosticos, API, ABI, NSMETA ni artefactos emitidos. Tampoco convierte estas rutas a
los algoritmos iterativos de R02/R03.

## SCOPE LOCK

- Objetivo: extraer fisicamente la familia generica y subdividir sus cuerpos por responsabilidad.
- Archivos funcionales: `Parser.h`, `Parser.cpp`, `GenericSyntaxParser.cpp`, CMake raiz, CMake de
  compiler-suite y el shim privado del nuevo modulo.
- Comportamiento protegido: consumo de tokens, `>>`/`>>=`, packs, const generics, lifetimes,
  equality constraints, capabilities, lookahead de llamada/tipo, diagnosticos y recovery.
- Acciones prohibidas: sintaxis nueva, correcciones semanticas, cambios HIR/MIR, metadata,
  fixtures, expectativas, baseline o suite completa.
- Archivo ajeno protegido: `tests/unit/ProjectMethodBodyModelTests.cpp`.
- Criterio: producto y tres targets focales compilables; 40 casos directos y dos smokes PASS;
  infraestructura completa; cero duplicados; ninguna funcion R01H sobre 60 statements.

## Inventario extraido

`GenericSyntaxParser.cpp` contiene 29 definiciones unicas de `Parser`, agrupadas en:

1. parametros genericos ordinarios, constantes, packs y lifetimes;
2. clausulas `where`, igualdad, outlives, capabilities y restricciones nominales;
3. argumentos de tipo, tuplas y argumentos constantes;
4. cierre de tokens `>`, `>>` y `>>=`;
5. lookahead de llamadas genericas y referencias cerradas a tipos genericos.

Los siete entry points productivos conservan su firma y ahora delegan a helpers privados
cohesivos. `clang-tidy` con `readability-function-size` y umbral de 60 statements no reporta
cuerpos de `GenericSyntaxParser.cpp`. El unico aviso pertenece al header preexistente
`DiagnosticId.h`.

## Conservacion algoritmica

La extraccion conserva:

- el orden de lookahead y consumo de cada token;
- la acumulacion de `pendingGenericGreater_` y `pendingGenericEqualToken_`;
- los mismos tipos sintacticos para argumentos de tipo, tupla y constantes;
- la validacion y el orden de diagnosticos de parametros duplicados y packs;
- la distincion entre parametros ordinarios, constantes, packs y lifetimes;
- el orden, flags y rangos de constraints de igualdad, nominales y capabilities;
- las mismas fronteras de recovery y los mismos retornos parciales;
- la misma topologia AST y el mismo estado observable por fases posteriores.

## Frontera de compilacion e infraestructura

- `src/Parser/GenericSyntaxParser.cpp` aparece exactamente una vez en `nsc_frontend`.
- `Parser.cpp` no conserva ninguna de las 29 definiciones movidas.
- `tests/compiler-suite/host/modules/ParserGenericSyntaxProductTestShim.cpp` incluye el modulo
  una sola vez bajo la misma vista privada usada por los otros modulos Parser.
- El CMake de la suite registra el shim exactamente una vez y no registra directamente la fuente
  productiva.
- Los nueve modulos fisicos Parser tienen conteo `root=1`, `suiteDirect=0`, `shim=1`.
- El shim no crea casos. Reutiliza `nsc_component_tests`, su `add_test`, catalogo, manifest y ruta
  default existentes; por ello no se agregan filas ni contratos.
- `validate_source_registration.py` acepta la cadena completa.
- `CanonicalCatalog` carga 5,995 casos, 30 componentes y 682 contratos.

## Cobertura focal

R01H reutiliza pruebas existentes y no modifica fixtures ni expectativas:

- `TP-PAR-070..079`: parametros genericos;
- `TP-PAR-080..093`: restricciones genericas;
- `TP-PAR-094..100`: argumentos genericos;
- `TP-PAR-101..109`: lookahead y cierre generico;
- `TP-PAR-186`: smoke de llamada generica en expresiones;
- `TP-PAR-268`: smoke de tipo generico en declaracion.

El conjunto dirigido contiene 42 casos y termina 42/42 PASS. No sustituye baseline ni suite
completa.

## Evidencia

- Build compartido de `nsc` sobre R01H: PASS, exit 0.
- Build exacto de `nsc_component_tests`, `nsc_component_tests_destructive` y
  `nsc_component_tests_source_extended`: PASS, exit 0.
- Host.Parser focal: 40/40 PASS; smokes: 2/2 PASS; total 42/42 PASS, exit 0.
- `clang-tidy readability-function-size`: cero avisos R01H, threshold 60.
- Definiciones: 29 en `GenericSyntaxParser.cpp`, cero duplicados internos y cero duplicados con
  `Parser.cpp`.
- Registro mecanico: nueve fuentes Parser productivas una vez en raiz, cero directas en suite y
  una vez por shim.
- `validate_source_registration.py`: PASS, exit 0.
- `CanonicalCatalog`: 5,995 casos, 30 componentes y 682 contratos.

Logs persistentes:

- `A:\temp\m02b-build-nsc.log`;
- `A:\temp\r01h-suite-three-targets-build.log`;
- `A:\temp\r01h-host-parser-40-plus-smokes.log`;
- `A:\temp\r01h-function-size.log`;
- `A:\temp\r01h-parser-registration-audit.log`;
- `A:\temp\r01h-validate-source-registration.log`;
- `A:\temp\r01h-canonical-catalog.log`.

No se ejecuto baseline ni suite completa.

## Criterio de cierre

R01H queda listo para integracion cuando el indice contiene exclusivamente esta extraccion,
declaraciones, CMake, shim y expediente. El cambio concurrente en
`tests/unit/ProjectMethodBodyModelTests.cpp` permanece fuera del indice y del commit.
