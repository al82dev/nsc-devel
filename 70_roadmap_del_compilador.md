# 70 — Roadmap del compilador

**Fecha:** 2026-08-30, actualizado 2026-08-31.
**Alcance:** el compilador y su biblioteca (`src/`, `include/`, `lib/`, `tests/`). Nada más.

Los bindings y los ports de `saru_framework` y del tutorial de OpenGL los llevan otras instancias.
No son carril de este repo: **actúan como consumidoras y reportan limitaciones del compilador**, y
esos reportes se atienden aquí. Arreglarlos es responsabilidad de esta instancia; su código no.

---

## 1. Cerrado

**Fases C1…C8 del roadmap del compilador** (`01_Compiler_Roadmap.md`, docs `63_`…`67_`):

```
C1  Interop completion         proc-address round trip, identidad de import, borde de excepciones
C2  C aggregate completeness   arrays embebidos, union, pack/align/offsets
C3  Alias / FFI ergonomics     typedefs de builtin/puntero/array/function-pointer
C4  Language references        ref / out / in
C5  Certification hardening    layout contra clang + matriz de EJECUCION en Windows real
C6  Struct members             ctor, propiedades, indexers, operadores
C7  Definite assignment        zero-init + inicializador agregado
C8  Type root                  Object / ToString / Equals / GetHashCode, sin boxing
```

C5 era la única con un `◑`: su mitad de ejecución dependía de Wine y se cerró corriendo la matriz
en Windows real (`claude/44_ejecucion_real_windows_c1_c8.md`).

**Bugs reportados: cero vivos.** Ocho medidos, siete arreglados, uno ya estaba muerto, y los ocho
bajo `ctest` (`68_triage_y_cierre_de_bugs_reportados.md`).

**Stage 57 — huecos de lenguaje, primera y segunda ronda:**

```
?:          operador condicional, perezoso, asociativo a la derecha   doc 71  VERDE
do...while  cuerpo antes de la primera prueba, continue a la cond.    doc 71  VERDE
switch/case sin fall-through, etiquetas constantes, LLVMBuildSwitch   doc 72  VERDE
```

Las tres cerraron en Windows real. `nsc.stage57.language` cubre las tres fixtures a `-O0` y `-O2`
más seis casos negativos del `switch` que verifican que falló **y por qué**.

**Estado verificable hoy:** `ctest` 239/239, checklist de ejecución 39/39, triage 16/16.

---

## 2. Lo que le queda al compilador

Los huecos concretos, medidos contra el árbol, están en `69_huecos_de_runtime_y_lenguaje.md`.
Resumen del orden:

### 2.1 Lenguaje

**Hecho:** `?:` y `do…while` (doc 71) · `switch`/`case` (doc 72).

**Lo siguiente, por costo creciente:**

1. **Re-medir `x += y`** en lowering OOP contra C8. Reportado roto contra C1–C5; nunca se
   re-midió. Es medición, no implementación, y puede que ya esté muerto — como b7.
2. **`params`** y **`sbyte`**. Los dos son mecánicos.
3. **Interpolación de strings.** Depende de `StringBuilder` y de `ToString(double)` para valer la
   pena, así que va después de §2.2.1.
4. **`??` y `?.`.** El token `?` ya existe; lo que falta es la decisión de qué significan con
   referencias, y `Nullable<T>` si se quiere la versión completa. Diseño, no cobertura.
5. **Structs genéricos** (`Vector2<T>`), **`readonly struct` / `in` en el receptor**, union anónima,
   alias genéricos, inferencia de argumentos de tipo en genéricos.

**No planeados, dicho explícitamente:** `goto`; `goto case`/`goto default` (no hacen falta sin
fall-through); switch sobre `string`; patrones.

### 2.2 Biblioteca (`lib/NativeSharp.Core*`)

1. **Texto y números** — `ToString(float/double)`, `Parse`/`TryParse`, `StringBuilder`, `Console`
   completo. Hoy no se puede imprimir ni leer un flotante. Es lo que más cuesta en el día a día.
2. **Datos** — `Encoding`, `Convert`/Base64, `BitConverter`, `Span<T>`, `Guid`, algoritmos sobre
   `T[]` crudo.
3. **IO** — `Stream`, `BinaryReader`/`BinaryWriter`, `ReadAllBytes`.
4. **Compresión** — inflate/deflate + CRC32.
5. **Red** — Winsock2 y WinHTTP.
6. **Serialización.**

### 2.3 Infraestructura del compilador

**COM como primitiva del lenguaje** es la de mayor retorno: hoy el consumo de COM se hace generando
thunks fuera del compilador, y el lenguaje no lo expresa.

Después: debug info CodeView/PDB · link contra `.lib` externos · linker embebido e incremental ·
modo release/debug formal (qué se cae: asserts, bounds checks) · LTO/ThinLTO · determinismo de
punto flotante · assembler integrado y `asm {}` · escape analysis para stack allocation de clases ·
`comptime`/`#run` · self-hosting.

### 2.4 Del roadmap 0.2

`01_roadmap_nativesharp_0_2.md` marca stages 56 a 62 que **este repo no va a hacer**: los entregó
el carril de bindings, fuera del compilador, y el número nunca se actualizó. De esa lista sigue
abierto para el compilador: **57 (External HWND Hosting)** y **63 (Certificación 0.2)**.

Ojo con la colisión de numeración: "Stage 56" significa dos cosas — el slot de Win32 Windowing en
ese roadmap, y el contenedor de las fases C1…C8. Los docs `63_` a `67_` son lo segundo.

---

## 3. Deuda abierta, sin ronda asignada

- **Propiedades sólo-`set`.** Medido y documentado, no implementado: la resolución de propiedades
  está anclada al getter de punta a punta. Ver §6.3 de `68_triage_y_cierre_de_bugs_reportados.md`.
- **Archivo y línea en los errores internos.** Hoy dicen el método (`while lowering '<mangled>'`),
  que ya es la diferencia entre bisecar y abrir un archivo, pero no la ubicación exacta.
- **El evaluador de constantes está duplicado.** Stage 37 tiene uno completo, encerrado dentro de
  `isCompileTimeConstantExpression`, que sólo devuelve un `bool`; `switch` necesitó otro, más
  estrecho, para plegar etiquetas. Extraer el de Stage 37 a un método reutilizable dejaría una sola
  noción de "constante" en el compilador y le daría a las etiquetas las mismas reglas que ya tienen
  los campos `const`. Ver §3.5 y §6 de `72_switch_case.md`.
- **Orden de `ProjectMethodBodyModel::build()`.** Analiza cuerpos de método antes que
  inicializadores de campo, lo que obliga a rodeos cuando algo de un cuerpo necesita el valor de un
  `const` que se define en términos de otro. Reordenar los dos bucles probablemente es seguro
  —un inicializador de campo no puede referirse a un cuerpo— pero no está medido.

---

## 4. El marco que no cambia

De `claude/03_posicionamiento.md` y `claude/04_restricciones_arquitectonicas.md`:

- NativeSharp es un **lenguaje ligero para hacer videojuegos**, nunca un engine.
- Plataforma: **Win32 / OpenGL / Vulkan / DirectX**, exclusivamente.
- **Sin reflection en runtime, sin runtime gestionado, sin ejecución dinámica de código.**
  `comptime`/`#run` no viola esto: es compile-time y produce un binario 100% estático.
- **LLVM no es una dependencia permanente asumida.** HIR→MIR→LLVM ya está en forma
  backend-agnostic donde importa: MIR es IR propio, LLVM es un backend en el último escalón.
  Self-hosting y cambiar de backend son ejes independientes.

  La consecuencia práctica, que hay que saber antes de tocar el backend: **el árbol no incluye los
  headers de LLVM.** `src/LLVM/LlvmCApi.h` declara a mano el subconjunto de la API C que se usa, y
  se linkea contra `LLVM-C.lib`. Eso mantiene la superficie de dependencia explícita y medible —hoy
  141 símbolos— pero significa que **usar una función de LLVM que no esté en esa lista falla al
  compilar**, como identificador desconocido. Después de tocar `ProjectOopLlvmLowerer.cpp`, cruzar
  sus símbolos `LLVM*` contra ese header antes de pedir una compilación. Ver §7 de
  `72_switch_case.md`, donde esto costó una corrida.
