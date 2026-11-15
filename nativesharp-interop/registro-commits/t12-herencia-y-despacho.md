# T12-001 a T12-012: herencia, despacho virtual y tipos dinámicos C++

## Defecto

En la [auditoría inicial](../TANDA-12-AUDITORIA-INICIAL.md) ningún caso pasa:

- un record derivado solo tenía sus propios campos y métodos: `no compatible member named 'x'` para un
  campo o un método de una base;
- un `D*` no se convertía a `B*`: `no viable overload … accepts the argument types`;
- un método virtual se rechazaba;
- un record con bases no tenía constructores ni destructor importados, y un local de un record trivial
  con bases daba `experimental C++ local requires direct automatic construction and cleanup`.

Causa en el código:

- `ClangSession.cpp` (`collectRecords`) captura las bases con su acceso, si son virtuales y su offset,
  pero ninguna proyección las usaba.
- `projectCppSemanticInstanceMethod` (`CppSemanticFunction.cpp`) rechaza `isVirtual`, y
  `projectCppSemanticLifetimeMethod` exige `record.bases.empty()`.
- `cppRecordValueOperationsEligible` (`CppInteropSession.h`) exige un POD de disposición estándar para
  tratar un record como valor; un derivado con campos en la base y en el derivado no lo es.
- NativeSharp no tiene herencia de structs ni conversión de puntero entre tipos C++.

## Diseño

NSC no reimplementa el ajuste de `this`, la vtable del ABI de Microsoft, la herencia múltiple ni la
virtual: los aplica Clang.

- **Adaptadores** (T12-002, T12-004 a T12-009): cada método heredado de una base pública y cada método
  virtual se llama mediante un adaptador que genera Clang. Es una función `extern "C"` con el ABI
  físico de un método de instancia: el objeto primero y, si el resultado es de clase, la memoria del
  resultado después. Su cuerpo es la llamada C++ sobre el tipo derivado por un puntero a miembro de su
  propia clase, convertido a su tipo exacto: la sobrecarga no puede elegir otro miembro y la llamada
  conserva el despacho virtual. El nombre (`cppMemberThunkSymbol`) es un hash de los USR de Clang del
  record y del método, así que toda sesión que lo emite emite el mismo símbolo. La sesión vuelve a
  analizar su fuente con los adaptadores pedidos añadidos y Clang los compila (`weak_odr`). Si Clang
  rechaza uno, cada uno se compila por separado y el rechazado no oculta a los demás.
- **Campos de las bases** (T12-001, T12-008): los campos públicos de las bases públicas no virtuales
  son campos del derivado en el offset de Clang. Un nombre que declara el derivado o una base más
  cercana oculta el de la base; un nombre que dos bases alcanzan a la misma profundidad es ambiguo y no
  se promueve, como en C++. Los campos de una base virtual no tienen offset fijo y no se promueven.
- **Métodos heredados** (T12-004): mismas reglas de ocultación y ambigüedad, por nombre sobre todo
  miembro de la base. Una base no virtual alcanzada por dos caminos (Clang `CXXBasePaths::isAmbiguous`)
  no aporta métodos.
- **Conversión de puntero** (T12-003): Clang informa, para cada base alcanzada solo por derivación
  pública no virtual y por un camino, la suma de sus offsets de base. Un `D*` se convierte de forma
  implícita a ese `B*` sumando el offset, y un puntero nulo sigue nulo. La base se identifica por su
  USR, nunca por nombre. Un cast explícito entre punteros relacionados es un `static_cast`, hacia la
  base o hacia el derivado; `reinterpret_cast` y los records no relacionados conservan la dirección.
- **Construcción y destrucción** (T12-006, T12-007): un record con bases no virtuales, polimórfico o
  no, se construye y destruye con su constructor y destructor de Clang, que construyen las bases y
  fijan el puntero a la vtable. Un local se destruye con una llamada directa al destructor, también si
  es virtual, como en C++. La destrucción mediante una base con destructor virtual la hace la
  biblioteca (`delete` o `p->~B()`), que despacha por la vtable.
- **Valores triviales con bases** (T12-001): un record trivialmente copiable con construcción por
  defecto y destrucción triviales es un valor NativeSharp aunque no sea de disposición estándar. Su
  ABI de retorno lo clasifica Clang (`arrangeFreeFunctionType`): el ABI de Microsoft devuelve en
  memoria un record con base, con constructor declarado o con campos no públicos, aunque mida 8 bytes
  o menos.
- **RTTI y conversiones dinámicas** (T12-010, T12-011): `typeid` y `dynamic_cast` se ejecutan en la
  biblioteca sobre los objetos que ella o NativeSharp crearon. NativeSharp no expresa `typeid` ni
  `dynamic_cast` de un tipo C++.

## Cambio

- `CppMemberThunk.h` (nuevo): prefijo y nombre de los adaptadores.
- `CppRecordDeclaration.h`: un método lleva `inherited` y `dispatchSymbol`; el record lleva sus
  `upcasts` y la clasificación de retorno `indirectReturn`.
- `ClangSession.cpp`:
  - `collectRecords` promueve los campos de las bases, añade los métodos heredados, da adaptador a los
    virtuales y calcula los `upcasts` con `CXXBasePaths`;
  - `recordsSnapshot` pide a Clang la clasificación de retorno de los records trivialmente copiables que
    las reglas de C quizá no describen;
  - `emitCppMemberThunkIR` genera y compila los adaptadores pedidos.
- `CppInteropSession.h`: `emitCppMemberThunkIR`, y `cppRecordValueOperationsEligible` ya no exige POD
  ni disposición estándar.
- `CppSemanticFunction.cpp`: un método heredado o virtual con adaptador se proyecta y se llama por el
  adaptador; los métodos de vida admiten bases no virtuales y destructores virtuales.
- `CppSemanticRecord.cpp`, `ProjectNameLookup.h`: el tipo importado lleva sus `upcasts` y su
  clasificación de retorno.
- `ProjectConversionKind.h`, `ProjectImplicitConversion.h`, `ProjectTypeConversion.cpp`,
  `ProjectConversionClassifier.cpp`: conversión `CppPointerUpcast` con su offset.
- `MirNodeMachine.cpp`: el offset de la conversión implícita y el del cast explícito entre records
  relacionados.
- `OopLlvmExpressionMachine.cpp`: la conversión suma el offset y conserva el puntero nulo.
- `ProjectOopMirStructLayout.h`, `MirLayoutImporter.cpp`, `MirFunctionLowerer.cpp`, `OopLlvmCalls.cpp`:
  una función importada devuelve en memoria el record que Clang clasifica así.
- `DriverArtifactGeneration.cpp`: emite los adaptadores que el programa llama.

No se modificó Clang ni NSLink. El catálogo NSMeta no cambia de formato.

## Evidencia

### `pruebas-tanda-12-herencia`

`namespace T12`:

- `A` y `B2` no polimórficas, `M : A, B2` con constructor;
- `V` con destructor y métodos virtuales, `W : V` con override y destructor;
- `X : A, W`, cuya base polimórfica `W` es la primaria del ABI de Microsoft;
- `VD : virtual VB` con override;
- funciones libres que crean, liberan y reciben punteros a bases.

El consumidor:

- lee y escribe campos de las bases en un `M` construido en NativeSharp (T12-001);
- llama a métodos de la primera y la segunda base (T12-002, T12-004, T12-008);
- convierte un `M*` a `A*` y `B2*`, también nulo, y los pasa a funciones C++; convierte de forma
  explícita hacia la base y hacia el derivado (T12-003);
- construye `W` y `X` y llama a sus métodos virtuales propios y heredados, y a métodos virtuales por un
  `V*` que apunta a un `W` o a un `X` (T12-005, T12-006);
- libera un `W` por su `V*` y comprueba los dos destructores (T12-007);
- llama a un método de la base virtual de `VD` y a su override (T12-009);
- con `--dll`, `typeid` y `dynamic_cast` en la biblioteca (T12-010, T12-011).

Sin `--dll` la biblioteca estática no tiene CRT: se compila con `-fno-rtti`, crea los objetos en
almacenes propios y define `operator delete` (que nombran los destructores de borrado de la vtable) sin
liberar. Con `--dll` es una DLL enlazada con su CRT que usa `new` y `delete`.

De O0 a O3 el fixture compara un control C++ compilado por Clang 23.1.0 con el consumidor NativeSharp
(resultado 4051, y 124055 con `--dll`; T12-012). Exige además que cada símbolo C++ que NSC llama
directamente (16, 19 con `--dll`) tenga los tipos físicos de Clang, y que las 13 llamadas heredadas o
virtuales vayan por adaptadores.

Controles negativos:

- campo y método de una base privada: `no compatible member named 'a'` / `'getA'`;
- campo de una base virtual: `no compatible member named 'vb'`;
- campo y método de una base alcanzada por dos caminos: `no compatible member named …`;
- conversión a una base ambigua, a una base privada o de base a derivado:
  `cannot initialize local …`;
- construcción de un record con base virtual: `struct has only an implicit parameterless constructor`.

Contrapartes del mismo encabezado: conversiones y campos válidos pasan.

| Variante | Resultado | Evidencia |
|---|---|---|
| Estática | pasa (O0–O3) | `A:/temp/nsc-t12-herencia-x8h6xstq` |
| DLL con CRT | pasa (O0–O3) | `A:/temp/nsc-t12-herencia-avg10iut` |

### `pruebas-tanda-12-valores`

`Q : P` y `R : Q` sin constructores, y `S` con constructor declarado. El consumidor declara locales sin
constructor, escribe y lee campos de la base y del derivado, copia, toma la dirección de un local y la
convierte a `P*`, y pasa y recibe estos records por valor. Resultado 112 frente al control, de O0 a O3,
con las 7 declaraciones iguales a las de Clang: `makeQ` (8 bytes) y `makeS` (4 bytes) devuelven en
memoria como en Clang.

| Variante | Resultado | Evidencia |
|---|---|---|
| Estática | pasa (O0–O3) | `A:/temp/nsc-t12-valores-dm_8b_t8` |
| DLL | pasa (O0–O3) | `A:/temp/nsc-t12-valores-wmot47l1` |

Regresión: ver [TANDA-12-VALIDACION-AGRUPADA.md](../TANDA-12-VALIDACION-AGRUPADA.md).

## Límites

- Los campos de una base virtual no se promueven, y un record con base virtual no se construye desde
  NativeSharp: su constructor necesita el argumento oculto de objeto más derivado.
- NativeSharp no expresa `typeid` ni `dynamic_cast` de un tipo C++; los usa la biblioteca.
- Una biblioteca estática con RTTI, `new`, `delete` o variables estáticas con guarda necesita el CRT de
  C++ en el ejecutable: tanda 16.
- Conversión entre punteros a una base virtual: no se ofrece.
- Una referencia a la base (`B&` como `ref` o `in`) no acepta un derivado; se pasa un puntero.
- Herencia de un tipo C++ desde NativeSharp: fuera del alcance.
