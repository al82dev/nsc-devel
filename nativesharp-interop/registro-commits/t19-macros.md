# T19-003: macros C y C++ de función y macros que dan cadenas, punteros, records o tipos

Fixture nuevo, con y sin `--dll`: `pruebas-tanda-19-macros/validar.py`, para C y C++.

Sondas de desarrollo:

- `tools/t19c/d14/sonda-t19-003.py` (`logs/sonda3.log`);
- `tools/t19c/d14/sonda-limites.py` (`logs/sonda3-limites.log`).

## Defectos

Confirmados con la sonda (`logs/sonda3.log`, antes del cambio):

- Una llamada a una macro de función se rechazaba con «C/C++ macro 'X' is a function-like macro: NativeSharp does
  not expand C/C++ macros» (T14-003), aunque Clang la acepta.
  - Es la forma de la API de zlib (`deflateInit`, `inflateInit`).
- Una macro de objeto cuyo valor no es un escalar (una cadena, un puntero, un record) o que nombra un tipo se
  rechazaba con «does not expand to a scalar constant that NativeSharp can import» (T14-002).

## Decisiones

- **Macro de función**: cada llamada desde NativeSharp la expande Clang dentro de un envoltorio cuyos parámetros
  tienen los tipos de los argumentos de esa llamada.
  - En C es `static inline __typeof__(M(...)) __nsc_macro_M_k(T0 __nsc_a0, ...) { return M(...); }`.
  - En C++ es `inline auto` en el namespace global.
  - Si la expansión no es una expresión (`do { ... } while (0)`), el envoltorio devuelve `void`.
  - Un argumento `ref x` es `T*` en C, y se expande como `(*p)`; en C++ es `T&`. Así la macro puede asignarlo, y
    cada argumento se evalúa una sola vez, como en una llamada.
  - El envoltorio se importa con el nombre de la macro; un mismo nombre con otros tipos es otro envoltorio.
- **Macro de objeto** no escalar:
  - si su expansión es un inicializador constante de tipo puntero, record o complejo, es una constante con ese
    tipo, importada con el nombre de la macro;
    - en C, `static const __auto_type __nsc_macro_value_X = (X);`, y una cadena decae a `char*`;
    - en C++, `inline const auto`;
  - si nombra un tipo, `typedef X __nsc_macro_type_X;`, importado como alias `X`.
- **Plantilla precompilada**: una biblioteca no analiza el cuerpo de sus plantillas genéricas, y el tipo de los
  argumentos solo se conoce al instanciarla.
  - Por eso el catálogo C del productor lleva, por cada referencia cuyas macros de función llaman sus plantillas
    publicadas, la fuente de esa referencia preprocesada por Clang con sus definiciones de macros (`-E -dD`).
    Es un bloque opcional al final del catálogo (`macroSources`), escrito solo si hay alguno; el contrato de
    NSMeta no cambia de versión.
  - El consumidor abre sobre esa fuente una sesión de Clang solo para macros (`macroExpansionOnly`), propia del
    proyecto precompilado. Informa solo las macros de función y sus envoltorios; los records de los envoltorios son
    los del catálogo, por identidad de Clang.
  - Así la macro se expande también sin el encabezado.
- **Llamadas anidadas** (`SQ(HALF(3))`): la deducción corre en rondas, hasta 8. La llamada exterior se deduce
  cuando la interior ya tiene tipo, y una llamada ya deducida no se deduce otra vez. Vale también para las
  plantillas de función C++ (T13-005).

## Diseño

- `ClangSession.cpp`:
  - `expandCFunctionMacroCall` arma el envoltorio y analiza otra vez `source_ + envoltorio`.
    - Si Clang no da errores, la sesión adopta esa unidad. `ast_` y `source_` son `mutable`, y la unidad nueva
      vuelve a informar a `consumer_` (también `mutable`).
    - En C++ repite además las especializaciones nombradas y los `instantiateTemplate` que pidió NativeSharp
      (`templateUses_`).
  - `declareCMacroValuesAndTypes` hace lo mismo con los valores y los tipos, en C y C++. Corre al abrir la
    sesión, antes de cualquier captura o pedido de plantilla.
    - Un typedef con un error en su línea, como el `int` implícito de `typedef __declspec(dllimport) T;`, no es un
      tipo, aunque Clang lo deje válido. Antes de esa comprobación, la macro `API` de un encabezado DLL hacía
      fallar el análisis y no se importaba ningún valor ni tipo.
  - Una macro que convierte un argumento en cadena (`#x`, también a través de otra macro) o lo pega a otro token
    (`a##b`) se rechaza con su motivo: escribiría el parámetro del envoltorio, no el argumento de la llamada.
  - `preprocessedSourceWithMacros` da el `-E -dD` de la entrada original (`ClangSessionAction.cpp`,
    `preprocessClangSessionSource`).
- `CppSemanticRecord.cpp` captura las fuentes de macros del productor, y `CppCRecordMetadataCodec.cpp` las
  codifica (`MacroSource`).
- `PrecompiledModuleConfiguration.cpp` abre las sesiones de macros del consumidor, cuyo dueño es la metadata del
  proyecto precompilado; `ProjectCompilation::appendCppProjectSessions` las agrega.
- `CppSemanticFunctionGroups.cpp` da a los envoltorios de esas sesiones los records C y C++ importados del
  catálogo del proyecto.
- `CppSemanticInventory.cpp` lleva también en el catálogo un alias C++ de un escalar o de un puntero a un escalar
  o a un record con nombre (el de una macro de tipo).
  - Antes viajaban solo los de records, enums y punteros a función.
- `ProjectCompilationSnapshots.cpp`, `CallAnalyzer.cpp` y `ProjectCompilation.cpp` registran la llamada a una macro
  de función como una deducción de plantilla (`functionMacro`), con las rondas.
- `DriverArtifactGeneration.cpp` y `CppSemanticInventory.cpp` enlazan y emiten cada envoltorio y cada valor por
  su nombre C (`cMacroWrapper`, `cMacroStorage`).

No se modificó Clang ni NSLink.

## Qué funciona, como en Clang

En C y C++:

- macros de función que son expresiones o sentencias;
- macros con argumentos `ref` que la macro asigna (`INC`, `SWAPI`), variádicas (`__VA_ARGS__`) y sin argumentos;
- macros anidadas, también con tipos distintos (`SQ(HALF(3))`);
- macros que usan otras macros y valores (`init`, como `deflateInit` de zlib);
- macros que leen un record por valor (`PSUM`);
- en C++, macros que llaman funciones de un namespace y una función inline, y que devuelven un record por valor
  (`MAKEP`);
- macros de objeto que dan una cadena, un puntero (también a una cadena) o un record, y macros que nombran un tipo
  (`REAL`, `IPTR`).

Todo O0–O3, en biblioteca estática y DLL, directo y desde una plantilla precompilada con y sin encabezado.

## Evidencia

| Fixture | Resultado | Estática | DLL |
|---|---|---|---|
| `pruebas-tanda-19-macros` (C) | 799583 | `A:/temp/nsc-t19-macros-ec2lq73q` | `A:/temp/nsc-t19-macros-vn3jyw7n` |
| `pruebas-tanda-19-macros` (C++) | 651789 | `A:/temp/nsc-t19-macros-ec2lq73q` | `A:/temp/nsc-t19-macros-vn3jyw7n` |

Rechazos genuinos, también en Clang, en C y C++:

- una macro llamada con un argumento que su expansión no acepta (`FIELD(3)`: «member reference base type»);
- una macro llamada con un número de argumentos que no toma (`SQ(1, 2)`).

`pruebas-tanda-14-macros`: el negativo obsoleto `T14_SQUARE(3)` (macro de función rechazada) pasa a ser una
comparación positiva con Clang (`T14_SQUARE(7) + 1`, 50). Queda un rechazo genuino, `T14_FIELD(3)`, que Clang también
rechaza (`A:/temp/nsc-t14-macros-mrm4fu1z`, y en DLL `A:/temp/nsc-t14-macros-dugzuvhj`).

Regresión con este cambio:

- Tandas 10 a 19 en estática: 76 de 76 (`logs/regresion-t1*-d55.log`).
- Tanda 9 en estática: pasan 1093 de 1099 y se omiten 2 que exigen parámetros (`logs/regresion-t09-d55.log`).
  - Fallan, como en la línea base, los dos `cpp-variadica-preserve-none` (bloqueo de Clang, T19-032).
  - Bajo carga fallan dos de `cpp-tamano-objeto` (T19-026); repetidos solos, pasan (`logs/regresion-d55r.log`).
- DLL de las tandas 9 a 19: pasan 72 de 75 (`logs/regresion-dll-t*-d55.log`).
  - En los tres que fallan, el control de Clang tardó unos 30 s y terminó con 0, la falla conocida del entorno:
    `cpp-tamano-objeto`, `tanda-10-namespaces` y `tanda-19-vla-parametros`.
  - Repetidos, pasan los tres (`logs/regresion-dll-d55r.log`).
- `ninja -k 0`, los nueve CTest de interop y la identidad de NSC pasan (`logs/lote-ct2.log`).
- Los tres guiones de T08 terminan con 0 (`logs/p55.log`).

## Límites

Con tarea nueva:

- T19-040: macros que convierten un argumento en cadena (`#x`) o lo pegan a otro token (`a##b`). Se rechazan con
  su motivo: un argumento NativeSharp no tiene una escritura C/C++.
- T19-041: macros de objeto cuya expansión no es una constante (`errno`, la dirección de una global `dllimport`
  de una DLL, una llamada). Siguen sin importarse, con su motivo.
- T19-042: argumentos de macro que son tipos (`SIZE(int)`) u otras formas que NativeSharp no escribe como
  expresión. NativeSharp rechaza la sintaxis.

Otras observaciones:

- La deducción de llamadas anidadas tiene un máximo de 8 rondas.
- `errno` no se pudo comparar en este equipo: el entorno de pruebas no tiene los encabezados del CRT
  (`errno.h`), y Clang tampoco compila el control.
