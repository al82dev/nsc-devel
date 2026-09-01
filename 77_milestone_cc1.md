# Milestone CC1 — Compiler Correctness & Semantic Consolidation

**Abierto:** 2026-08-31 · **Cerrado:** 2026-08-31 · **Alcance:** el compilador y su biblioteca.
**Origen:** `NativeSharp_Milestone_CC1_Compiler_Correctness_Semantic_Consolidation_2026-08-31.md`,
auditoría externa.
**Tabla:** `MILESTONE_1_TASKS.md`.

---

## 1. Qué era esto

La auditoría externa no encontró un compilador que no supiera implementar cosas. Encontró uno donde
**distintas capas definían partes distintas del mismo lenguaje**:

> *«Los bugs encontrados no son casualidades desconectadas. Son síntomas de duplicación semántica.»*

Dos evaluadores de constantes. Una regla de `char` en el layout y otra en la conversión. Un CFG real
en LLVM y un modelo secuencial en el análisis semántico. Un parser real y un escáner heurístico de
type seeds. Un frontend clásico y uno moderno.

El milestone se planteó, en palabras del propio documento, para ser *«deliberadamente aburrido en el
mejor sentido: menos sintaxis nueva y más invariantes únicas, compartidas y medibles»*.

---

## 2. El registro, cerrado

**23 de 27 IDs cerrados. Los cuatro abiertos se difirieron a propósito y con razón escrita.**

| ID | Área | Qué era | Estado |
|---|---|---|---|
| C-001 | Constantes | los casts de `case` no normalizaban el valor | ✅ |
| C-002 | Constantes | `/`, `%`, `>>` sin signo evaluados con signo | ✅ |
| C-003 | Constantes | mínimo con signo `/ -1` sin proteger | ✅ |
| C-004 | Constantes | ancho de desplazamiento fijo a 64 | ✅ |
| C-005 | Tipos | `char` de 8 bits en el layout, de 16 en la conversión | ✅ |
| F-001 | Flujo | `?:` sin snapshot/join de ramas | ✅ |
| F-002 | Ownership | `?:` perdía la transferencia de propiedad | ✅ |
| F-003 | Flujo | `&&` analizaba el lado derecho como incondicional | ✅ |
| F-004 | Flujo | `\|\|` igual | ✅ |
| F-005 | Flujo | `do…while` analizado como `while` | ✅ |
| EH-001 | EH | una salida estructurada de `catch` podía saltarse la limpieza del TLS | ✅ |
| EH-002 | EH tests | faltaba la fixture de ejecución `loop`/`catch`/`switch` | ✅ |
| EH-003 | EH tests | faltaba la fixture con dos profundidades de funclet reales | ✅ |
| M-001 | Pipeline moderno | `stackalloc` no llegaba al HIR | ✅ |
| M-002 | Semántica | `stackalloc` moderno sin validaciones | ✅ |
| M-003 | Pipeline moderno | `sizeof(type)` no llegaba al HIR | ✅ |
| M-004 | Semántica | `sizeof(expr)` no era no evaluado y usaba otro tipo | ✅ |
| M-005 | Pipeline moderno | raw `new T` aceptado por la semántica y rechazado por el HIR | ✅ |
| M-006 | Pipeline moderno | sin aritmética de punteros | ✅ |
| R-001 | Reproducibilidad | manifiesto inválido según su propio mecanismo | ✅ |
| R-002 | Reproducibilidad | identidad de build atrasada | ✅ |
| R-003 | Testing | arnés del milestone y cierre del `default` del HIR | ✅ |
| P-001 | Parser | retirar el mini-parser de type seeds | **diferido** |
| P-002 | Parser | con P-001 | **diferido** |
| A-001 | Arquitectura | frontend canónico único | **diferido** |
| A-002 | Arquitectura | frontera backend-neutral | **diferido** |

**Por qué esos cuatro se difirieron, y no es pereza:** ninguno cierra un P1, y cada uno es un
proyecto propio. Retirar el escáner heurístico de type seeds o unificar los dos frontends cambia la
forma del compilador; meterlos en un milestone de corrección habría significado o hacerlos mal o no
terminar el resto. La regla que se siguió fue la del propio documento: **arquitectura no es
corrección.**

---

## 3. El defecto que hay que recordar

`switch` sobre un `char`, con dos etiquetas:

```
switch (aChar)
{
    case 65:
    case 321:   // no cabe en un byte
}
```

El frontend veía **dos etiquetas distintas** y las aceptaba. El backend emitía **`i8 65` para las
dos**. Un `switch` de LLVM con dos valores de caso idénticos es **comportamiento indefinido**.

No era un bug, eran dos compuestos:

1. **el camino** — `char` medía 8 bits en el layout y la conversión constante lo trataba como de 16,
   así que 321 se aceptaba donde no cabía (C-005);
2. **la causa estructural** — los duplicados se comparaban **antes** de normalizar al tipo
   gobernante, así que 65 y 321 parecían distintos (C-001).

Arreglar sólo uno deja el otro esperando. Por eso la fase 1 no fue «arreglar `char`»: fue poner un
**único evaluador de enteros tipado** donde había dos, con la invariante de que una constante entera
es siempre el par `(bits, tipo)` y nunca un `int64_t` suelto.

---

## 4. La regla que gobernó el milestone entero

> **Ninguna verificación cuenta si el modelo contra el que verifico lo construí yo.**

Se escribió después de que el mismo error mordiera tres veces en dos días:

- validé un lowering nuevo contra un stub de LLVM **que había escrito yo**, así que tenía los
  símbolos por construcción — y la compilación real falló con cuatro identificadores desconocidos;
- di `sizeof` y `stackalloc` por presentes en `69_huecos_de_runtime_y_lenguaje.md` **mirando sólo la
  mitad delantera del pipeline**;
- escribí la cadena esperada de un test negativo **sin leer el diagnóstico real**.

De ahí salieron las prácticas que el resto del milestone siguió sin excepción:

| Práctica | Qué evitó |
|---|---|
| barrer los símbolos `LLVM*` contra `LlvmCApi.h` **antes** de escribir backend | tres fases sin una sola compilación fallida por un símbolo |
| compilar con `-Wall -Wextra` antes de entregar, no sólo `-fsyntax-only` | un `-Wswitch` que con `/W4 /WX` habría roto el build |
| que un test negativo exija **el mensaje**, no sólo el fallo | descubrió que `cc1-sizeof-unevaluated` pasaba por la razón equivocada |
| ir a **leer** el sitio donde una tarea dice que hay que tocar | cuatro tareas resultaron no implementables como estaban escritas |
| anotar los riesgos **antes** de la corrida | trece riesgos anotados, ninguno se materializó |

---

## 5. Cuatro tareas no eran implementables como estaban escritas

Esto merece su propia sección porque es el hallazgo más transferible del milestone: **el plan tenía
defectos que sólo aparecen al ir a ejecutarlo.**

| Tarea | Decía | Se midió |
|---|---|---|
| **32** | colgar la obligación de un campo en `FuncletContext`, marcado en «los contextos creados para cláusulas `catch`» | **Nadie empuja una entrada `Catch`** desde el arreglo B-006/B-007: el campo habría sido código inalcanzable, cerrando el hallazgo en el papel sin medir nada |
| **40** | cambiar `sizeof` a `nuint`, hecho cuando «coincide con el pipeline clásico» | **No hay `sizeof` clásico en este árbol.** El criterio no se puede cumplir ni incumplir |
| **44** | calcular el tamaño de `sizeof` en `ProjectMethodBodyModel` | Ese modelo se construye **antes** que `ProjectObjectLayoutModel` y ni siquiera lo recibe: habría exigido reordenar el pipeline o duplicar las reglas de tamaño — la duplicación que el milestone existe para quitar |
| **47** | portar las validaciones de `stackalloc` «del pipeline clásico» | Igual que la 40. **Segunda vez** |
| **62** | extender el lowering de `++`/`--` para punteros | **No existe para ningún tipo**, ni `int`. Hacerlo sólo para punteros dejaría `++p` funcionando y `++i` no |
| **67** | regenerar el manifiesto «con `verify_manifest.py`» | Ese script **sólo verificaba**. No había generador en el árbol |

El patrón: **el documento de origen atribuye repetidamente a un «pipeline clásico» piezas que no
están en este árbol.** Tres tareas distintas (40, 47, 67) apoyadas en la misma premisa falsa. Quien
lea ese documento después debería comprobar cada afirmación de ese tipo antes de planificar sobre
ella.

---

## 6. Lo que se midió, no lo que se construyó

| | Antes | Después |
|---|---|---|
| checklist de Windows | 45/45 | **63/63** |
| `ctest` | 239 tests | **242/242, 100%** |
| comprobaciones en `nsc.cc1.correctness` | 0 (no existía) | **~90** |
| fixtures de ejecución CC1 | 0 | **12**, todas en 42 a `-O0` y `-O2` |
| triage de bugs reportados | 16/16 | **16/16, cero vivos** |
| avisos del build | — | **0**, con `/W4` y `NSC_WARNINGS_AS_ERRORS=ON` |

Y cuatro formas del lenguaje que **parseaban, tipaban y no compilaban** ahora ejecutan: `sizeof`,
`stackalloc`, `new T` escalar y la aritmética de punteros.

La que más valía en la práctica: **`p[i]` funcionaba y `*(p + i)` no compilaba.** Son la misma
dirección escrita de dos formas; ahora salen del mismo GEP con el mismo paso, y una fixture lo
comprueba en las ocho posiciones de un buffer para que no vuelvan a separarse. Era una limitación
reportada por un consumidor externo.

---

## 7. Lo que queda abierto, con ID propio

Nada de esto se arregló «de paso». Cada uno se midió, se anotó y se dejó.

| ID | Qué | Por qué no ahora |
|---|---|---|
| **77** | los logs no muestran **cuántas** comprobaciones corrió una suite, sólo que ninguna falló | un bloque de tests que desapareciera de un `.cpp` pasaría en verde |
| **78** | `valueLayoutForTypeKey` dice que un `enum` mide **8**; `layoutNamedType`, en la misma clase, dice **4** | es la función que calcula el paso de los arreglos. **Sin medir qué hace el binario** — afirmar la expectativa en un test es el error del §4 |
| **79** | `++`/`--` no se baja en el pipeline OOP **para ningún tipo** | es una feature, no una corrección. Fase propia |
| **80** | `p->campo` no se baja; `(*p).campo` sí | cuarta forma con la mitad delantera presente. Ya tiene mensaje propio que nombra la alternativa |

**Deuda menor, anotada donde toca:** `sizeof` no es todavía constante para el evaluador de
constantes (`const int n = sizeof(int);` no compila), lo que depende del mismo problema de orden de
construcción que la tarea 44 y pertenece a la conversación de P-001/A-001; y `sizeof(x)` con `x` sin
asignar sigue reportando error, a propósito.

**El manifiesto no cubre los `.bat`** que construyen y verifican el paquete. Se deja anotado y no se
cambia: ampliar la cobertura es una decisión sobre qué significa el manifiesto, no un arreglo de
R-001. `python verify_manifest.py --audit` lista lo que queda fuera.

---

## 8. Dónde está cada cosa

| Documento | Qué cuenta |
|---|---|
| `73_cc1_fases_0_1_2.md` | identidad de build, constantes y `char`, flujo |
| `74_cc1_fase3_eh.md` | manejo de excepciones |
| `75_cc1_fase4a_sizeof.md` | `sizeof` |
| `76_cc1_fases_4b_4c_4d_4e.md` | `stackalloc`, raw `new T`, punteros, cierre de paridad |
| `77_milestone_cc1.md` | este |
| `MILESTONE_1_TASKS.md` | la tabla de 80 tareas, con su estado |
| `STAGE_CC1_*_CHANGED_FILES.txt` | el desglose por capa de cada fase |
