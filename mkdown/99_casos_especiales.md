# 99 — Casos especiales: lo que se evaluó y se dejó fuera

**Fecha:** 2026-09-02 (vivo — se añade, no se reescribe)

**Para qué es este documento.** Hay cosas que salen una y otra vez en la conversación, se evalúan,
se descartan, y a los quince días vuelven a salir porque el motivo del descarte no quedó escrito en
ningún sitio. Aquí queda **qué se descartó, por qué, y qué tendría que pasar para que volviera**.

**No sustituye a la Lista C del doc 62**, que es el corte del perfil de bootstrap: formas del
lenguaje que no entran ni antes ni durante (LINQ, `async`, `record`, pattern matching…). Aquello es
alcance congelado. Esto es otra cosa: piezas que **sí** estaban en el roadmap y salieron después,
por un motivo que sólo se vio al mirarlas de cerca.

**Regla de este documento:** cada entrada dice **la condición concreta** que la reabriría. Una
entrada sin esa condición no está descartada, está olvidada.

---

## 1. `NativeLibrary` — carga dinámica de DLLs (SHR-035)

**Estaba en:** Bloque 4, marcada «Sí / estratégico» en el doc 62 (B.2, fila 15).
**Estado:** **fuera del roadmap.** No aplazada.

### Qué es

`LoadLibraryW` / `GetProcAddress` / `FreeLibrary`: cargar una DLL en tiempo de ejecución y llamar a
lo que exporta, sin que el enlazador supiera de ella.

### Por qué estaba

Porque parecía la vía para que `nsc` dejara de lanzar `lld-link.exe` como proceso hijo: cargar el
enlazador como DLL y llamarlo. Eso sacaría a `Process.Run` (SHR-032) del camino crítico del
auto-hospedado.

### Por qué sale

**Hay una vía mejor para lo mismo, y no la necesita.** El flag de enlazador tiene tres valores
—`lld`, `nslink-exe`, `nslink-inproc`— y los tres se compilan **dentro** de `nsc`. Para el tercero,
`nsc` enlaza contra `NativeLinkCore.lib` —que ya está en el prefijo— y llama a
`CompatibleDriver::run`. El enlazador va dentro del binario: no hay nada que cargar.

**Y va contra la línea del proyecto.** NativeSharp es estático a propósito: sin CLR, sin GC, interop
por bibliotecas de importación resueltas al enlazar. Un binario que sabe en tiempo de compilación
todo lo que va a llamar es más fácil de razonar y de auditar. La carga dinámica no extiende esa
línea, la contradice.

### Los cuatro casos que sí la justificarían, y por qué ninguno aplica hoy

| Caso | Estado |
|---|---|
| Cambiar de enlazador o backend **sin recompilar `nsc`** | El flag lo cubre para los que se conocen; para uno desconocido haría falta distribuir `nsc` como producto y que cada quien enchufe el suyo. No es el caso. |
| **LLVM como DLL** en vez de `LLVM-C.lib` monolítico | Real, pero es adelgazar el binario, no una capacidad nueva. Vive en el Bloque 7 si algún día pesa. |
| **Analizadores o pasadas de usuario** (estilo *analyzers* de Roslyn) | Decisión de producto que nadie ha tomado. |
| Que un programa NativeSharp cargue una DLL opcional que puede no estar instalada | Hoy la interop va por bibliotecas de importación (Stage 48 entero). Nadie ha pedido lo otro. |

### Qué la reabre

* **Recarga en caliente de código de juego.** El caso nombrado por Alberto el 2026-09-02: un motor
  como el RE Engine de Capcom tiene el motor en C++ y la lógica de juego en C#, y recarga esa lógica
  **en tiempo de ejecución** sin reconstruir el motor. Ese es un consumidor real de carga dinámica y
  el único que ha aparecido hasta ahora con una forma concreta. **No es para ahora** —hoy el motor es
  básico— pero queda escrito para no volver a descartarlo por falta de motivo.

  Cuando toque, `NativeLibrary` es **necesaria pero no suficiente**: recargar pide además poder
  **descargar** (`FreeLibrary` con todo lo que eso implica sobre punteros a función vivos),
  una **ABI estable entre recargas** —que es SHR-088, el mismo problema que el Bloque 6— y decidir
  qué pasa con el **estado** que la DLL vieja tenía en memoria. La pieza pequeña de las cuatro es
  cargar la DLL.

* Que se decida distribuir `nsc` de forma que un tercero pueda enchufar su propio enlazador o
  backend **sin reconstruirlo**.
* Que LLVM como DLL deje de ser cosmética y pase a ser un requisito medido (tamaño del binario,
  o cambiar de versión de LLVM sin reconstruir).
* Que aparezca un consumidor real de plugins.

En cualquiera de los cuatro, se añade **con ese motivo escrito**, no por volver a estar en una
lista.

---

## 2. ID 130 — `uwtable` con valor 0

**Estado:** **medido y deliberadamente no arreglado.**

`LLVMCreateEnumAttribute(uwtable, 0)` imprime basura y trunca el módulo. Medido con el propio
`LlvmCApi.h` del proyecto sobre LLVM 18: el valor 0 es `UWTableKind::None`; con 2 imprime bien.

**Por qué no se arregla:** cambiar el atributo afecta a **todas** las funciones que emite el
compilador, y eso pide su propia medición en Windows —tamaño del `.pdata`/`.xdata`, comportamiento
de SEH, tamaño del binario—. Es un cambio de una línea con una superficie que no se ha medido.

**Qué lo reabre:** que alguien necesite el desenrollado de pila en un caso concreto, o una medición
en Windows del efecto real. En el contenedor se rodea con un interpositor en `harness/shim18.cpp`.

---

## 3. ID 132 — sobrecarga de métodos genéricos por lista de parámetros

**Estado:** **abierto, no bloqueante.** No es un descarte; está aquí para que no se confunda con
ID 131, que sí se cerró.

Las plantillas genéricas se identifican por `nombre``aridad` (`ProjectGenericModel.cpp`, la
construcción de `canonicalKey`), así que dos con la misma aridad y distinta lista de parámetros
chocan con *«duplicate generic template identity»*.

Arreglarlo pide meter la firma en la clave **y** resolución de sobrecargas entre plantillas en el
punto de llamada — y esa clave nombra además la monomorfización, así que el cambio llega hasta los
símbolos decorados.

**Se rodea renombrando la variante**, que es una decisión de API perfectamente normal y es lo que se
hizo en el Bloque 3 (`Of` → `OfRange`). **Qué lo reabre:** que el rodeo deje de ser aceptable —
varias variantes de la misma operación donde renombrar cada una haga la API peor que el hueco.

---

## 4. Decisiones que son tuyas, no de ingeniería

No están descartadas ni planificadas: **esperan una decisión**, y hasta que llegue no se toca nada.

| Qué | Por qué no lo decido yo |
|---|---|
| **SHR-063** — mecanismo de condicionales de compilación | Es elegir una forma del lenguaje, no implementarla. Bloquea al Bloque 4 **sólo si** el driver necesita distinguir configuraciones. |
| **ID 87** — `volatile` | El doc 62 lo aplaza explícitamente y **en este orden**: primero el contrato del modelo de memoria y la semántica de atómicos (SHR-073/074), la palabra después. El propio SHR-075 lo dice: *«no implementar keyword sin contrato»*. |
| **ID 125** — UTF-8 | Cambia qué es un `char` y qué es la longitud de una cadena. No es una función, es una definición. |
| **ID 124** | Probablemente falso. Pendiente de reproducir antes de gastar nada en él. |
| **`nsc` enlaza en proceso o lanza un `.exe`** | El flag permite las dos, así que **ya no bloquea nada**. Lo que decide es si `Process.Run` es bloqueante para el auto-hospedado o simplemente útil. |

---

## 5. Cómo se usa este documento

Cuando algo de aquí vuelva a proponerse —por mí o por quien sea—, la respuesta correcta **no** es
«ya lo descartamos». Es mirar la condición que lo reabre y contestar si se cumple. Si se cumple, se
hace y se borra la entrada. Si no, se dice cuál falta.

Y si algo se descarta y **no** se le puede escribir una condición de reapertura, no está
descartado: no se ha entendido.
