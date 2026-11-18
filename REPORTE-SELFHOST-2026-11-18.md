# Reporte de self-host — 2026-11-18

Repositorio `A:\nsc-devel-fixes`, rama `development_cpp__clang_integracion`.
Bibliotecas `A:\nativesharp-libraries`, rama `development`.

Todo el trabajo usó compilaciones incrementales y pruebas dirigidas, ejecutadas desde CMD. **No se ha corrido la
batería completa**: queda pendiente hasta que se pida. No se hizo push ni despliegue.

## Resumen

| Conjunto | Estado | Commit |
|---|---|---|
| SH41: generación de `nsc2` | Emite y enlaza; la autoprueba sigue fallando (falta especializar genéricos) | `877301aba` |
| Frontend (SH05): metaprogramación | Aprobado | `ecb360660` |
| `NS_ROOT` con prefijo aislado (13 pruebas) | Aprobado | `534802717` |
| Defectos semánticos del self-host | Corregidos | `d3d9e1ee6`, `842eb4b6e` |
| Oráculo de semántica (SH06) | Aprobado | `842eb4b6e` |
| Cobertura de lenguaje que destapó SH07 | Avanza: brechas 265 → 131 | `3de14d804`, `b0cc05402` |
| Oráculo de IR (SH07) | **Sin aprobar**: el corpus aún falla | sin commit |
| `stdlib_build`: aviso de checkout desactualizado | Hecho | `0f5c4b2fc` |

Lista de commits de esta tanda, del más reciente al más antiguo:

```
b0cc05402 Parse file-scoped namespaces, C delegates, pack(N) and inferred array initializers
3de14d804 Analyze overflow policies and the 128-bit and decimal types in the self-host
842eb4b6e Compare SH06 semantics against nsc0's implicit project with a fact oracle
0f5c4b2fc Say which libraries checkout is outdated when stdlib_build cannot pass --build-root
d3d9e1ee6 Type calls through generic constraints, base classes and delegates in self-host semantics
534802717 Run the NS_ROOT self-host tests against an isolated prefix and repair SH03 and SH10
ecb360660 Process attribute directives and #run calls in the self-host front end like nsc0
877301aba Let the self-host bootstrap generation build and link nsc2
```

## 1. SH41 — imagen autoalojada (`877301aba`)

Corregido:

- **Búsqueda global.** En el árbol fusionado del bootstrap, el método `FileId File()` ocultaba la clase `File`, y
  `File.GetSize` quedaba tipado como `error` sin ningún diagnóstico. Ahora un miembro de una clase, struct,
  interfaz o unión solo es visible sin calificar desde ese tipo, desde un tipo derivado o desde un tipo anidado en
  él.
- **`ExitProcess` duplicado.** La rutina de arranque lo declaraba otra vez, LLVM lo renombraba a `ExitProcess.1` y
  NSLink fallaba con NSL1020. Ahora reutiliza la declaración existente.
- **`ProjectReference` absoluta.** El self-host la concatenaba al directorio del proyecto; ahora la usa tal cual,
  igual que `nsc0`.

Resultado: `nsc2` se emite (O0), se enlaza con NSLink y `nsc2 --version` responde. Pasaron 17 pruebas dirigidas.

**Sigue bloqueado:** `nsc2 --self-test-basic` termina con una violación de acceso en `ucrtbase`. La causa, reducida
a programas mínimos, es que el backend del self-host emite un único cuerpo por método genérico, con `T` borrado a
una ranura de 8 bytes. El código de Core que trabaja sobre memoria real (`Span<T>` con `T*` y `T[]`) lee entonces
con el ancho y el paso equivocados. El mismo struct sin genéricos devuelve el valor correcto. Hace falta
**especialización de genéricos (monomorfización)** en el backend del self-host. De esto dependen también las 12
pruebas que esperan `nsc2`.

**Regresión introducida por este cambio:** `GlobalLookup` impide que el inicializador de una constante de clase vea
las demás constantes de su propio tipo (`Next = Base + 1`), porque esos inicializadores se analizan sin el tipo
actual. La corrección está escrita pero **sin compilar ni probar**; ver la sección 8.

## 2. Frontend SH05 — metaprogramación (`ecb360660`)

Los dos archivos de metaprogramación siguen en el contrato, sin exclusiones.

- `#[...]` se blanquea hasta el primer `]` fuera de una cadena, conservando los saltos de línea, igual que en
  `nsc0`.
- `#run(Nombre(args))` se evalúa con el subconjunto del sandbox de `nsc0`: una función `int` única, con parámetros
  `int`, cuyo cuerpo es un solo `return expr;`.
- Paridad de AST: una local `raii` pasa a ser un modificador, y `extern C` dentro de un tipo produce
  `MethodDeclaration` con `cimport:<dll>`.

Pruebas: la paridad de tokens se cumple en los 1158 archivos y la de AST en los 16 casos del contrato. Pasaron
12 pruebas dirigidas, `nsc.selfhost.frontend` incluida.

## 3. Pruebas `NS_ROOT` con prefijo aislado (`534802717`)

El prefijo `A:\temp\ns-prefix` se generó con `cmake --install build-integracion` más el mismo `nslink.exe`.

**Por qué es equivalente a `C:\native-sharp`:**

- Doce verificadores solo leen `bin/nslink.exe`, que es idéntico byte a byte al de `C:\native-sharp`.
- SH03 lee además el runtime y su `.abi`. En el prefijo aislado son idénticos a los de este build; en
  `C:\native-sharp` no coinciden, porque es una instalación anterior.

Resultado: pasaron 11 de 13 sin cambios. Las dos restantes se corrigieron así:

- **SH03.** Los símbolos `LLVMMDNodeInContext2` y `LLVMAddNamedMetadataOperand`, que se usan para
  `/include:_fltused`, se registraron como extensiones del bootstrap. La comprobación del inventario no cambia.
- **SH10, árboles limpios.** Ahora conservan la relación real entre checkouts (`compiler/` junto a
  `nativesharp-libraries/stdlib/`) sin reescribir nada. El manifiesto vuelve a cubrir las fuentes reales de las
  bibliotecas.
- **SH10, fuentes del compilador.** Desde `077e481e2`, una fuente del compilador no es un programa válido por sí
  sola, y `nsc0` también la rechaza. Ahora `nsc1 --verify-ssa <proyecto.nsproj>` verifica el grafo completo, y la
  prueba exige que contenga cada fuente. Hay dos controles negativos: una fuente dependiente suelta y un proyecto
  con un nombre sin resolver.
- **Proyectos de reenvío.** Un proyecto sin fuentes es válido si referencia otro proyecto; esa es la regla de
  `nsc0`.

## 4. Correcciones semánticas del self-host (`d3d9e1ee6`, `842eb4b6e`)

Llamadas que el self-host tipaba como `error` sin informar nada:

- miembros accesibles a través de restricciones genéricas (`T : IComponente`);
- miembros heredados de la clase base;
- invocación de delegados, incluidos los genéricos cerrados.

Además:

- una variable local de tipo delegado dejaba de ser un lugar (`place`) cuando se la invocaba;
- el parser leía `throws` de un constructor como una declaración dentro del cuerpo; ahora sigue el orden de
  `nsc0`.

## 5. Oráculo SH06 (semántica) — aprobado (`842eb4b6e`)

Desde `2e154f127`, `nsc0` analiza un archivo suelto como **proyecto implícito** (el archivo más Core) y vuelca un
modelo ya monomorfizado. La comparación byte a byte con el self-host dejó de tener sentido.

- **Self-host.** Nueva opción `--core-project <NativeSharp.Core.nsproj>` detrás del archivo. Equivale a
  `NSC_CORE_LIBRARY_ROOT`: en el self-host, `Environment.GetEnvironmentVariable` es un stub. Los volcados y el IR
  se limitan al archivo raíz. Sin la opción, la salida no cambia.
- **Oráculo.** Proyecta las salidas de ambos compiladores a hechos del archivo raíz y los compara:
  - tipos (clase y nombre);
  - métodos (dueño, nombre y aridad);
  - pares (nombre, tipo) de cada lugar nombrado.

  Las instancias genéricas se comparan por nombre base, y ningún nodo puede quedar tipado como `error`.
- **Controles negativos.**
  - Mutación: `int sum` → `long sum`, que solo ve el self-host, debe detectarse.
  - Un nombre sin resolver debe rechazarse en los dos compiladores.
- **Ajuste de contrato.** Dos ids de tipo en invariantes de metadatos pasan de 18 a 19 y de 19 a 20, porque
  `05a105dfc` añadió el builtin `sbyte`. Tamaños y alineaciones no cambian.

Resultado: 12 casos con 115 hechos, 2 controles negativos, 28 invariantes y estabilidad O0/O2. Pasaron 8 pruebas
dirigidas.

## 6. Cobertura de lenguaje que destapó SH07 (`3de14d804`, `b0cc05402`)

La regla del corpus de SH07 es: «si `nsc0` llega a SSA con una fuente, el self-host también». Antes se cumplía
vacuamente, porque el self-host no informaba los nombres sin resolver. Hoy deja ver construcciones del lenguaje
que el self-host no implementa.

Añadido al self-host:

- `checked(…)`, `saturating(…)`, `unchecked(…)` y `wrapping(…)`. El backend las sigue rechazando: no las compila
  sin la comprobación de desbordamiento.
- Tipos `i128` y `u128` (16 bytes, alineación 16) y `decimal` (16 bytes, alineación 8). Son la ABI de `nsc0` y se
  registran en el primer uso, para no mover los ids de otros tipos.
- `namespace X;`, `extern C delegate …`, `pack(N)` y `new T[] { … }`. Este último lo rechaza explícitamente el
  backend de bootstrap.

Brechas del corpus (fuentes que `nsc0` acepta y el self-host rechaza): **265 → 131**.

## 7. SH07 (IR) — sin aprobar, cambios sin commit

Los cambios están en `selfhost/bootstrap/verify_ir.py` y `selfhost/contracts/ir.contract.json`.

- Valida las cabeceras actuales de `nsc0`: `project.oop.hir`, `cfg.module nsc-llvm-cfg/1`, etc.
- Compara los nombres de función del módulo raíz desmangleando los de `nsc0`
  (`__nsc_function_<mod>$3A$3A…__Nombre`).
- El self-host se ejecuta con `--core-project`. Controles negativos: una función renombrada y un nombre sin
  resolver.
- La invariante `free.array` pasa a ser `cleanup.if-live @heap`. `2e154f127` cambió la fuente de `delete[]` a
  `owned`, y `nsc0` hace la misma limpieza en los dos `return`.

Las 6 etapas y sus invariantes pasan. **Falla el corpus** con 131 brechas:

| Grupo | Fuentes | Requiere |
|---|---|---|
| Familias `static_field_query*` (`static foreach (field in Fields(T))`) | ~96 | Monomorfización (la misma pieza que SH41) |
| Cola larga: `is`/`as`, propiedades, `record`/`derive`, traits, extensiones, `foreach` por rango, `unsafe {}`, `simd`/`mask`, genéricos con parámetros constantes, conversiones implícitas | ~35 | Cambios puntuales de parser y semántica |

La regla del corpus **no se relajó**.

## 8. Cambios en curso sin compilar ni probar (sin commit)

Escritos, pero **no validados**:

- `Parser.ns`: el modificador `partial` (flag como en `nsc0`) y los inicializadores agregados `T v = { a, b }`.
- `SemanticAnalyzer.ns`:
  - análisis de inicializadores agregados;
  - **corrección de la regresión de la sección 1**: el inicializador de una constante se analiza dentro de su
    tipo.

## 9. Pendiente

1. Revisar este reporte. El trabajo queda detenido hasta esa revisión.
2. Compilar y validar los cambios de la sección 8, prioritariamente la regresión de constantes.
3. Cola larga de SH07: unas 35 fuentes; estimación de algunas horas.
4. **Monomorfización en el self-host**: desbloquea SH41, sus 12 pruebas dependientes y unas 96 fuentes de SH07.
   Estimación de días; es una funcionalidad nueva del compilador.
5. Batería completa, cuando se pida.

## Nota sobre la otra PC (`stdlib_build` / `--build-root`)

El error `invalid choice: '...\nativesharp-libraries-build'` venía de que `nsc-devel-fixes` ya enviaba
`--build-root` mientras el checkout de bibliotecas era anterior a `41d76a9`. Con `0f5c4b2fc`, `stdlib_build`
comprueba esto primero y nombra el checkout desactualizado. Se probó con una copia de la versión de `origin`:
salida 2 y el mensaje nuevo.
