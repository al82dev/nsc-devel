# 70 — Roadmap del compilador

**Fecha:** 2026-08-30.
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

**Estado verificable hoy:** `ctest` 238/238, checklist de ejecución 33/33, triage 16/16.

---

## 2. Lo que le queda al compilador

Los huecos concretos, medidos contra el árbol, están en `69_huecos_de_runtime_y_lenguaje.md`.
Resumen del orden:

### 2.1 Lenguaje

`switch`/`case` y el operador ternario `?:` primero — no existe ni el token `?` — más `do…while`, y
re-medir `x += y` en lowering OOP contra C8. Son los más baratos y los que están deformando código
real ahora mismo.

Después: `params`, interpolación de strings, `sbyte`, `Nullable<T>`, structs genéricos,
`readonly struct` / `in` en el receptor, union anónima, alias genéricos, inferencia de argumentos
de tipo en genéricos.

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
