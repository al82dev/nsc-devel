# T11-006, T11-015, T11-016 y T11-017: constructores, copia y movimiento

## Defecto

En la [auditoría inicial](../TANDA-11-AUDITORIA-INICIAL.md):

- un constructor con un enum, un record, un puntero a record o una referencia no se importaba;
- `M(int) noexcept` junto a `M(M&&) noexcept` daba `struct has only an implicit parameterless
  constructor`;
- un constructor privado daba el mismo diagnóstico, sin mencionar el acceso;
- un destructor privado daba `experimental C++ local requires direct automatic construction and
  cleanup`.

Causa en el código:

- `projectCppSemanticLifetimeMethod` (`CppSemanticFunction.cpp`) mapeaba los parámetros sin búsqueda
  de nombres ni records, rechazaba las referencias y todo constructor no público.
- `collectCppSemanticLifetimeFamilies` descarta la familia entera de constructores de un record si uno
  no se proyecta. Así un constructor eliminado, uno de movimiento o uno privado ocultaban todos los
  demás. Es deliberado: un candidato que falta podría cambiar la resolución.
- `ProjectMemberModel.cpp`, `ProjectCompilation.cpp` y `MirNodeMachine.cpp` repetían la restricción
  a parámetros escalares o `void*`.

## Diseño

- Un constructor toma lo mismo que un método de instancia: escalares, punteros, enums y records
  triviales por valor, `T&` como `ref` y `const T&` como `in`.
- **Copia** (T11-015): un constructor de copia `T(const T&)` es un constructor con parámetro `in T`. La
  copia de un objeto C++ no trivial es explícita, `new T(in a)`, y ejecuta el constructor de Clang. La
  copia implícita (`T b = a;`) sigue rechazada, porque el valor es move-only en NativeSharp.
- **Movimiento** (T11-016): NativeSharp no tiene referencias rvalue. El constructor de movimiento
  `T(T&&)` se proyecta con un parámetro `ref T` y se llama de forma explícita, `new T(ref a)`. El
  origen sigue vivo y se destruye al salir de su ámbito, como en C++. NativeSharp, como C#, no
  sobrecarga `ref` frente a `in`: si el record tiene un constructor de copia invocable, este tiene
  prioridad y el de movimiento queda fuera. Así se importa el movimiento de los tipos move-only.
- **Eliminados** (T11-017): un constructor eliminado no es candidato y ya no oculta a los demás. Una
  copia de un tipo con copia eliminada no encuentra constructor.
- Un constructor que toma otro `T&&` no tiene argumento NativeSharp y queda fuera, sin ocultar a los
  demás.
- **Acceso** (T11-006, T11-007): un constructor o destructor protegido o privado se importa con su
  acceso. NativeSharp lo comprueba en la construcción (`matching constructor exists but is not
  accessible`). Un local exige un destructor accesible.

## Cambio

- `CppSemanticFunction.cpp` y `.h`:
  - `projectCppSemanticLifetimeMethod` recibe los records visibles y la búsqueda de nombres; mapea los
    parámetros como un método, con modo de paso, y conserva el acceso;
  - el constructor de movimiento pasa su `T&&` como la dirección del origen;
  - `collectCppSemanticLifetimeFamilies` salta los constructores eliminados y los que toman `T&&` (salvo
    el de movimiento elegido), y la firma distingue `ref` e `in`.
- `CppImportedInstanceMethod.h`: el método de vida importado lleva los modos de paso y el acceso.
- `ProjectMemberModel.cpp`: admite los nuevos parámetros, exige que uno por valor sea un enum o un
  record trivial, y crea el miembro con su acceso.
- `ProjectCompilation.cpp` y `MirNodeMachine.cpp`: aceptan los parámetros que el modelo de miembros
  admitió, y un local exige un destructor accesible.

No se modificó Clang ni NSLink. El catálogo NSMeta no cambia de formato.

## Evidencia

Fixture nuevo `pruebas-tanda-11-constructores/validar.py`, con y sin `--dll`. `struct C` en
`namespace T11` con constructores de `int`, `double`, `enum class E`, `P` por valor, `const P*`,
`const C&` (copia que suma 100), `C&&` e `(int&, int)`, y destructor que cuenta. `struct M` es
move-only: `M(int)`, `M(M&&)` (deja el origen en 0) y copia eliminada.

El consumidor construye un `C` con cada constructor. La copia usa `new T11.C(in a)` y el movimiento
de `M` usa `new T11.M(ref m)`. Al salir del bloque se destruyen los nueve objetos. De O0 a O3, el
fixture compara un control C++ compilado por Clang 23.1.0 con el consumidor NativeSharp (resultado
227). También exige las 12 declaraciones del IR de NSC con los tipos físicos de Clang. `C(C&&)` no
se proyecta porque `C(const C&)` tiene prioridad.

Controles negativos:

- `new T11.U(in a)` con `U(const U&) = delete`: `no viable constructor accepts the argument types`;
- `new T11.V(5)` con `V(int)` privado: `matching constructor exists but is not accessible`;
- un local de `W` con destructor privado: `C++ local requires an accessible destructor`.

Contrapartes del mismo encabezado: `new T11.U(1)` y `new T11.V()` pasan.

| Variante | Resultado | Evidencia |
|---|---|---|
| Estática | pasa (O0–O3) | `A:/temp/nsc-t11-constructores-42bp1fyf` |
| DLL | pasa (O0–O3) | `A:/temp/nsc-t11-constructores-vi5yp77x` |

Regresión: ver [TANDA-11-VALIDACION-AGRUPADA.md](../TANDA-11-VALIDACION-AGRUPADA.md).

## Límites

- La copia implícita (`T b = a;`) de un record no trivial, incluso trivialmente copiable, sigue
  rechazada; la copia es explícita.
- Un record con constructor de copia y de movimiento solo ofrece la copia.
- Constructores que pueden lanzar: tanda 15.
- Constructores de records con bases: tanda 12.
- Plantillas precompiladas con objetos C++ de vida no trivial: T13-001.
