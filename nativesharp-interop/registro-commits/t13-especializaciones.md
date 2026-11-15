# T13-003, T13-004, T13-010, T13-011, T13-012, T13-015, T13-018 y T13-023: especializaciones de clase nombradas en el encabezado

## Defecto

En la [auditoría inicial](../TANDA-13-AUDITORIA-INICIAL.md), un alias a una especialización
(`using IntBox = Box<int>`) daba `C/C++ type alias cannot yet be represented without losing type
information`, también con un parámetro no tipo y con una instanciación explícita en la biblioteca.

Causa en el código (`ClangSession.cpp`):

- un record dentro de una especialización tiene `specializationContext` y no recibe identidad
  NativeSharp; la especialización misma también, porque la marca incluye al propio record;
- `collectRecords` recorre las declaraciones de cada contexto, y una especialización implícita no está
  entre ellas: vive en `ClassTemplateDecl::specializations()`;
- nombrar `Box<int>` en un alias o en una firma no exige el tipo completo, así que Clang no lo instancia;
- `emitCppInlineIR` solo emite funciones `TK_NonTemplate`, y un miembro de una especialización implícita
  no tiene cuerpo hasta que Sema lo instancia;
- `findComparableRecords` (operaciones de valor, vida implícita) excluye las especializaciones.

## Diseño

- **Ubicación de la definición** (T13-003): la plantilla y sus especializaciones son las que Clang ve en
  la unidad de traducción del encabezado, con la configuración de la sesión.
- **Instanciación** (T13-010, T13-015): al abrir la sesión, cada especialización que el encabezado nombra
  (un alias, la firma de una función o método, un campo, una variable, y lo que estos nombran a su vez) se
  completa con Sema (`isCompleteType`), como lo haría una unidad C++ que la usa. Clang elige la plantilla
  primaria, una especialización parcial o una explícita y comprueba las restricciones. Una instanciación
  que Clang rechaza no es un error del encabezado: sus diagnósticos se suprimen y el tipo queda
  incompleto, sin importar.
- **Identidad** (T13-004, T13-018): una especialización declarada en un namespace es un record importado
  cuyo nombre NativeSharp es el de su plantilla seguido de `__t` y un hash del tipo C++ canónico, el mismo
  en toda sesión que ve la misma especialización. Su nombre calificado es el que Clang da a sus miembros
  (`T13::Box<int>`), así los métodos se reconocen por su record. NativeSharp la nombra por el alias.
- **Miembros** (T13-011, T13-012): campos, métodos, constructores y destructor se importan como los de
  cualquier record. Un miembro de una especialización tiene enlace vago: Clang instancia su definición
  (`InstantiateFunctionDefinition` y las instanciaciones pendientes) y la emite con `weak_odr`. Con una
  declaración de instanciación explícita (`extern template`) la definición es de la biblioteca.

## Cambio

`ClangSession.cpp`:

- `cppSpecializationName` y `cppSpecializationQualifiedName`: nombre e identidad;
- `instantiateNamedSpecializations`, al terminar `initialize`;
- `describeType` y `collectRecords` dan identidad a la especialización y la recogen de su plantilla;
- `findComparableRecords` encuentra una especialización por su nombre calificado;
- `emitCppInlineIR` recorre las especializaciones de clase y de función y emite los miembros instanciados.

No se modificó Clang ni NSLink. El catálogo NSMeta no cambia de formato.

## Evidencia

Fixture nuevo `pruebas-tanda-13-especializaciones/validar.py`, con y sin `--dll`. En `namespace T13`:

- `Box<T>` con métodos en la clase y uno fuera de ella, `Box<char>` especialización explícita y
  `Box<T*>` parcial;
- `Arr<int N>` con un arreglo de `N` enteros;
- `Counter<T>` con constructor y destructor que registran en un `int*`;
- `Acc<T>` con `extern template struct Acc<long long>` en el encabezado y la instanciación explícita en
  la biblioteca (exportada en la DLL);
- alias a `Box<int>`, `Box<double>`, `Box<char>`, `Box<int*>`, `Arr<4>`, `Counter<int>`,
  `Acc<long long>`, y `Box<int>` como campo de `Holder`.

El consumidor usa cada especialización: campos, métodos de la plantilla primaria, de la especialización
explícita y de la parcial, el arreglo, la instanciación explícita de la biblioteca, construcción y
destrucción de `Counter<int>` y el campo de `Holder`. Resultado 1083 frente al control C++ de Clang
23.1.0, de O0 a O3, con las 14 llamadas C++ de NSC iguales a las de Clang.

Control negativo: `Only<double>` con un `static_assert` que falla no se importa y el alias se rechaza
(`C/C++ record alias cannot yet be resolved safely: 'T13.Bad'`); la contraparte `Only<int>` pasa.

| Variante | Resultado | Evidencia |
|---|---|---|
| Estática | pasa (O0–O3) | `A:/temp/nsc-t13-especializaciones-k47qpw5x` |
| DLL | pasa (O0–O3) | `A:/temp/nsc-t13-especializaciones-owf33qrn` |

Regresión: en la validación agrupada de la tanda (`TANDA-13-VALIDACION-AGRUPADA.md`).

## Límites

- Una especialización dentro de un record (plantilla miembro o anidada) no tiene identidad.
- Una especialización que el encabezado no nombra se pide desde NativeSharp en el cambio siguiente.
- Los diagnósticos de una instanciación rechazada por el encabezado se suprimen; los de una pedida desde
  NativeSharp se conservan (cambio siguiente).
