# T13-002, T13-005, T13-006, T13-010, T13-012, T13-013, T13-014, T13-016 y T13-017: plantillas C++ escritas en NativeSharp

## Defecto

En la [auditoría inicial](../TANDA-13-AUDITORIA-INICIAL.md), escribir una especialización desde NativeSharp
(`A.Box<int> b;`, `A.twice<int>(3)`) daba `unknown identifier`. Tras el
[cambio 1](t13-especializaciones.md) solo se importaban las especializaciones que el encabezado nombra.

Causa en el código:

- el analizador decide si `A.Box<int> b` es una declaración con los nombres de tipo conocidos, y ninguna
  especialización que el encabezado no nombra es un tipo importado;
- la sesión de Clang no tiene una operación para instanciar una plantilla con argumentos dados;
- `collectFreeFunctions` salta las plantillas de función y no recorre sus especializaciones.

## Diseño

- **Interfaces de Clang** (T13-002): `Sema::CheckTemplateIdType` forma el tipo de una especialización de
  clase con los argumentos (conversión, argumentos por defecto, concepts) y `Sema::RequireCompleteType` la
  instancia. `Sema::DeduceTemplateArguments` con los argumentos explícitos completa los de una plantilla de
  función, paquetes incluidos, y comprueba sus restricciones; `InstantiateFunctionDefinition` y
  `PerformPendingInstantiations` instancian su definición. NSC no reimplementa sustitución, deducción,
  selección de especializaciones ni concepts.
- **Uso escrito en NativeSharp** (T13-006, T13-010): antes de analizar, NSC busca en los tokens de cada
  unidad un nombre calificado de una plantilla que declara una biblioteca C/C++ del proyecto
  (`T13.Box`, `T13.twice`) seguido de `<…>`. Cada argumento es un tipo básico NativeSharp (con su tipo
  C++: `int`, `long` es `long long`, `nint` es `long long`…; `char` no tiene equivalente y no se acepta), un tipo C++ importado (record, enum o alias
  de record, por su identidad de Clang), una especialización anidada, cualquiera con `*`, una constante
  entera (también negativa) o bool, o una plantilla de clase sin argumentos. Las sesiones del proyecto
  que declaran la plantilla la instancian al final de su unidad de traducción, como lo haría una unidad
  C++ que la nombra ahí, y los tokens desde el nombre de la plantilla hasta el `>` (también la mitad de
  un `>>`) pasan a ser un identificador con el nombre de la especialización. Una especialización de clase
  nueva se importa como las que nombra el encabezado: los tipos importados se recalculan.
- **Nombres**: una especialización de clase se llama como en el cambio 1 (plantilla, `__t` y un hash de su
  tipo canónico). Una de función se llama con su plantilla, `__f` y un hash del nombre calificado y de los
  argumentos escritos; las plantillas sobrecargadas con esos argumentos comparten el nombre y la
  resolución de sobrecarga de NativeSharp elige entre sus especializaciones por los argumentos de la
  llamada (`pick<int>(4, 9)` y `pick<int>(8)`). Solo se importan las especializaciones de función pedidas.
- **Parámetros** (T13-012, T13-013, T13-014): parámetros no tipo enteros y bool (con valores negativos),
  parámetros plantilla de plantilla (`Holder<T13.Box>`) y paquetes con argumentos explícitos
  (`count<int, int, int>`).
- **Diagnósticos** (T13-016, T13-017): un rechazo de Clang se informa en el uso escrito, con el mensaje
  `Clang cannot instantiate the C++ template '…' with these arguments` y, como notas, los diagnósticos de
  Clang (concept no satisfecho con su explicación, `static_assert`, argumentos de más) o el motivo de la
  deducción. Un argumento cuyo tipo no declara una sesión no la toca.
- La caché del corpus de análisis no se usa con sesiones C/C++, porque los tokens dependen de ellas, y la
  copia de observación de la sintaxis parte de los tokens ya reescritos.

## Cambio

- `CppInteropSession.h`: `CppTemplateArgument`, `CppTemplateInstantiation`, `CppTemplateName`,
  `templatesSnapshot` e `instantiateTemplate`.
- `ClangSession.cpp`: `templatesSnapshot`, `instantiateTemplate`, `findTagByUsr`,
  `builtinTemplateArgumentType`, `cppFunctionSpecializationName`; `collectFreeFunctions` importa las
  especializaciones de función pedidas con su nombre; `instantiateNamedSpecializations` acepta una semilla
  para completar los tipos que nombra una especialización nueva.
- `ProjectCompilation.h/.cpp`: `applyCppTemplateUses` (con `CppTemplateUseRewriter`) al empezar
  `parseSyntaxUnits`; `refreshCppImportedTypes`, extraído de `compileSyntaxCore`; los nombres reescritos
  viven en `cppTemplateLexemes_`; `cloneSyntaxForObservation` copia los tokens reescritos.

No se modificó Clang ni NSLink. El catálogo NSMeta no cambia de formato.

## Evidencia

Fixture nuevo `pruebas-tanda-13-plantillas-ns/validar.py`, con y sin `--dll`. El programa NativeSharp
escribe `T13.Box<int>`, `T13.Box<int*>` (parcial), `T13.Box<bool>` (explícita), `T13.Arr<3>`,
`T13.Tag<-5, false>`, `T13.Pair<short, double>`, `T13.Wrap<T13.Point>`, `T13.Wrap<T13.Box<int>>`,
`T13.Wrap<T13.Color>`, `T13.Wrap<T13.Point*>`, `T13.Tiny<short>` (concept), `T13.Holder<T13.Box>`, y
llama a `T13.twice<int>`, `T13.twice<double>`, `T13.convert<int, double>`, `T13.scaled<4>`,
`T13.count<int, int, int>`, las dos `T13.pick<int>` y a una función de la biblioteca que recibe
`const Box<int>*`. Resultado 292 frente al control C++ de Clang 23.1.0, de O0 a O3, con las 17 llamadas
C++ de NSC iguales a las de Clang.

Rechazos con el diagnóstico de Clang, cada uno con su contraparte válida: `Tiny<double>` (concept
`Small`: `because 'sizeof(double) <= 4' (8 <= 4) evaluated to false`), `Only<double>` (`static_assert`),
`Box<int, int>` (`too many template arguments`) y `scaled<T13.Point>` (`invalid explicit template
arguments`).

| Variante | Resultado | Evidencia |
|---|---|---|
| Estática | pasa (O0–O3) | `A:/temp/nsc-t13-plantillas-ns-0uw2w3pv` |
| DLL | pasa (O0–O3) | `A:/temp/nsc-t13-plantillas-ns-lyfpeqwn` |

El fixture del cambio 1 sigue pasando (`A:/temp/nsc-t13-especializaciones-ox0qtbt2`), y la sonda de la
auditoría pasa 9 de 12 casos (`A:/temp/nsc-t13-sonda-3pi57au0`); quedan la llamada con argumentos
deducidos, las variables `constexpr` y el struct NativeSharp como argumento, de los cambios siguientes.

Regresión: en la validación agrupada de la tanda (`TANDA-13-VALIDACION-AGRUPADA.md`).

## Límites

- La llamada a una plantilla de función sin argumentos explícitos (`A.twice(3)`) necesita deducir con los
  tipos de los argumentos de NativeSharp; queda abierta en T13-005.
- El uso se reconoce por el nombre calificado completo de la plantilla; un nombre importado con `using`
  no se reescribe.
- Una plantilla miembro o anidada en un record no se escribe desde NativeSharp.
- Si la misma especialización de función se pide con dos listas de argumentos distintas (una con un
  argumento por defecto explícito), solo la primera la nombra.
