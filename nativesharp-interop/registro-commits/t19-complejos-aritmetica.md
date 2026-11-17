# T19-039: aritmética, comparación y conversiones implícitas de C sobre `_Complex` en NativeSharp

Fixture nuevo: `pruebas-tanda-19-complejos-aritmetica/validar.py`, con y sin `--dll`. Sondas de desarrollo en
`A:/temp/cx39` (operadores, conversiones y plantilla precompilada).

## Defectos

Desde T19-001, un `_Complex T` llega a NativeSharp como el struct C sintético `__complex_T { T real; T imag; }`, y se
pasa como record. Sobre él NativeSharp no tenía operaciones:

- `a * b`, `a + 2.0` y `-a` daban «binary operator requires compatible numeric operands» o «unary numeric operator
  requires numeric operand»;
- `z *= w` y `z += 1.0` daban «compound assignment requires compatible numeric operands». NativeSharp no admite
  `op=` con operadores definidos por el usuario, tampoco en sus propios structs;
- `__complex_double z = 4.0;`, `code(2.5)` con un parámetro `double _Complex` y `z = cf` (de `float _Complex`) no
  convertían: «cannot initialize local 'z' of type '…__complex_double' from 'builtin:double'» y «no viable
  overload».

## Decisiones

- **Clang calcula cada operación.** En una unidad C con valores `_Complex T`, la sesión agrega a la fuente, por cada
  portador:
  - el struct, con los mismos campos que le daría la síntesis, así que la unidad analizada otra vez conserva el mismo
    tipo C;
  - funciones C `static inline` que convierten el portador a `T _Complex`, aplican el operador C y convierten el
    resultado de vuelta (`ClangSession::declareCComplexOperators`).

  Así la multiplicación y la división siguen el Anexo G con `__muldc3`, `__mulsc3`, `__divdc3` y `__divsc3`, igual
  que en Clang, y la aritmética de complejos enteros también es la de Clang. Si Clang rechaza el código, la sesión
  queda como estaba.
- **Operadores**: `+`, `-`, `*` y `/` entre complejos, entre un complejo y su real T y entre su real T y un complejo;
  `-` y `+` unarios; `==` y `!=` con las mismas combinaciones, que devuelven `_Bool`.
  - Cada función se informa como el operador C++ que representa (`operator*`), así que NativeSharp la demanda
    desde una expresión como a un operador libre C++ (T10-016).
  - NativeSharp la importa como operador de extensión `op_Multiply`, y la función enlaza por su nombre C, como un
    envoltorio de macro (`cMacroWrapper`).
  - Un real de otro tipo usa la conversión numérica implícita de NativeSharp al real T (`a * 2` con `a` un
    `double _Complex`).
  - Las funciones auxiliares de conversión (`__nsc_cx_*`) no se importan.
- **Conversiones implícitas**: de cada tipo real estándar y de cada otro portador a un portador, con `(T _Complex)v`.
  - Todas comparten el nombre `__nsc_complex_cv`, que cada unidad demanda una vez, porque no tienen nombre escrito.
  - Una conversión nueva, `CComplexValue`, la resuelve el clasificador del cuerpo y el de HIR
    (`src/Frontend/CComplexConversion.h`). El HIR la baja como una llamada a la función C.
  - NativeSharp la admite cuando admite implícitamente la conversión de los tipos elemento, como con los reales:
    `float _Complex` a `double _Complex` sí, y `int` a `double _Complex` sí. `double _Complex` a `float _Complex` y
    un complejo a un real necesitan conversión explícita, igual que `double` a `float` en NativeSharp. Clang los
    acepta en C; es la regla de conversiones de NativeSharp, no un rechazo de Clang.
  - Con las conversiones, una operación mixta (`cf * cd`) usa el operador del tipo más ancho, como las conversiones
    aritméticas usuales de C.
- **Asignaciones compuestas**: `+=`, `-=`, `*=` y `/=` sobre un portador. Con un lado derecho complejo la operación
  es entre complejos, y con uno real, entre el complejo y su tipo real, como las dan las conversiones aritméticas
  usuales de C.
  - Las calcula una función C `__nsc_complex_as_<Op>(struct __complex_T* a, b)` que hace `*a = *a op b`, así que el
    destino se evalúa una sola vez, por su dirección, como en C (`a[f()] *= w`).
  - El modelo del cuerpo la elige y registra la llamada sobre la asignación. HIR la baja como una llamada con
    `&destino` y el lado derecho.
  - Como con los reales en C# y NativeSharp, el lado derecho debe convertir implícitamente al destino. `cf += cd`
    se rechaza; Clang lo acepta en C.
  - Solo cambia la asignación compuesta sobre los portadores `_Complex`. La de los structs NativeSharp sigue igual.
- **Emisión**: cada expresión con operadores puede usar una de estas funciones, así que se importan todas, pero
  solo se emiten las que el programa llama (`appendCStaticInlineObjects` recorre el MIR). Si no, un programa con
  una multiplicación de enteros necesitaría `__muldc3`. Lo detectó `pruebas-tanda-19-complejos` (T19-001), que no
  enlaza compiler-rt.
- **Plantilla precompilada**: una referencia C con valores `_Complex` viaja en el catálogo con su fuente
  preprocesada, como las macros de T19-003 (`macroSources`, con la marca `_Complex` en la lista de nombres).
  - La sesión de macros del consumidor agrega los mismos operadores y conversiones. Las plantillas publicadas los
    usan con y sin encabezado, con los records del catálogo.
  - El contrato NSMeta no cambia.
- **Enlace**: como en Clang, el programa que multiplica o divide complejos de punto flotante enlaza
  `clang_rt.builtins-x86_64.lib`. NSC no lo agrega por su cuenta; el fixture lo pasa con `--link-lib`, igual que al
  control de Clang.

No se modificó Clang ni NSLink.

## Qué funciona, como en Clang

En C, con `double`, `float`, `int` y `long long _Complex`, en estática y DLL O0–O3, directo y desde una plantilla
precompilada con y sin encabezado:

- las cuatro operaciones entre complejos, con un real a cada lado, unarias, `==` y `!=`;
- multiplicación y división con infinitos, NaN, cero y desbordamiento (`(inf + NaN i) * i`, `(1 + i) / 0`,
  `1e308 * 1e308`), con la misma clasificación de cada parte que en Clang;
- conversiones implícitas de `double`, `int` y `float` a complejos en argumentos, inicializaciones y asignaciones, y
  de `float _Complex` a `double _Complex`, también dentro de `a + f`;
- `*=`, `+=`, `-=` y `/=` con lado derecho complejo, real o entero, sobre un local y sobre una global C, también
  `a += f` con `f` de tipo `float _Complex`.

Resultado del control: 690695. En el IR de NSC hay 57 llamadas a los operadores y conversiones C.

Rechazos genuinos, también en Clang: `<` y `%` entre complejos.

## Evidencia

| Fixture | Estática | DLL |
|---|---|---|
| `pruebas-tanda-19-complejos-aritmetica` | `A:/temp/nsc-t19-complejos-4be23zn0` | `A:/temp/nsc-t19-complejos-cl8mvkdq` |

Validación puntual, por decisión del usuario:

- `pruebas-tanda-19-complejos-aritmetica`: pasa en estática (arriba) y en DLL (`logs/regresion-dll-d65.log`).
- `pruebas-tanda-19-complejos` (T19-001), tocado por el cambio: pasa en estática
  (`A:/temp/nsc-t19-complejos-ieslkrh0`) y en DLL (`A:/temp/nsc-t19-complejos-mkgxck_9`). En una corrida intermedia falló por la
  emisión de las funciones no usadas, ya corregida (ver Decisiones).
- `pruebas-tanda-19-atomicas-punteros` (T19-038) en DLL, por el cambio en las conversiones: pasa.
- Las tandas 18 y 19 completas no se corrieron: quedan para la regresión global, por decisión del usuario.

La regresión global queda para el final.

## Límites

- Un complejo a real, y un complejo a otro más estrecho, necesitan conversión explícita, que NativeSharp todavía no
  ofrece sobre el portador (se leen `real` e `imag`). Es la regla de NativeSharp para reales; Clang los convierte
  en C.
- Las operaciones y conversiones son de C. En un encabezado C++, `_Complex` es una extensión de Clang y su portador
  es un record C++ trivial sin operadores.
