# T19-005: nombres C++ por `using`, alias, referencias, anidados no públicos y enums sin nombre

Fixture: `pruebas-tanda-19-traslados/validar.py`, casos `nombres-using`, `alias-plantilla` y `enums-y-anidados`.
Cada caso compara un programa NativeSharp con un control C++ de Clang 23.1.0, O0–O3, con la biblioteca estática y,
con `--dll`, como DLL. `nombres-using` y `enums-y-anidados` repiten el programa desde una plantilla NativeSharp
precompilada, con y sin el encabezado C++ visible para el consumidor. Sonda de desarrollo:
`tools/t19c/n6/sonda-t19-005.py` (`logs/sonda5.log`).

## Qué funciona ahora, como en Clang

- `using T::x;` de una variable, una función y un tipo, también en el namespace global, y `using namespace T;`
  dentro de otro namespace o en el global (tipos, funciones y variables). Cada nombre es un alternativo de la
  misma declaración: el mismo USR y el mismo símbolo.
- Globales y miembros static referencia (`int& alias = value;`, `static int& r;`, `const P& view`): el símbolo
  guarda la dirección del referente, como lo emite Clang. NativeSharp nombra al referente con el tipo referido, y
  cada acceso carga esa dirección (`cppReferenceStorage`, hasta `staticFieldAddress`). Lectura y escritura a
  través de la referencia, también `dllimport`.
- Plantillas alias: de una plantilla de clase (`template<class X> using Alias = Box<X>;`), en otro namespace
  (`U::Box2<X> = T::Box<X>`) y de un escalar o puntero (`Same<int>`, `Ptr<int>`). Clang sustituye los argumentos
  (`CheckTemplateIdType` con el `TypeAliasTemplateDecl`); el uso se reescribe con el tipo que nombra, con sus
  namespaces.
- Records y enums anidados privados o protegidos: se importan con su acceso, así que nombrarlos fuera de su record
  da el diagnóstico de acceso de NativeSharp (`nested type 'T.Outer+Hidden' is private ...`,
  `nested type 'T.Base+Mode' is protected ...`), como Clang rechaza el mismo uso. Un enumerador de un enum privado
  por el record se rechaza con `member 'X' is not accessible`.
- Enumeradores de un enum sin `class` anidado en un record, por el record (`T.R.Green`).
- Una global de un enum de namespace sin que el programa nombre el enum (`T.current`).
- Enums sin nombre: sus enumeradores en el namespace (`T.Alpha`) o en el record (`T.R.K`), con el tipo entero del
  enum; un campo de un enum sin nombre (`s.mode`); `typedef enum { ... } Kind;` en C++ da al enum el nombre `Kind`,
  como lo decora Clang.
- Plantillas de clase y de función nombradas por `using T::Box;`, por `using namespace T;` dentro de otro namespace
  y por un alias de namespace (`Box<int>`, `U.Box<int>`, `A.Box<int>`, `thrice<int>`, `U.thrice<int>`), caso
  `plantilla-por-using`. El uso se reescribe con los namespaces de la especialización; el nombre de una
  especialización de función no depende del nombre con que se escribió.
- Un tipo anidado privado o protegido nombrado como receptor (`T10.Hidden.Secret.zero()`) da el mismo diagnóstico
  de acceso que en una declaración.

## Diseño

- `ClangSession`:
  - `addNamespaceAliasTypeNames` también trata directivas y declaraciones `using` de tipos; los nombres que crea
    llevan como identidad el USR del tipo más el nombre, para que una plantilla precompilada pueda llevarlos.
  - `addAlternateGlobalNames` da a las globales los nombres de `using` y alias de namespace; las funciones
    globales con directivas en el namespace global quitan el prefijo `::`.
  - `collectEnums` recoge enums anidados no públicos y sin nombre, con su acceso (`CppEnumDeclaration::access`).
  - `visitNamespaceTemplates` incluye las plantillas alias; `describeAliasTarget` describe el tipo que nombran.
- Catálogo precompilado, un solo contrato con bloques opcionales:
  - el byte de tipo de un record lleva en sus bits altos el acceso de un record anidado protegido (`0x40`) o
    privado (`0x80`); el conteo de enumeradores de un enum, el de un enum protegido (`0x20000000`) o privado
    (`0x10000000`); los catálogos anteriores se leen como públicos;
  - un segundo bloque opcional, después del de retorno en memoria, lleva los enumeradores de los enums sin nombre
    de cada record;
  - un enumerador de dependencia C++ lleva al final, opcional, su namespace;
  - viajan también los alias C++ de un record o enum, no solo los de puntero a función.
- `captureCDependencyGlobal` elige, entre las observaciones de una global con varios nombres, la del namespace que
  nombró el programa.

## Negativos obsoletos convertidos

- `pruebas-tanda-10-anidados`: el record privado y el enum protegido se siguen rechazando, ahora con el diagnóstico
  de acceso, como Clang; `Outer.Red` (enumerador de un enum sin `class` por el record) pasa a contraparte positiva,
  como en C++.
- `pruebas-tanda-10-using-namespace`: una directiva `using namespace` en el ámbito global da nombres globales, como
  en C++; el negativo pasa a contraparte positiva. La declaración propia que oculta la del namespace nominado se
  sigue rechazando.

## Límites

- `alias-plantilla` solo se prueba directo: el cuerpo de una plantilla NativeSharp precompilada todavía no admite
  usos de plantillas C++ (T19-016).
- Una global de un enum sin nombre con enlace externo no existe en C++: Clang la rechaza (`its type does not have
  linkage`).

## Regresión

Validación agrupada `t19c/n6/validar-vs8.cmd` (registros en `logs/`):

- Fixture de traslados completa, estática y DLL (`fx-vs8t.log`, `fx-vs8td.log`); vectores C (`vc2*.log`); nivel de
  CPU (`ncpu1.log`); tanda 8 (`t08-*-vc8.log`); `lote-ct2.log` sin errores.
- CTest dirigido (`ctest-vs8.log`, 57 pruebas): fallan `nsc.cpp-interop.session` y `nsc.stage42.delegates-events`,
  como antes.
- Tandas 9 a 19, estática (`regresion-vs8.log`): 1162 de 1169; fallan las conocidas (`cpp-variadica-preserve-none`
  ×2, T19-032; `cpp-tamano-objeto/validar-indice-expresion` bajo carga, T19-026) y los dos negativos obsoletos de la
  tanda 10 que se convirtieron. DLL (`regresion-dll-vs8d.log`): 67 de 69, los mismos dos.
- Con los negativos convertidos, el receptor anidado y las plantillas por `using`: tandas 10 y 13, estática 34 de
  34 y DLL 17 de 17 (`regresion-n7.log`, `regresion-dll-n7d.log`); `plantilla-por-using` estática y DLL
  (`fx-n7a2.log`, `fx-n7ad2.log`); fixture de traslados completa estática y DLL (`fx-n7t.log`, `fx-n7td.log`).
