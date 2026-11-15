# T11-002, T11-003, T11-008, T11-009, T11-011, T11-012, T11-018, T11-019 y T11-022: objetos y propiedad

Pruebas de conjunto, sin cambios de producción.

## Representación (T11-002)

Una instancia de clase C++ es un valor struct de NativeSharp:

- su almacenamiento tiene el tamaño y la alineación que calcula Clang (`ASTRecordLayout`), y sus
  campos públicos están en los offsets de Clang (T11-003, T11-011, T11-012);
- un record trivial (POD, con copia, asignación y destructor triviales y públicos) es copiable y pasa
  por valor con la convención de Clang;
- un record con constructor, destructor o copia no triviales es move-only. Se construye en su
  almacenamiento con un constructor importado (`new T(...)`, T11-009) y su destructor se llama al
  salir del ámbito (T11-013). La copia y el movimiento son explícitos
  ([t11-constructores.md](t11-constructores.md));
- `ref`, `in` y los punteros no poseen el objeto: no lo destruyen (T11-019).

## Objetos dinámicos y propiedad (T11-018, T11-022)

NativeSharp no reserva objetos C++ en el heap. Un objeto dinámico lo crea y lo libera la biblioteca con
sus propias funciones (`create` y `destroy`). NativeSharp guarda el puntero, que no posee el objeto. Así
el `new` y el `delete` son del mismo CRT y del mismo heap, también cuando la biblioteca es una DLL.

## Evidencia

### `pruebas-tanda-11-objetos`

`struct alignas(32) Q { char a; double b; short c; }`, con relleno entre los campos, constructor,
destructor y métodos. El consumidor:

- construye un local, que la biblioteca comprueba alineado a 32;
- escribe `a` y `c`, que la biblioteca lee;
- llama a `scale`, que escribe `b`, y lee `b`;
- compara `sizeof(T11.Q)` y la alineación con las de la biblioteca.

Resultado 1169 frente al control C++ de Clang 23.1.0, de O0 a O3, con las 6 declaraciones del IR de
NSC iguales a las de Clang.

| Variante | Resultado | Evidencia |
|---|---|---|
| Estática | pasa (O0–O3) | `A:/temp/nsc-t11-objetos-8xzs5ci7` |
| DLL | pasa (O0–O3) | `A:/temp/nsc-t11-objetos-dlxk8kni` |

### `pruebas-tanda-11-propiedad`

`struct R` cuenta sus objetos vivos. La biblioteca ofrece:

- `create` y `destroy`: con `--dll`, `new` y `delete` en una DLL enlazada con el CRT; sin `--dll`, un
  almacén propio de la biblioteca estática, que no tiene CRT;
- `shared`: un objeto de la biblioteca que nunca se destruye;
- `bump(R&)`.

El consumidor modifica el objeto compartido por un puntero, crea uno dinámico, construye un local que
pasa por `ref` y lo destruye al salir del bloque, y libera el dinámico con `destroy`. El contador
comprueba que cada objeto se destruye una sola vez y por su propietario: los punteros no destruyen.
Resultado 29 frente al control, de O0 a O3, con 8 declaraciones iguales a las de Clang.

| Variante | Resultado | Evidencia |
|---|---|---|
| Estática | pasa (O0–O3) | `A:/temp/nsc-t11-propiedad-seh1wwwb` |
| DLL con CRT | pasa (O0–O3) | `A:/temp/nsc-t11-propiedad-_deqyn62` |

Regresión: ver [TANDA-11-VALIDACION-AGRUPADA.md](../TANDA-11-VALIDACION-AGRUPADA.md).

## Límites

- Un ejecutable NativeSharp no enlaza ni inicializa el CRT de C++. Una biblioteca estática que usa
  `new`, `delete` o variables estáticas con guarda necesita el CRT en el ejecutable: tanda 16.
- NativeSharp no expresa `new T` en el heap para un tipo C++.
