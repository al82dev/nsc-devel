# 70 — Roadmap consolidado: los cinco carriles

**Fecha:** 2026-08-30.
**Por qué existe:** hay cinco carriles corriendo en paralelo y ninguno estaba en el mismo lugar
que los otros. Esta vista no existía; había que reconstruirla leyendo cinco documentos que no se
citan entre sí, y dos de ellos daban una imagen desactualizada. Si algo diverge entre este archivo
y los docs fuente, **los docs fuente mandan** — esto es el mapa, no el registro primario.

---

## Carril 1 — Roadmap del compilador, fases C1…C8

Fuente: `01_Compiler_Roadmap.md` y los docs `63_`…`67_`.

```
C1  Interop completion         CERRADA   proc-address round trip, identidad de import, borde de excepciones
C2  C aggregate completeness   CERRADA   arrays embebidos, union, pack/align/offsets
C3  Alias / FFI ergonomics     CERRADA   typedefs de builtin/puntero/array/function-pointer
C4  Language references        CERRADA   ref / out / in
C5  Certification hardening    CERRADA   layout contra clang + matriz de EJECUCION en Windows real
C6  Struct members             CERRADA   ctor, propiedades, indexers, operadores
C7  Definite assignment        CERRADA   zero-init + inicializador agregado
C8  Type root                  CERRADA   Object / ToString / Equals / GetHashCode, sin boxing
```

**El carril está cerrado.** C5 era el único con un `◑` —la mitad de ejecución dependía de Wine— y
se cerró corriendo la matriz en la máquina Windows real (`claude/44_ejecucion_real_windows_c1_c8.md`).

Colas menores, ninguna bloqueante: union anónima dentro de struct, alias genéricos, structs
genéricos, `readonly struct` / `in` sobre el receptor.

---

## Carril 2 — Roadmap de stages 0.2

Fuente: `01_roadmap_nativesharp_0_2.md`.

```
48.5  PInvoke host integration   OK       56  Win32 Windowing            (*)
48.6  C++/CLI + cierre 48        OK       57  External HWND Hosting      ABIERTO
49    RAII + Move                OK       58  Input / Gamepads           (*)
50    Ownership Types            OK       59  Math                       (*)
51    String completo            OK       60  Graphics Bootstrap         (*)
52    Generic Collections        OK       61  nsbind                     (*)
53    Time / FS / Environment    OK       62  Official Bindings          (*)
54    Threading / Atomics        OK       63  Certificacion 0.2          ABIERTO
55    Process / HW / Diagnostics OK
```

**(\*) Este roadmap fue rebasado por los hechos y nunca se actualizó.** Los stages marcados no
están pendientes: los entregó el carril de bindings, fuera del compilador. Win32 windowing (B2),
input (B2/B2b), Math (B6), graphics bootstrap (B1 y B4), nsbind 1.0 y las official bindings ya
existen y están verificadas.

De esta lista sigue realmente abierto: **57 (External HWND Hosting)** y **63 (Certificación 0.2)**.

**Colisión de numeración a tener presente:** "Stage 56" significa dos cosas distintas — el slot de
Win32 Windowing en este roadmap, y el contenedor de las fases C1…C8 del compilador. Los docs `63_`
a `67_` son lo segundo.

---

## Carril 3 — Bindings, 12 subinstancias

Fuente: `claude/38_bindings_roadmap.md` y `claude/40_estado_subinstancias_bindings.md`.
Criterio de VERIFICADO: generado, compila, linkea, carga la DLL, llamada real, `-O0` y `-O2`.

**11 de 12 verificadas.**

| | | |
|---|---|---|
| B1 OpenGL | OK | 66 imports + 1083 thunks, 5645 constantes, GL 1.0→4.6 |
| B2 Win32 | OK | 67 entry points, ventana real + input |
| B2b GLFW | OK | 108 entry points, 21 callbacks, gamepads |
| B3 SDL2 | OK | 169 entry points; SDL3 espera headers |
| B4 DirectX D3D11 | OK | 421 thunks COM, swap chain + shaders + read-back |
| B5 OpenAL | OK | 81 entry points + 33 de EFX |
| B6 Math | OK | ~80 chequeos contra GLM |
| B7 Assimp | OK | shim de escena, Milestone 2 |
| B8 stb | OK | los 7 headers, 91 entry points |
| B9 raylib | OK | 70 de 517 entry points |
| B11 Vorbis + MP3 | OK | round trip encode/decode |
| B12 Física | OK | Box2D 3, Bullet, Box3D |
| **B10 SFML/CSFML** | **PENDIENTE** | cortado en Vulkan dentro del CMake de SFML 2.6 |

Abierto: B10, el cuelgue de Chipmunk, wrappers para los joints de Box2D/Bullet/Box3D, y **todo el
Tier 3 sin tocar** — Vulkan, OpenXR, WebGPU, OpenCL — más D3D12, XAudio2 y DirectStorage.

---

## Carril 4 — Port de `saru_framework`

Fuente: `claude/51_port_saru_framework_fundacion.md` y `claude/52_handover_port_saru_nativesharp.md`.
61.412 líneas en 353 archivos. Es lo que está ejerciendo el lenguaje de verdad.

```
Ronda 1   Fundacion: valores y matematica            HECHA
Ronda 2   Hojas: 97 archivos, 4.841 lineas           SIGUIENTE  (96 sin dependencia nativa)
Ronda 3   Ciclo de graficos: 40 archivos, 15.706     el obstaculo real
Resto     256 archivos, 27.529 lineas                incremental
```

Faltan **47.813 líneas**. El ciclo de 40 archivos —`GraphicsDevice`, `Texture2D`, `Game`,
`ContentManager`, `SpriteFont`…— no se puede partir y no se dibuja un sprite sin él. Su
verificación ya no es comparar floats: hay que abrir ventana y leer píxeles, o sea que la corre
Alberto en Windows.

Carril hermano: port del tutorial de OpenGL (`claude/51_port_opengl_tutorial_ronda_1.md`).

**Nota importante tras cerrar los bugs:** varios workarounds de este port ya no hacen falta, y
tres de ellos deformaban la API pública de los tipos. Ver `claude/46_cierre_de_bugs_reportados.md`.

---

## Carril 5 — Candidatos 0.3.0

Fuente: `claude/06_candidatos_0_3_0.md`. Vive después de certificar 0.2.

Debug info CodeView/PDB · interop COM genérico · linker y assembler embebidos · link contra `.lib`
externos de vcpkg · linking incremental · **networking** · **serialización** · determinismo de
punto flotante · modo release/debug formal · LTO/ThinLTO · sanitizers para código de usuario ·
debugger (PDB → sidecar de ownership → DAP) · extensión de VS Code y LSP · `comptime`/`#run` ·
allocators explícitos · stack allocation por escape analysis · **self-hosting (NativeSharp 2.0)**.

---

## El marco que no cambia

De `claude/03_posicionamiento.md` y `claude/04_restricciones_arquitectonicas.md`:

- NativeSharp es un **lenguaje ligero para hacer videojuegos** — de un Pong a un motor propio —
  **nunca un engine**. Cualquier candidato que empuje hacia ser engine (render graph, scene graph,
  editor, asset pipeline) queda descartado por diseño.
- Plataforma: **Win32 / OpenGL / Vulkan / DirectX**, exclusivamente. Nunca CLR, ASP.NET, SQL, web,
  ni aplicaciones de soporte comercial.
- **Sin reflection en runtime, sin runtime gestionado, sin ejecución dinámica de código.**
  `comptime`/`#run` no viola esto: es compile-time y produce un binario 100% estático.
- **LLVM no es una dependencia permanente asumida.** El pipeline HIR→MIR→LLVM ya está en forma
  backend-agnostic donde importa: MIR es IR propio y LLVM es un backend en el último escalón.
  Self-hosting y cambiar de backend son ejes **independientes**; no asumir que van juntos.

---

## Lo que sigue, mirando sólo el compilador

Los huecos concretos, medidos, están en `69_huecos_de_runtime_y_lenguaje.md`. El orden:

1. **`switch` + `?:`** — los dos que están deformando código real ahora mismo, y los más baratos.
2. **Texto y números** — `ToString(float/double)`, `Parse`, `StringBuilder`, `Console`. Hoy no se
   puede imprimir ni leer un flotante.
3. **`Encoding`, `Convert`, `BitConverter`, `Span<T>`, `Stream`, `ReadAllBytes`.**
4. **Inflate/Deflate + CRC32.**
5. **Fundación COM** — desbloquea D3D11, WIC, WASAPI, DirectWrite de una sola vez.
6. **Certificación 0.2** (stage 63).

Abierto y documentado, sin dueño de ronda todavía: propiedades sólo-`set` (§6.3 del doc 68) y
archivo:línea en los errores internos, que hoy sólo dicen el método.
