# NativeSharp 1.0.1 — historial acumulativo de cambios

## Alcance de la serie

Este documento unifica la historia local comprendida entre los commits
`5a9ed611416cfce39711efc267ce246475af92dd` y
`c209ea0c3cb5db783ad3a238a228d42b3e3abe47`, en orden cronológico.

La serie resolvió diferencias reales descubiertas al portar bibliotecas nativas,
recuperó el baseline completo y estableció el procedimiento con el que se registrarán
los cambios siguientes.

| Commit | Resultado principal |
|---|---|
| `5a9ed611` | Expresiones enum, propiedades `get`, enum contra cero y unión condicional de strings |
| `4b9ac39a` | Acta retrospectiva de la unión condicional de strings |
| `3e563720` | Tipado correcto de literales positivos que requieren `ulong` |
| `50ec8a3b` | `borrowed string` temporal en métodos y constructores |
| `289b4bca` | Baseline G00 ejecutable desde consola |
| `2af29c5f` | Registro y acta del baseline certificado |
| `0a2d558c` | Política local de historia, documentación y commits bilingües |
| `c209ea0c` | Fotografía versionada de los dos árboles de pruebas después del baseline |

## 1. `5a9ed611` — correcciones de enum, constructores y strings

Título original:

```text
Fix enum constants, constructor initialization, and native string joins
```

Este commit agrupó tres correcciones ejecutables. Fue creado antes de la política que
exige una unidad independiente por cambio y título bilingüe.

### 1.1 Expresiones constantes en inicializadores enum

El compilador aceptaba operaciones bitwise con enums en expresiones ordinarias, pero
el registro temprano de miembros sólo admitía un literal entero plano. Por eso fallaba
la forma canónica:

```nativesharp
All = Red | Green | Blue | Alpha
```

Cambios principales:

- `src/Frontend/ProjectMemberModel.cpp`
  - agregó `EnumInitializerResolver`, limitado a miembros anteriores del mismo enum;
  - agregó `enumValueFromIntegerConstant`;
  - hizo que `ProjectMemberModel::collectEnumMembers` use
    `ProjectIntegerEvaluator`;
  - mantuvo la validación de rango contra el tipo subyacente.
- `tests/unit/Stage51EnumOopSupportTests.cpp`
  - verificó que `All` vale `15`;
  - añadió un rechazo para llamadas usadas como supuesto inicializador constante.
- `spec/NativeSharp_Language_Specification_1_0.md`
  - definió la expresión constante integral permitida en un miembro enum.
- `spec/NativeSharp_1_0_conformance.map`
  - enlazó implementación y evidencia con `NS-TYPE-002`.
- `mkdown/158_1_0_1_expresiones_constantes_en_inicializadores_enum.md`
  - documentó causa, implementación y prueba.

La solución reutiliza el evaluador entero tipado y no introduce reglas exclusivas para
enums. Las referencias adelantadas, llamadas y valores dinámicos continúan rechazados.

### 1.2 Asignación a auto-propiedades `get` en el constructor

Una auto-propiedad sólo lectura ya tenía backing field generado, pero una asignación
dentro de su propio constructor intentaba encontrar un setter y fallaba.

Cambios principales:

- `include/nativesharp/Frontend/ProjectMethodBodyModel.h`
  - extendió el análisis de expresiones con `allowConstructorPropertyWrite`;
  - declaró `automaticPropertyBackingField`.
- `src/Frontend/ProjectMethodBodyModel.cpp`
  - hizo que `analyzeAssignment` habilite la escritura especial sólo para `=`;
  - actualizó `analyzeIdentifier` y `analyzeMember` para resolver el backing field;
  - implementó `automaticPropertyBackingField`;
  - limitó la escritura al constructor del tipo declarante y al `this` actual.
- `src/Frontend/ProjectMemberModel.cpp`
  - permitió reconocer el getter sintético durante el escaneo temprano.
- `tests/unit/Stage38PropertiesIndexersOperatorsTests.cpp`
  - cubrió `Prop = value` y `this.Prop = value`;
  - conservó rojos desde métodos ordinarios y mediante otra instancia.
- `tests/cc1/c54_constructor_get_only_enum_zero/`
  - añadió una fixture ejecutable con la forma usada por `GamePadTriggers`.
- `mkdown/159_1_0_1_inicializacion_get_only_y_comparacion_enum_cero.md`
  - documentó la frontera exacta de la regla.

No se sintetiza un setter. La superficie pública permanece de sólo lectura después de
terminar el constructor.

### 1.3 Comparación entre enum y literal cero

La forma `(buttons & button) != 0` era rechazada aunque el cero representa válidamente
el valor nulo de cualquier enum.

Cambios principales:

- `src/Frontend/ProjectMethodBodyModel.cpp`
  - `analyzeBinary` admite únicamente `==` y `!=` entre un enum y el literal `0`.
- `src/Frontend/ProjectOopHirModel.cpp`
  - `lowerExpressionImpl` materializa la conversión del cero al enum antes de comparar;
  - evita tipos LLVM incompatibles en enums basados en `byte`, `short`, `long` o
    `ulong`.
- `tests/unit/Stage51EnumOopSupportTests.cpp`
  - cubrió ambos órdenes, enum basado en `byte` y rechazo de `enum != 1`.
- `tests/cc1/c54_constructor_get_only_enum_zero/Main.ns`
  - comprobó la forma real en `-O0` y `-O2`.
- especificación y mapa de conformidad
  - registraron la conversión estrecha bajo `NS-CONV-001`.

La regla no vuelve compatibles enums nominalmente diferentes ni enteros distintos de
cero.

### 1.4 Unión condicional entre literal y string nativo

Un condicional con una rama `string-literal` y otra `builtin:string` no encontraba un
tipo de resultado, aun cuando la conversión implícita ya existía en asignaciones,
argumentos y retornos.

Cambios principales:

- `src/Frontend/ProjectMethodBodyModel.cpp`
  - el análisis de `ConditionalExpression` elige `builtin:string` para ese par en
    cualquiera de los dos órdenes;
  - conserva la evaluación perezosa y la combinación de ownership existente.
- `tests/language/l1_conditional/Main.ns`
  - añadió ejecución de ambos órdenes.
- `tests/unit/Stage57LanguageTests.cpp`
  - mantuvo rojo el par incompatible string/entero.
- especificación y conformidad
  - documentaron la regla bajo `NS-EVAL-001`.

No se implementó un algoritmo general de tipo común de C# ni se agregó `??`.

### 1.5 Productos generados incluidos históricamente

El commit también incluyó salidas bajo `build-test-lldlinker/` y
`build-test-nslink/`: objetos, ejecutables, bibliotecas, archivos Ninja y registros de
configuración. Esos archivos no forman parte de la lógica de las cuatro correcciones.
La política añadida al final de esta serie establece que los productos reproducibles
no deben mezclarse con commits de fuente posteriores.

## 2. `4b9ac39a` — acta de la unión condicional de strings

Título:

```text
Documenta la unión condicional de strings nativos / Document native string conditional joins
```

Archivos:

- creó `mkdown/160_1_0_1_union_condicional_de_strings_nativos.md`;
- actualizó `mkdown/CONTENIDO.txt`.

El documento separó retrospectivamente la cuarta corrección de `5a9ed611`. Registró
las funciones modificadas, la decisión de mantener una unión estrecha, los casos
positivos y negativos y la diferencia entre pruebas enfocadas y baseline certificado.

## 3. `3e563720` — literales positivos que requieren `ulong`

Título:

```text
Corrige literales positivos que requieren ulong / Fix positive literals that require ulong
```

### Defecto encontrado

La primera corrida completa posterior a `5a9ed611` encontró que el literal sin sufijo
`18446744073709551615` se clasificaba como `long`. Al normalizarlo se convertía en
`-1` y un miembro de enum con base `ulong` era rechazado incorrectamente.

### Archivos y funciones

- `include/nativesharp/Frontend/IntegerLiteralTyping.h`
  - `integerLiteralTypeKey` selecciona `int`, `long` o `ulong` según la magnitud;
  - conserva `-9223372036854775808` como el mínimo directo de `long`;
  - añadió aserciones para el bit 63 positivo y `ulong.MaxValue`.
- `tests/unit/Cc1IntegerEvaluatorTests.cpp`
  - verificó tipo `builtin:ulong` y conservación de los 64 bits.
- `tests/cc1/c38_enum_ancho/Main.ns`
  - volvió a ejecutar el máximo unsigned sin modificar el caso canónico.
- `tests/unit/Cc1CorrectnessTests.cpp`
  - recuperó el diagnóstico de que no existe sucesor después de `ulong.MaxValue`.
- `mkdown/161_1_0_1_literal_positivo_ulong_y_rango_enum.md`
  - registró corrida roja, causa, solución y recertificación.
- `mkdown/158_1_0_1_expresiones_constantes_en_inicializadores_enum.md`
  - añadió el enlace al defecto de rango descubierto por el baseline.

La corrección quedó centralizada para que enums, campos `const`, `switch` y
expresiones ordinarias compartan el mismo tipo de literal.

## 4. `50ec8a3b` — préstamos temporales de string

Título:

```text
Permite préstamos temporales de string en constructores / Allow temporary string borrows in constructors
```

### Regla implementada

Un literal tiene duración de proceso y puede prestarse directamente. Un string owned
fresco puede prestarse hasta el final de la expresión completa mediante un owner
oculto. La misma regla se aplica a llamadas ordinarias, `new`, `this(...)` y
`base(...)`.

### Archivos y funciones

- `include/nativesharp/Frontend/ProjectConstructionModel.h`
  - agregó `fullExpressionBorrowArgumentOrdinals` a los candidatos;
  - conservó los ordinales seleccionados en inicializadores y construcciones.
- `include/nativesharp/Frontend/ProjectOopHirModel.h`
  - declaró `wrapFreshOwnedTemporaryBorrow`.
- `src/Frontend/ProjectConstructionModel.cpp`
  - agregó `isStringLiteralBorrow` e `isFreshOwnedStringBorrow`;
  - actualizó `bindConstructorInitializer` y `bindConstruction`;
  - distinguió préstamo temporal de transferencia de ownership.
- `src/Frontend/ProjectMethodBodyModel.cpp`
  - `analyzeCall` acepta el préstamo de literales con duración de proceso.
- `src/Frontend/ProjectOopHirModel.cpp`
  - `wrapFreshOwnedTemporaryBorrow` crea `BorrowValue`, owner oculto,
    `OwnedTemporaryBorrow` y cleanup de expresión completa;
  - `wrapCallArgumentBorrow` y las rutas de construcción reutilizan la misma regla.
- `tests/cc1/c49_temporary_string_borrow/Main.ns`
  - cubrió método, constructor, `this(...)`, literal, préstamo existente y
    concatenación owned fresca.
- `tests/unit/Stage50_7BorrowEscapeAnalysisTests.cpp`
  - mantuvo rojo el escape de un parámetro borrowed hacia un campo.
- especificación y mapa de conformidad
  - actualizaron `NS-OWN-003` con lifetime y evidencia.
- `mkdown/162_1_0_1_borrowed_string_en_constructores.md`
  - documentó implementación, invariantes y validación.

La solución no prolonga arbitrariamente un préstamo ni convierte la operación en una
transferencia. El destructor del owner oculto se ejecuta al terminar la expresión.

## 5. `289b4bca` — baseline G00 desde consola

Título:

```text
Habilita el baseline G00 desde consola / Enable the G00 baseline from the command line
```

Archivos y funciones:

- `nscbuilder.py`
  - agregó `make_baseline_plan`, compartido por GUI y consola;
  - hizo que `MainWindow._make_plan("baseline")` use esa fábrica;
  - agregó `run_baseline_cli`;
  - añadió `--baseline` y `--jobs` a `main`;
  - definió códigos `0` para verde, `1` para corrida fallida y `2` para argumentos
    inválidos.
- `mkdown/163_1_0_1_baseline_g00_desde_consola.md`
  - documentó matriz, implementación, uso, salidas y validación.

La entrada nueva usa el mismo `PipelineWorker`; no existe una segunda matriz que pueda
certificar un verde parcial o divergir de la interfaz gráfica.

## 6. `2af29c5f` — baseline certificado

Título:

```text
Registra el baseline certificado de NativeSharp 1.0.1 / Record the certified NativeSharp 1.0.1 baseline
```

Archivos:

- actualizó `_abi/gates/G00/gate.json` con la corrida certificada;
- creó `mkdown/164_baseline_certificado_1_0_1_strings_y_enums.md`;
- actualizó `mkdown/CONTENIDO.txt`.

Evidencia registrada:

```text
run_id:     20260908-210330-3a9390d3
estado:     LANDED, certified=true
duración:   682.304 s
CTest:      306/306 lld-link, 306/306 NSLink
fixtures:   148/148 lld-link, 148/148 NSLink
triage:     18/18 lld-link, 18/18 NSLink
fallos:     0
```

Compilador instalado y probado:

```text
nsc 0.9.0+build.20260909T030331Z.cbacb6c12a1e60f5
SHA-256: c80b474240e895423a1f19d877946eff89a594ab8ff5ea5053547ca0c0ae4486
```

S0 y S1 midieron 1039 fuentes y el mismo digest:

```text
9a867a52543a509237fff3ec44d1a455f3778306387421aebf4a34f0a7e266ca
```

El acta conserva también la corrida roja `20260908-204545-ec8fea99`, que permitió
descubrir y corregir el problema `ulong` antes de certificar.

## 7. `0a2d558c` — política local de historial

Título:

```text
Define la política local de historial de desarrollo / Define the local development history policy
```

Archivos:

- creó `mkdown/165_politica_local_de_historial_de_desarrollo.md`;
- actualizó `mkdown/CONTENIDO.txt`.

La política vuelve obligatorio para cada cambio coherente:

- implementación, pruebas y especificación correspondientes;
- Markdown numerado con archivos, funciones, decisiones y evidencia;
- entrada en el índice;
- verificación enfocada;
- baseline G00 para cambios ejecutables;
- commit local inmediato con título y descripción en español e inglés;
- exclusión de artefactos generados y cambios ajenos.

También define que una corrida roja se conserva, se corrige como unidad separada y se
repite por completo antes de registrar un baseline verde.

## 8. `c209ea0c` — fotografía de productos de pruebas

Título:

```text
Fix enum constants, constructor initialization, and native string semantics
```

Aunque el mensaje resume las correcciones funcionales de la serie, el diff propio de
este commit no modifica `include/`, `src/`, `tests/` fuente, `spec/` ni `mkdown/`. Su
contenido exacto son 441 productos generados:

- 226 archivos bajo `build-test-lldlinker/`;
- 215 archivos bajo `build-test-nslink/`.

Incluye objetos, ejecutables de prueba, bibliotecas, información Ninja, logs CTest,
manifiestos y Build IDs producidos por la matriz. Por tanto, este commit debe leerse
como una fotografía binaria posterior al baseline, no como una segunda implementación
de enums, strings u ownership.

Esta observación es necesaria para que la historia describa el diff real de cada
commit. La política de `0a2d558c` mantiene para cambios futuros la regla de excluir
productos reproducibles, salvo que se decida explícitamente versionar otra fotografía
de construcción.

## Resultado acumulado

Al final de la serie, las fuentes, pruebas, especificaciones, documentos y el registro
G00 están guardados en commits locales. Los productos reproducibles de los árboles
`build-test-lldlinker` y `build-test-nslink` quedaron versionados por separado en
`c209ea0c`, sin mezclarse con el commit que crea este historial acumulativo.

No quedan pendientes funcionales conocidos dentro del alcance de esta serie. Las
adaptaciones deliberadas del diseño nativo —por ejemplo no implementar `??`— no se
clasifican como defectos.

## Referencia de commit / Commit reference

**Título ES:** `Unifica el historial de cambios de NativeSharp 1.0.1`

**English title:** `Consolidate the NativeSharp 1.0.1 change history`

**Descripción ES:**

> Reúne commit por commit la implementación, archivos, funciones, decisiones, pruebas
> y evidencia del baseline de la serie de enums, strings y ownership de NativeSharp
> 1.0.1. Registra también el antecedente de artefactos generados y la política aplicada
> a los commits posteriores.

**English description:**

> Consolidate, commit by commit, the implementation, files, functions, decisions,
> tests, and baseline evidence for the NativeSharp 1.0.1 enum, string, and ownership
> series. Also record the generated-artifact precedent and the policy applied to later
> commits.
