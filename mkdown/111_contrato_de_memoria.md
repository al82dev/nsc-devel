# 111 -- Contrato de memoria, atómicos y `volatile`

Fecha: 2026-09-03
Fase: M1.3
Estado: **contrato escrito, sin `volatile` todavía.** SHR-075 lo dice desde el principio: *"no
implementar keyword sin contrato"*, y el doc 62 aplaza ID 87 **en este orden**: primero el contrato
del modelo de memoria y la semántica de atómicos (SHR-073/074), la palabra después.

**Y una advertencia sobre el alcance de la verificación, dicha antes que nada.** Un modelo de
memoria **no se verifica con fixtures**. Un programa con una carrera de datos pasa mil veces y falla
la mil una, en otra máquina, con otro planificador. Lo que este documento entrega es un **contrato**:
lo que el compilador promete y lo que el programador puede suponer. Lo que las pruebas pueden
entregar es mucho menos — que los atómicos existentes hacen lo que dicen en el caso feliz, y que el
IR emitido lleva el ordenamiento que el contrato exige. Confundir las dos cosas es cómo un proyecto
acaba creyendo que tiene un modelo de memoria porque una prueba de hilos pasó.

---

## 1. Lo que ya existe

- `AtomicInt32` y `AtomicInt64` (Stage 54.2), con incremento, decremento, suma, intercambio y
  comparación-e-intercambio. Fixture `nsc.stage54-2.atomics`.
- `Thread` (crear, unir, separar), `Mutex`, `Monitor`, `Semaphore`, `Event` (Stage 54.1, 54.3–54.5),
  sobre `CreateThread`, SRW locks y variables de condición de Win32.
- `Interlocked*` como operaciones núcleo del compilador: diez enumeradores en
  `MethodCoreOperationKind`.

Lo que **no** existe: una sola línea que diga qué garantiza cualquiera de ellas.

---

## 2. El contrato

### 2.1 Accesos ordinarios: el compilador puede reordenarlos

Una lectura o escritura de un campo, un local o un elemento de arreglo **no tiene garantía de
ordenamiento respecto de ningún otro hilo**. El compilador puede reordenarlos, fusionarlos,
eliminarlos si el resultado no se observa dentro del mismo hilo, y mantenerlos en registro sin
tocar memoria.

**Dentro de un hilo, el programa se comporta como está escrito.** Eso es lo único que un acceso
ordinario promete.

### 2.2 Una carrera de datos es un error del programa, no un valor indefinido

Dos hilos accediendo a la misma posición sin sincronización, y al menos uno escribiendo, es un
**error del programa**. No "lee un valor viejo": el programa no tiene significado.

**Y aquí hay una decisión que se separa de C++ a propósito.** En C++ una carrera es comportamiento
indefinido y el optimizador puede razonar hacia atrás desde ahí. En NativeSharp la carrera es un
error del programa y **el compilador no la usa para deducir nada**: no elimina ramas ni asume
imposibilidades a partir de que "esto sería una carrera". El mismo motivo que el overflow definido
del doc 110 — el criterio del proyecto es que O0 y O2 se comporten igual, y razonar desde el UB es
el mecanismo que rompe eso.

Lo que se pierde con eso son optimizaciones que este compilador no hace hoy. Lo que se gana es que
una carrera dé un resultado raro y no un programa distinto.

### 2.3 Los cuatro ordenamientos, y cuál usa cada operación de hoy

| Ordenamiento | Qué promete |
|---|---|
| **relaxed** | atomicidad y nada más: ningún ordenamiento respecto de otros accesos |
| **acquire** | lo que venga después de esta lectura no se mueve antes de ella |
| **release** | lo que venga antes de esta escritura no se mueve después de ella |
| **acq_rel** | las dos, sobre una operación de lectura-modificación-escritura |
| **seq_cst** | además, un orden total único que todos los hilos ven igual |

**Lo que las operaciones de hoy garantizan, y esto es lo que hay que medir contra el IR emitido:**

- Todas las operaciones de `AtomicInt32`/`AtomicInt64` son **`seq_cst`**. Es lo que emiten los
  `Interlocked*` de Win32 (`lock xadd`, `lock cmpxchg` llevan barrera completa en x86-64) y es la
  única opción que no exige que el programador entienda ordenamientos para escribir código correcto.
- `Mutex`, `Monitor`, `Semaphore` y `Event`: **release al soltar, acquire al tomar**. Es lo que dan
  los SRW locks y las variables de condición de Win32.
- `Thread.Join`: **acquire** respecto de todo lo que el hilo unido escribió. Es lo que hace que
  «lanzar N hilos, unirlos, leer los resultados» sea correcto sin más sincronización — y es el
  patrón que un juego usa el 90 % de las veces.

**No se expone selección de ordenamiento por operación.** `AtomicInt32.Add(x)` es `seq_cst` y no hay
`AddRelaxed`. Cuando aparezca un consumidor con una medición que demuestre que el `seq_cst` cuesta —
un contador de estadísticas en un bucle caliente, típicamente — se añade la variante **con ese motivo
escrito**, no por completar la matriz.

### 2.4 Fences

No existen hoy y **no se añaden todavía**. Un fence sin ordenamientos por operación no tiene con qué
componer: si todo es `seq_cst`, un fence no añade nada. Entran junto con las variantes relajadas de
2.3, si entran, y por el mismo motivo medido.

### 2.5 `volatile`: **no es sincronización**

Si `volatile` entra en el lenguaje, su trabajo es exactamente uno: **impedir que el compilador
elimine, fusione o reordene respecto de sí mismos los accesos a una posición observable**. Para
registros de dispositivo y memoria mapeada, donde escribir dos veces el mismo valor no es redundante
y leer sin usar el resultado no es código muerto.

**Lo que `volatile` NO hace, y esto es la mitad importante del contrato:**

- **no ordena** respecto de accesos no volátiles
- **no ordena** respecto de otros hilos
- **no es atómico** — un `volatile long` en 32 bits se podría partir
- **no publica** nada: escribir un dato y luego una bandera volátil no garantiza que otro hilo que
  vea la bandera vea el dato

**La sincronización entre hilos se expresa con atómicos, fences, mutexes y monitores. Punto.** El
`volatile` de Java y el de C# tienen semántica de acquire/release, y eso es una decisión de esos
lenguajes que aquí **no se copia**: mezclar "no lo optimices" con "ordénalo" en una sola palabra es
lo que hace que la gente escriba código de sincronización con `volatile` y que funcione hasta que
cambia el compilador.

Si alguien necesita acquire/release, usa un atómico. Si necesita hablar con un dispositivo, usa
`volatile`. Son dos problemas y llevan dos herramientas.

**Consecuencia práctica:** ID 87 queda desbloqueado por este documento, pero su implementación sigue
sin consumidor. Un `volatile` sin memoria mapeada que tocar es una palabra clave que nadie usa. Se
implementa cuando aparezca el consumidor — MMIO, o un binding que lo necesite — y no antes.

---

## 3. Lo que hay que medir para cerrar M1.3

De nuevo: esto **no verifica el modelo**. Verifica que lo que se emite coincide con lo que el
contrato dice.

| Qué | Cómo | Si sale mal |
|---|---|---|
| ordenamiento de los `Interlocked*` en el IR | inspección: ¿llevan `seq_cst`? | el contrato miente en 2.3 |
| accesos ordinarios sin ordenamiento | inspección: nada de `atomic` donde no toca | — |
| `Thread.Join` como punto acquire | fixture: N hilos escriben, join, leer | si falla, 2.3 es falso y hay que arreglarlo o reescribirlo |
| mutex/monitor con release/acquire | fixture con dato + bandera bajo lock | — |
| que O0 y O2 no cambien el ordenamiento emitido | comparar el IR de los dos | divergencia = bug |

**El único que puede fallar de verdad es `Thread.Join`**, porque es el que el proyecto usa sin
pensarlo y el que nadie ha comprobado. Y es barato: lanzar ocho hilos que escriben en ocho
posiciones distintas, unirlos todos, leer las ocho. Si el contrato es cierto pasa siempre; si es
falso falla a veces, y "a veces" ya es información.

---

## 4. Lo que este contrato deja fuera a propósito

- **Ordenamiento por operación** — hasta que haya una medición que lo pida (2.3).
- **Fences** — no componen con nada mientras todo sea `seq_cst` (2.4).
- **`volatile` implementado** — desbloqueado, sin consumidor (2.5).
- **Un modelo formal** al estilo del de C++11 o del de Java, con relaciones happens-before escritas
  como axiomas. Lo que hay aquí es un contrato operativo: qué promete cada herramienta que existe.
  Un modelo formal sirve para razonar sobre optimizaciones agresivas del compilador, y este
  compilador no las hace. El día que las haga, este documento es el punto de partida y no el final.
