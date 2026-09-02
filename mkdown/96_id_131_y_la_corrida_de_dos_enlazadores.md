# 96 — ID 131 cerrado, y la corrida de dos enlazadores que por fin midió algo

**Fecha:** 2026-09-02
**Qué cierra:** el hueco del lenguaje que bloqueaba el Bloque 4, y la infraestructura que lo mide.
**Lo que hay que llevarse de aquí, si sólo se lee una línea:** correr *media* batería con el
segundo enlazador habría dado un verde falso. Se vio porque se corrió **entera**.

---

## 1. ID 131 — miembros estáticos de un tipo genérico cerrado

`Caja<int>.Make(7)` no parseaba. El diagnóstico no lo decía:

```
expected expression
expected expression
expected ';', found 'Make'
```

Ninguno menciona genéricos. Se descubrió implementando el Bloque 3, con `Span<int>.Of(items)`, y
el bloque salió adelante **rodeándolo**: las factorías se metieron en clases no genéricas
(`Spans.Of`, `Optionals.Some`). Ese rodeo no llegaba al Bloque 4, que es genérico de punta a punta
—`ByteBuffer`, `BinaryReader`, `BinaryWriter`, las variantes de bytes, todo sobre `Span<T>`— y
habría acabado con una clase-sombra no genérica por cada tipo.

### 1.1 Eran dos sitios, no uno

**En una expresión.** `looksLikeGenericCallTypeArguments` acepta una lista de argumentos de tipo
sólo si tras el `>` de cierre viene un `(`. Por eso `Ayuda.Envolver<int>(1)` —argumentos de tipo de
un **método**— entraba, y `Caja<int>.Make(7)` —argumentos de tipo de un **tipo**— no: detrás del
`>` viene un `.`.

**Al principio de una sentencia.** `Caja<int>.Cuenta = 5;` empieza igual que la declaración
`Caja<int> c = ...`, así que `canStartTypeAt` decía «declaración» y el parser terminaba pidiendo un
nombre de variable:

```
expected variable name, found '.'
```

Este segundo caso no se vio hasta escribir la prueba con más formas que la mínima. La primera
versión del arreglo pasaba `Caja<int>.Make(7)` y seguía rompiendo `Caja<int>.Cuenta = 5;`.

### 1.2 Por qué un predicado aparte y no un token más en el que ya existía

Porque las dos formas **no producen lo mismo**. Tras `Envolver<int>` viene una llamada y los
argumentos de tipo se cuelgan de la `CallExpression`; tras `Caja<int>` viene un **tipo cerrado** del
que se accede a un miembro, y lo que hace falta ahí no es una lista de argumentos suelta sino un
`TypeSyntax` completo — el mismo que `new Caja<int>()` y `Caja<int> c` ya construyen. Meter los dos
casos en un predicado habría obligado al bucle de sufijos a adivinar cuál de las dos formas tenía
delante.

`looksLikeClosedGenericTypeReference` reutiliza `canAppearInTypeArgumentList`, que es la lista
blanca que ya evita comprometerse con una expresión que sólo tenía comparaciones dentro (ver el
comentario de `looksLikeGenericCallTypeArguments`, y el defecto que lo motivó). El conjunto que
acepta tras el `>` es **uno solo**: `.` seguido de identificador, que es la única forma nueva que se
admite y que no puede empezar ninguna declaración.

### 1.3 La parte que importa de verdad: se resuelve por CLAVE, no por nombre

El parser deja el `TypeSyntax` del tipo cerrado colgado del identificador. El modelo de cuerpos lo
resuelve con **`canonicalTypeKey`** —el mismo servicio que usan `new` y la declaración de un
local— y no con `resolveTypeIdentifier`, que busca por `identity.name()`.

La diferencia no es de estilo. Para un genérico, `identity.name()` es el nombre **abierto**, así que
resolver por nombre habría devuelto la declaración de `Caja<T>` donde se escribió `Caja<int>`, y
todo lo deducido del miembro habría sido de otro tipo. **Un compilador que acepta el programa y le
pone el tipo equivocado es peor que uno que lo rechaza**, porque el error no aparece hasta que
alguien lee el IR.

Eso es lo que mide la comprobación central de la prueba: `Caja<string> mal = Caja<int>.Make(7);`
tiene que ser **rechazado**, y rechazado *por el tipo*, no porque el parser no supiera leerlo.

Si la monomorfización no existe —se escribió un tipo cerrado que nadie más usa— no se resuelve nada
y el camino de siempre da su diagnóstico. Es lo correcto: instanciar desde un acceso a miembro
sería monomorfizar en el sitio equivocado.

### 1.4 Cómo se comprobó

`tests/unit/Stage59ClosedGenericMemberTests.cpp`, 12 comprobaciones. Seis son formas nuevas
—llamada estática, campo estático como sentencia y dentro de una expresión, dos parámetros de tipo,
genérico anidado (donde el `>>` cuenta como dos niveles), y encadenado—. Cuatro son **lo que ya
funcionaba y no puede romperse**, que es la mitad importante: el método genérico de siempre, dos
comparaciones seguidas, la forma exacta que rompió la versión anterior del otro predicado
(`wide < (int)X || wide > (int)X`), y la declaración de un local genérico. La desambiguación entre
«menor que» y «lista de argumentos de tipo» es justo donde un arreglo de esto se lleva por delante
la aritmética de comparaciones sin que nadie lo note.

Barrido del contenedor sobre los 172 `.nsproj` del árbol, en `bodies` y en `llvm`: **167 OK, 5
FAIL** — la línea base exacta, y los cinco son los negativos intencionados.

### 1.5 ID 132 sigue abierto, y ahora se sabe cuánto cuesta

Los métodos genéricos se identifican por `nombre``aridad`:

```cpp
record.canonicalKey = record.moduleName + "::" + qualifiedName(currentNamespace, ownerName) +
    "." + record.name + "``" + std::to_string(record.parameters.size());
```

Dos plantillas con la misma aridad y distinta lista de parámetros chocan con *«duplicate generic
template identity»*. Arreglarlo pide **meter la firma en la clave y resolución de sobrecargas entre
plantillas en el punto de llamada** — y esa clave se usa además para nombrar la monomorfización
(`canonicalKey<args>`), así que el cambio llega hasta los nombres decorados. No es un vecino del
131: es su propia tarea. **No bloquea el Bloque 4**: renombrar la variante es una decisión de API
perfectamente normal, y es lo que se hizo en el Bloque 3 (`Of` → `OfRange`).

---

## 2. La corrida de dos enlazadores: qué midió, por fin

| | ctest | 84 trabajos | triage | manifiesto |
|---|---|---|---|---|
| **lld-link** | **243/243** | 84 OK | OK | OK |
| **NSLink** | **238/243** | 84 OK | OK | OK |

**nsc está verde.** Los cinco fallos son de NSLink y **son uno solo**: todos cuelgan de la
biblioteca de importación.

```
nsc.stage46.native-libraries   DLL generation creates the matching COFF import .lib
nsc.stage47.host-c-abi         8 checks: C y C++ no enlazan contra ese .lib
nsc.stage48_1.c-header         4 checks
nsc.stage48_3.c-host           2 checks
nsc.stage48_4.cpp-host         3 checks
```

El primero produce el `.lib`; los otros cuatro intentan enlazar contra él. Un defecto en el camino
`/dll /noentry /def: /implib:` explica los dieciocho *checks*.

**Y esto es lo que no se podía ver antes.** Los 84 trabajos pasan con los **dos** enlazadores. Si
la segunda pasada hubiera sido «las fixtures otra vez» —que es como estaba diseñada— el reporte
habría dicho *84 OK, 0 regresiones* y NSLink habría quedado en verde con un defecto real dentro.

---

## 3. Tres defectos de la infraestructura, y los tres eran míos

### 3.1 `CMAKE_CXX_COMPILER_WORKS` no existe en este árbol

`run_windows_checklist.bat` decidía si `build-clang\` servía mirando si la caché tenía
`CMAKE_CXX_COMPILER_WORKS:INTERNAL=1`. **Ese marcador no aparece nunca aquí**, porque CMake omite la
prueba del compilador — el log lo dice literalmente:

```
-- Check for working CXX compiler: C:/clang-llvm/bin/clang-cl.exe - skipped
-- Configuring done (1.2s)
-- Generating done (0.3s)
```

Resultado: el bloque daba positivo en **todas** las corridas, borraba un árbol sano y convertía cada
build incremental en una reconstrucción completa. Venía de antes de esta ronda y nadie lo había
notado porque el mensaje suena a diagnóstico correcto. La señal buena es que **exista
`build.ninja`**, que lo escribe el `Generating done` — el último paso de la configuración.

**La lección:** el marcador no faltaba porque algo fallara; no existía nunca. Una comprobación cuyo
positivo no se puede distinguir de su caso normal no es una comprobación.

### 3.2 `run_compare` no existía

`all.bat` llamaba a `python nslink_pipeline.py --compare`. Ni el flag estaba declarado en el
argparse ni la función existía: la corrida moría con `NameError` **después** de las dos pasadas, con
todo el trabajo hecho y el buzón sin generar. Y como `all.bat` lee el código de salida, imprimía
`Buzon de NSLink : generado, con regresiones` — o sea que el fallo se disfrazaba de resultado.

Lo di por escrito en una sesión anterior y nunca lo ejecuté. **Escribir la llamada no es escribir la
función**, y un `.py` compila igual con un nombre que no existe.

### 3.3 La comparación miraba media superficie

`--compare` leía sólo `cases\results.json`, o sea los 84 trabajos. Con los números de arriba habría
dicho **«0 regresiones»** con las cinco de ctest en rojo. Ahora lee también `02_ctest.log` de las
dos pasadas, separa las que ya fallaban con lld-link de las que sólo fallan con NSLink, y el código
de salida cuenta las dos superficies.

---

## 4. La forma final de `all.bat`, y por qué

```
PASADA 1   lld-link     build-clang-lldlink\  --->  _logs_lldlink\    OFICIAL
PASADA 2   nslink.exe   build-clang-nslink\   --->  _logs_nslink\
```

**Dos árboles de build y dos directorios de logs, desde el primer byte.** Es lo que hace simple todo
lo demás. Con un solo `build-clang\` había que elegir entre reconstruirlo en la pasada 2 —y entonces
los dos reportes medían compiladores distintos sin decirlo— o no reconstruirlo, y entonces la pasada
2 dependía de que la 1 hubiera dejado el árbol sano. Con dos árboles no hay que elegir. Que las dos
compilen las mismas fuentes por separado no es desperdicio: si algún día los dos `nsc.exe` no se
comportan igual, eso es un fallo de reproducibilidad y esta corrida es donde tiene que salir.

Las variables que lo hacen posible son dos, y sin ellas cada script se comporta como antes:

| Variable | La leen |
|---|---|
| `NSC_BUILD_DIR` | `run_windows_checklist.bat` |
| `NSC_LOG_DIR` | `run_windows_checklist.bat`, `run_bug_triage.bat`, `run_cases.py`, `report.py` |

**Las dos pasadas se corren siempre**, aunque la primera salga en rojo: los dos reportes juntos son
lo que dice de quién es el fallo, y tener sólo uno es justo lo que no sirve.

### 4.1 No se reescribe `all.bat` mientras corre

cmd.exe lee un `.bat` **por offset de byte** mientras lo ejecuta. Lo reescribí con la corrida en
marcha; al volver del `call` de la pasada 2, cmd retomó ~1800 bytes desplazado, imprimió
`'######...' is not recognized as an internal or external command`, **saltó a la limpieza del
principio —que borra los directorios de logs— y volvió a llamar al checklist**. Las dos pasadas de
esa corrida se perdieron enteras.

Está escrito en la cabecera del propio archivo. La regla no admite excepciones: mientras `all.bat`
corre, no se toca ningún archivo del repo.

### 4.2 Los logs de la pasada con NSLink van dentro del buzón

`_inbox_nsc\logs-nsc-tests-with-nslink\` — la corrida entera, no un resumen: los ocho logs, el
`TRIAGE`, el `REPORTE`, la comparación y `cases\` con un fichero por trabajo. No van `bin\` ni
`linktemps\`, que son cientos de megas por corrida y de los que lo útil ya va al buzón elegido caso
por caso.

**Dentro del buzón y no en un directorio hermano**, y el motivo es de ellos: `_inbox_nsc` es el
único sitio de su árbol donde escribe nsc, no forma parte de su construcción y no entra en su
manifiesto (su decisión D-058). Un segundo directorio suelto tendría que ganarse esas tres cosas
otra vez, y por nada.

---

## 5. Qué se tocó

| Archivo | Qué |
|---|---|
| `include/nativesharp/Parser/Parser.h` | `looksLikeClosedGenericTypeReference` |
| `src/Parser/Parser.cpp` | El predicado, su uso en el bucle de sufijos, y la desambiguación al principio de una sentencia |
| `src/Frontend/ProjectMethodBodyModel.cpp` | El receptor se resuelve por clave de tipo cuando lleva argumentos |
| `tests/unit/Stage59ClosedGenericMemberTests.cpp` | **Nuevo.** 12 comprobaciones |
| `CMakeLists.txt` | `nsc.stage59.closed-generic-member` — ctest pasa de 243 a 244 |
| `all.bat` | Reescrito: dos árboles, dos directorios, las dos pasadas siempre |
| `run_windows_checklist.bat` | `NSC_BUILD_DIR`, `NSC_LOG_DIR`, y el marcador de configuración completa |
| `run_bug_triage.bat`, `run_cases.py`, `report.py` | `NSC_LOG_DIR` |
| `nslink_pipeline.py` | `run_compare` (que no existía), la comparación de ctest, la copia de logs al buzón |
| `verify_manifest.py`, `.gitignore` | Los directorios nuevos fuera del manifiesto y de git |

---

## 6. Lo que queda antes del Bloque 4

**Nada que bloquee.** ID 131 era el tapón y está cerrado; ID 132 no bloquea. Queda pendiente correr
`all.bat` con la prueba nueva dentro para ver los 244 de ctest en verde — el barrido del contenedor
ya dice que el frontend no tiene regresiones, pero eso no enlaza ni ejecuta.

Sigue abierto y es **decisión suya, no ingeniería**: SHR-063 (condicionales de compilación, que
bloquea al Bloque 4 sólo si el driver necesita distinguir configuraciones), ID 87 (`volatile`),
ID 125 (UTF-8), ID 124. Y sin tocar: ID 129 (enums por nombre) e ID 130 (`uwtable`, medido y no
arreglado a propósito — cambiar los atributos de cada función pide su propia medición en Windows).
