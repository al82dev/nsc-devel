# T13-021 y T13-022: constantes C++ evaluadas por Clang

## Defecto

En la [auditoría inicial](../TANDA-13-AUDITORIA-INICIAL.md), leer `A.K` con `constexpr int K = 7;` en el
encabezado daba `unknown identifier`.

Causa en el código:

- una variable `constexpr` o `const` de un namespace tiene enlace interno en C++: no hay símbolo en
  ninguna biblioteca, y el inventario de globales (`CppSemanticInventory.cpp`) solo ofrece las de enlace
  externo y las internas de C, cuya definición emite Clang con un símbolo propio;
- `CppGlobalDeclaration` no lleva el valor de la constante;
- `emitCppInlineIR` no emite el miembro de datos `static` de una especialización implícita, cuya
  definición instancia Sema al usarlo, así que `Traits<int>::size` quedaba sin resolver al enlazar.

## Diseño

- **Constantes con enlace interno** (T13-022): una variable C++ con enlace interno, `const`, no
  `volatile`, escalar (entera, enum, bool o flotante de hasta 64 bits) y con inicialización constante se
  expone con el valor que le da el evaluador de constantes de Clang (`VarDecl::evaluateValue`). NativeSharp
  la lee como un valor del tipo proyectado, como un enumerador: no tiene dirección (`&T13.K` se rechaza) ni
  símbolo de biblioteca. Si dos unidades del proyecto declaran el nombre con tipos o valores distintos, el
  uso se rechaza.
- **Resultados `constexpr`** (T13-021): una función `constexpr` se llama con la definición que emite Clang
  (tanda 10), y LLVM la pliega al optimizar; el resultado de una expresión `constexpr` del encabezado
  (`constexpr int Sq5 = sq(5);`, `Traits<long long>::size`) es una constante evaluada por Clang. Una
  variable `inline constexpr` de enlace externo sigue siendo almacenamiento con la definición `weak_odr`
  que emite Clang, y el miembro `static constexpr` de una especialización también: Sema instancia su
  definición (`InstantiateVariableDefinition`) antes de emitirla.
- El tipo de un enum debe estar importado (NativeSharp lo nombra), como para cualquier global de ese tipo.

## Cambio

- `CppGlobalDeclaration.h`: `CppConstantValue` y `constantValue`.
- `ClangSession.cpp`: `collectGlobals` evalúa las constantes con enlace interno; `emitCppInlineIR` emite
  los miembros de datos `static` de una especialización implícita.
- `CppSemanticInventory.cpp`: las constantes internas son nombres disponibles y se proyectan como estado
  constante, con su tipo, o con el motivo del rechazo.
- `ProjectNamespaceFunctionModel.cpp`: una constante importada puede ser entera, flotante o bool.

No se modificó Clang ni NSLink. El catálogo NSMeta no cambia de formato.

## Evidencia

Fixture nuevo `pruebas-tanda-13-constexpr/validar.py`, con y sin `--dll`. En `namespace T13`: `constexpr`
`int`, `long long` (`1LL << 40`), `unsigned` (`0xFFFFFFF0u`), negativo, `bool`, `double`, `float` y un
enum; `const int Plain = 40 + 2`; `constexpr int Sq5 = sq(5)` con una función `constexpr`;
`constexpr int SizeOfLong = Traits<long long>::size`; `inline constexpr int Inl`; la función `sq`; y
`T13.Traits<int>.size` escrito en NativeSharp. Resultado 462 frente al control C++ de Clang 23.1.0, de O0 a
O3, con las 2 llamadas C++ de NSC iguales a las de Clang y sin ningún símbolo de una constante interna.

Control negativo: `&T13.K` se rechaza (`cannot take the address of readonly storage`); la contraparte copia
el valor a un local y toma su dirección.

| Variante | Resultado | Evidencia |
|---|---|---|
| Estática | pasa (O0–O3) | `A:/temp/nsc-t13-constexpr-otb50ft4` |
| DLL | pasa (O0–O3) | `A:/temp/nsc-t13-constexpr-qhof0wno` |

Los fixtures de los cambios 1 y 2 siguen pasando (`A:/temp/nsc-t13-especializaciones-tqccbn1n`,
`A:/temp/nsc-t13-plantillas-ns-8ujw2c7y`).

Regresión: en la validación agrupada de la tanda (`TANDA-13-VALIDACION-AGRUPADA.md`).

## Límites

- Una constante de tipo record, arreglo o puntero no se expone como valor.
- Las plantillas de variable (`template <class T> constexpr T zero`) no se escriben desde NativeSharp.
- Una expresión NativeSharp no se evalúa con Clang: NativeSharp lee los valores ya evaluados.
