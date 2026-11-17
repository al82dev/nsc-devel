# T19-007: referencias C++ en métodos estáticos, `T&&`, operadores y retornos de la STL

Fixtures:

- `pruebas-tanda-19-referencias`, caso `referencias-completas` (nuevo), frente a Clang 23.1.0, O0–O3, estática y DLL,
  también desde una plantilla NativeSharp precompilada con y sin el encabezado C++ en el consumidor;
- `pruebas-tanda-18-stl`, grupos nuevos `min-max-clamp` y `array-at`;
- negativos obsoletos convertidos en `pruebas-tanda-11-metodos`, `pruebas-tanda-10-operadores`,
  `pruebas-tanda-10-referencias` y `pruebas-tanda-12-operadores-miembro` (este con un operador miembro no const
  en su matriz).

Sonda de desarrollo: `tools/t19c/d8/sonda-t19-007.py` (`logs/sonda7.log`).

## Defectos

Confirmados con la sonda, todos rechazados antes del cambio:

- Un método estático con `T&` o `const T&` (parámetro o retorno) no se importaba: `projectCppSemanticStaticMethod`
  no tenía la proyección de referencias que ya tenían los métodos de instancia.
- `T&&` en funciones, métodos y constructores: `C++ rvalue reference is not yet mapped`. Solo el constructor de
  movimiento (T11-016) tenía un camino.
- Operadores libres: un operando `T&` o un retorno por referencia se rechazaba (T10-016). Un temporal no enlazaba un
  operando `const T&` (`make(1) + make(2)`): el análisis exigía un lugar direccionable. Un operador con operandos
  `T&&` no se encontraba: la búsqueda asociada (`cppDemandNamespaces`) solo quitaba `&` de los tipos.
- `std::min`, `std::max` y `std::clamp` (`const T&`) y `std::array::at`: el conjunto de sobrecargas se declaraba
  incompleto por una sobrecarga que no admite la llamada (`std::min(std::initializer_list<T>)`).
- Un campo de un record C++ devuelto por valor (`make(5).x`) fallaba al bajar a LLVM: el temporal owned del record no
  tenía almacenamiento C++.
- Un operador miembro no const (`int N::operator-(int)`) no se importaba: su adaptador de Clang (T12-004) recibe el
  objeto como `N&`, y la comprobación de operadores solo admitía operandos por valor o `in`.

## Qué funciona ahora, como en Clang

- Métodos estáticos: `static int& cell(int)`, `static const int& larger(const int&, const int&)`,
  `static void bump(int&)`, con `ref` e `in` como en las funciones (T10-003).
- `T&&` en funciones, métodos estáticos y de instancia y constructores:
  - un valor escrito sin `ref` se materializa en un temporal (`take(5)`, `new V(6)`), también con conversión
    (`take(short)`);
  - `ref x` pasa x mismo, como `std::move(x)`;
  - un lvalue del mismo tipo sin `ref` se rechaza, como Clang (`rvalue reference cannot bind to lvalue`);
  - con `pick(int&)` y `pick(int&&)`, `ref x` elige `T&` y un valor elige `T&&`, como C++ para un lvalue y un
    prvalue.
- Retornos `T&&` de funciones y métodos estáticos y de instancia como referencias de solo lectura: se leen y se
  pasan; escribir en ellos se rechaza, como Clang.
- Operadores libres:
  - operandos `T&` (`operator==(P&, P&)`), que exigen un lugar asignable, como C++ solo enlaza un lvalue;
  - operandos `T&&`, que solo enlazan un temporal;
  - retornos por referencia encadenados (`((c << 2) << 3).x`);
  - temporales enlazados a operandos `const T&` (`make(1) + make(2) + make(3)`).
- Operadores miembro no const sobre un lugar asignable (`m - 4` modifica `m`) y sobre un temporal
  (`makeM(7) - 2`), como C++ enlaza el objeto implícito de un miembro sin calificador de referencia; sobre un
  objeto const (un parámetro `in`) se rechazan, como Clang. Entran en el caso `referencias-completas`, también
  desde la plantilla precompilada.
- `std::min<int>`, `std::max<int>`, `std::clamp<int>` y `std::array<int, 4>::at` (escritura por `T&` y lectura).
- Campos de un record C++ devuelto por valor, también de un operador (`(make(5) - make(2)).x`).

## Diseño

- `MethodParameterSymbol::cppRvalueReference` marca un parámetro C++ `T&&`. Se proyecta como `ref` (o `in`, para
  `const T&&`) con el portador de `T&` (`cppParameterReferenceCarrier`).
- Análisis (`CallAnalyzer`, `ConstructorCandidateEvaluation`): un argumento sin `ref` para ese parámetro se convierte al
  tipo del referente; si es un lvalue de ese mismo tipo, el candidato no es viable. `checkArgumentPassKinds` no pide
  la palabra clave. Entre sobrecargas, `ref x` prefiere `T&` a `T&&`.
- HIR: el valor se guarda en un `TemporaryAddress` cuya dirección recibe el llamado. Pasa en las llamadas a funciones
  de namespace, a métodos y a constructores, y en los operandos `const T&` y `T&&` de un operador C++.
- Un operador que devuelve una referencia denota el lugar que devuelve (`analyzeOverloadedBinary` e
  `isNaturallyAddressable`).
- `MethodParameterSymbol::cppImplicitObject` marca el primer parámetro (`self`) del adaptador de un operador
  miembro. Si es `T&`, también enlaza un temporal, materializado como un operando `const T&`; un lugar const no.
- Una sobrecarga diferida que no admite el número de argumentos escritos ya no deja incompleto el conjunto.
- El almacenamiento C++ de LLVM también se da al temporal owned de un record C++ devuelto por valor; la expresión
  completa lo destruye, como C++.
- Los métodos estáticos llevan `parameterPassKinds`, `returnsReference` y `returnsReadonlyReference`, y su retorno por
  referencia se liga al único parámetro `ref`/`in` o, si no, a almacenamiento estático (T10-003).

El sufijo de firma de T19-006 lleva `@i:&&`, así que `f(int&)` y `f(int&&)` son dos miembros. No se modificó Clang
ni NSLink. El contrato NSMeta no cambia.

## Evidencia

Resultados de `referencias-completas`: 460973 directo y 875611 desde la plantilla precompilada, con y sin
encabezado, iguales a los de Clang O0–O3, con cuatro rechazos que Clang también da. La sonda (casos
`rvalue-retorno` y `rvalue-retorno-escritura`) da 446 como Clang y rechaza la escritura en un xvalue.

| Fixture | Estática | DLL |
|---|---|---|
| pruebas-tanda-19-referencias | `A:/temp/nsc-t19-referencias-vjhlrimw` | `A:/temp/nsc-t19-referencias-dhv67rvo` |
| pruebas-tanda-10-operadores | `A:/temp/nsc-t10-operadores-jf99sbb_` | `A:/temp/nsc-t10-operadores-6zlx2rt_` |
| pruebas-tanda-10-referencias | `A:/temp/nsc-t10-referencias-mbwrjshc` | `A:/temp/nsc-t10-referencias-fw52oxlh` |
| pruebas-tanda-12-operadores-miembro | `A:/temp/nsc-t12-operadores-gcob65be` | `A:/temp/nsc-t12-operadores-evg79a1t` |

`pruebas-tanda-11-metodos` y `pruebas-tanda-18-stl` pasan en la regresión. Regresión de las tandas 10 a 19 con
este cambio: 70 de 70 en estática (`logs/regresion-d29e`) y 51 de 51 en DLL (`logs/regresion-dll-d29d` y, para
`pruebas-tanda-19-traslados`, `A:/temp/nsc-t19-traslados-eszxyxa9`: en esa corrida y en otra, un ejecutable de
control de Clang tardó 30 s y terminó con 0, un fallo del entorno ajeno a NSC; la tercera corrida pasó entera).
Los nueve CTest de interop y la identidad de NSC pasan (`logs/lote-ct2.log`).

## Negativos obsoletos convertidos

Cada uno pasa ahora a una comparación con Clang y conserva un rechazo genuino, que Clang también da:

- `pruebas-tanda-11-metodos` esperaba que `c.take(1)` (`int take(int&&) const`) no tuviera miembro compatible. Ahora
  `c.take(6)` devuelve 6, igual que Clang; se rechaza `c.take(a)` con un lvalue `a`.
- `pruebas-tanda-10-operadores` esperaba que `V operator^(V&, V&)` no se proyectara. Ahora `a ^ b` entra en su
  matriz (estática y DLL, O0–O3, directa y precompilada con y sin encabezado) y modifica `a` como en C++; se rechaza
  `(a + b) ^ b`, un temporal para `V&`.
- `pruebas-tanda-10-referencias` esperaba que `int take(int&&)` no se proyectara. Ahora `T10.take(3)` devuelve 3,
  igual que Clang; se rechaza `T10.take(z)` con un lvalue `z`.
- `pruebas-tanda-12-operadores-miembro` esperaba que `int N::operator-(int)` (no const) no se importara. Ahora `m - 4`
  y `makeN(7) - 2` entran en su matriz (estática y DLL, O0–O3); se rechaza el operador sobre un parámetro `in`.

## Límites

- Los constructores con `T&&` en una plantilla precompilada siguen en T19-016 (objetos C++ con constructores).
- Los operadores compuestos libres (`P& operator+=(P&, const P&)`) necesitan la asignación compuesta sobrecargable de
  NativeSharp; siguen en T19-013, con los operadores de asignación.
- Un record con destructor propio devuelto por valor (`D makeD()`) sigue en T19-009.
