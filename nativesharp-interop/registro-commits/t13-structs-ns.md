# T13-007, T13-008, T13-009, T13-024 y T13-025: structs NativeSharp como argumentos de plantillas C++

## Defecto

En la [auditoría inicial](../TANDA-13-AUDITORIA-INICIAL.md), `A.Box<P> b;` con `P` un struct NativeSharp daba
`unknown identifier`. Tras el [cambio 2](t13-plantillas-ns.md), un argumento solo podía ser un tipo C++.

Causa en el código:

- Clang no conoce los tipos NativeSharp: una plantilla C++ solo se instancia con tipos de su unidad de
  traducción;
- las proyecciones de campos y firmas (`CppSemanticRecord.cpp`, `CppSemanticFunction.cpp`) y el modelo de
  miembros solo aceptan como record un tipo C/C++ importado.

## Diseño

- **Representación** (T13-007): un struct NativeSharp dado como argumento se refleja en la sesión con un
  struct C++ con los mismos campos de instancia, en orden, en el namespace
  `__nsc::<módulo>::<namespace NativeSharp>`. Clang calcula su layout e instancia la plantilla con él. El
  espejo no se importa como tipo C++: una firma o un campo C++ que lo nombra corresponde al propio struct
  NativeSharp (`projectCppRecordTypeIdentity`), así que `b.v` es un `App.P` y `b.get()` devuelve un `App.P`.
  Los nombres decorados llevan ese namespace, así que no chocan con un tipo C++ del mismo nombre.
- **Layout**: al construir el modelo de layout, el tamaño, la alineación y los offsets de cada struct
  reflejado se comparan con los que Clang dio a su espejo; si difieren, la compilación se rechaza.
- **Structs triviales y no triviales** (T13-008): los structs se leen de los tokens antes de analizar. Solo es
  argumento un struct cuyos campos de instancia son tipos básicos, punteros u otros structs así, sin
  destructor, propiedades ni tipos anidados (un struct trivialmente copiable, como un struct C). Un struct
  con destructor, con una cadena u otro campo que posee memoria, o con una propiedad se rechaza con el
  motivo: su vida la gestiona NativeSharp y C++ lo copiaría bit a bit.
- **Clases** (T13-009): una clase NativeSharp es un tipo de referencia que gestiona NativeSharp; como
  argumento de plantilla se rechaza con su diagnóstico (`is a reference type; a C++ template argument needs a
  NativeSharp struct`).

## Cambio

- `CppInteropSession.h`: `CppNativeStruct`, `CppNativeStructField`, `CppNativeStructLayout`,
  `cppNativeStructNamespace` y el argumento `NativeStruct`.
- `ClangSession.cpp`: `nativeStructMirror` declara el espejo con `NamespaceDecl`, `CXXRecordDecl` y
  `FieldDecl` y devuelve su layout; `collectRecords` salta `__nsc`.
- `ProjectCompilation.h/.cpp`: `scanNativeTypes` y la resolución de structs en `CppTemplateUseRewriter`;
  las claves y layouts de los structs reflejados.
- `ProjectCompilationSnapshots.cpp`: la comparación de layouts en `buildObjectLayoutModel`.
- `ProjectNameLookup.h/.cpp`: `setCppNativeMirrorTypes`, `isCppNativeMirrorType`.
- `CppSemanticRecord.cpp`, `CppSemanticFunction.cpp`, `ProjectMemberModel.cpp`: un espejo registrado es un
  tipo válido en campos, parámetros y retornos.

No se modificó Clang ni NSLink. El catálogo NSMeta no cambia de formato.

## Evidencia

Fixture nuevo `pruebas-tanda-13-structs-ns/validar.py`, con y sin `--dll`. Con `App.P { int x; int y; }`,
`App.Q { P a; double d; }` y `App.T3 { int a; int b; int c; }`: `T13.Box<App.P>` (campo, `get` por valor,
`set` por valor, `sizeof(T)`), `T13.PtrBox<App.P>` (campo puntero), `T13.sumXY<App.P>(&p)`,
`T13.makeT<App.P>(5, 6)` (retorno por valor), `T13.twiceX<App.P>(p)` (parámetro por valor),
`T13.Box<App.Q>` (struct anidado con un double), `T13.makeABC<App.T3>` y `T13.sumABC<App.T3>` (12 bytes, en
memoria) y `sizeof(App.P)` igual al `sizeof(T)` de C++. El control C++ declara los mismos structs en
`__nsc::T13App::App`, así que los nombres decorados coinciden. Resultado 80 frente al control de Clang
23.1.0, de O0 a O3, con las 11 llamadas C++ de NSC iguales a las de Clang.

Rechazos, con la contraparte `T13.Box<App.P>`: una clase (`is a reference type`), un struct con destructor
(`it has a destructor`) y uno con una cadena (`it has a string field, which owns its storage`).

| Variante | Resultado | Evidencia |
|---|---|---|
| Estática | pasa (O0–O3) | `A:/temp/nsc-t13-structs-ns-egvqatjt` |
| DLL | pasa (O0–O3) | `A:/temp/nsc-t13-structs-ns-xqv060uz` |

Los fixtures de los cambios anteriores siguen pasando (`A:/temp/nsc-t13-deduccion-z30jca5y`,
`A:/temp/nsc-t13-plantillas-ns-3f6qbotf`, `A:/temp/nsc-t13-especializaciones-3kizwhsu`), y la sonda de la
auditoría pasa sus 12 casos (`A:/temp/nsc-t13-sonda-j950r20o`).

Regresión: en la validación agrupada de la tanda (`TANDA-13-VALIDACION-AGRUPADA.md`).

## Límites

- Un struct NativeSharp con un campo de tipo C++ importado, un arreglo fijo o un enum no se refleja todavía.
- Un struct genérico NativeSharp no es argumento.
- La deducción desde una llamada (cambio 3b) no considera un argumento de tipo struct NativeSharp; se
  escribe con argumentos explícitos.
