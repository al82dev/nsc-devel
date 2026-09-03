# 105 -- Bloque 4 cerrado: el driver mínimo

Fecha: 2026-09-02
Ámbito: solo el compilador (`A:\nsc-devel`).

## QUÉ es esto

El criterio de cierre del Bloque 4 estaba escrito en el doc 69 **antes de empezarlo**:

> un programa NativeSharp encuentra fuentes, las lee, escribe un binario y **llama al enlazador**

`tests/cc1/c26_driver_minimo` es ese programa. Hasta hoy el bloque estaba implementado y medido
por partes --c24 la E/S binaria de fichero, c25 el viaje de ida y vuelta de `ByteBuffer` /
`BinaryWriter` / `BinaryReader`-- pero **nadie había ejecutado nunca `Process.Run`**. Sus diez
argumentos de `CreateProcessA` existían sólo como IR.

Un fallo aquí con c24 y c25 en verde significa que las piezas están bien y la composición no. Ese
es el hueco que esta fixture tapa, y es la razón de que el criterio fuera un programa y no una
lista de comprobaciones.

## QUÉ hace, paso a paso

```
1  prepara un árbol de fuentes      File.WriteAllText  x3
2  las encuentra                    Directory.GetFileCount + Directory.GetFiles
3  las lee y las mide               File.ReadAllText, checksum por fichero
4  emite un objeto binario          ByteBuffer + BinaryWriter + File.WriteAllBytes
   y lo relee campo por campo       File.ReadBytes + BinaryReader
5  lanza un proceso hijo            Process.Run  -> código de salida Y salida de texto
6  lanza EL ENLAZADOR de verdad     Process.Run
7  limpia y devuelve 42
```

El objeto binario tiene forma de cabecera de objeto real: magia `0x4E533236` ("NS26"), versión,
número de fuentes, y por cada una un nombre en **24 bytes de ancho fijo** más longitud y checksum,
seguidos de un total y un centinela. Se relee entero y se compara contra lo calculado, incluido el
**relleno de ceros** del campo de nombre: si `WriteFixedAscii` dejara basura detrás, un lector que
sólo mirara los primeros bytes no lo vería nunca.

## Las tres decisiones que no son obvias

### 1. El hijo es el propio ejecutable, y no puede haber bomba de procesos

El paso 5 lanza **este mismo `.exe`** con `--hijo`. Es el único ejecutable que el programa sabe
seguro que existe, con un código de salida y una salida de texto que decide él mismo. Cualquier
herramienta externa haría que la prueba midiera la CLI de esa herramienta además de `Process.Run`.

Un programa que se lanza a sí mismo es una bomba de procesos si la condición de parada está mal.
Aquí **la parada no depende de leer bien un argumento, depende de cuántos hay**:

```csharp
if (cuantos >= 2)
{
    if (String.Equals(argumentos[1], "--hijo")) { Console.WriteLine("C26-HIJO-VIVO"); return 23; }
    return 98;
}
```

El padre sólo existe con `cuantos == 1`, y todo hijo recibe uno más. Un hijo **no puede** llegar al
paso 5 ni aunque no reconociera su propio argumento — por eso el `return 98` está ahí y no hay
ninguna rama que siga adelante con argumentos de más. Es una garantía estructural, no una
comprobación que haya que acertar.

Y no basta con el código de salida: se lee el fichero de salida y se exige el marcador
`C26-HIJO-VIVO`. Redirigir stdout es la mitad del contrato de `Process.Run`; sin comprobarlo, un
hijo que no imprimiera nada pasaría igual.

### 2. Del enlazador real se comprueba que arrancó, no su código de salida

Esta es la frase literal del criterio, y es distinta del paso 5: el paso 5 demuestra el mecanismo
con un hijo que controlamos, el 6 demuestra que el mecanismo sirve para **la herramienta real del
compilador**.

Se comprueba que el proceso **arrancó**, que se esperó a que terminara y que devolvió un código —
es decir, todo lo que `Process.Run` promete. **No** se exige un código de salida concreto, y es a
propósito: en la pasada 1 el enlazador es `lld-link` y en la pasada 2 es `nslink.exe`, dos
programas distintos con dos CLI distintas. Exigir un código concreto convertiría esta fixture en
una prueba de la línea de comandos del enlazador —y encima de uno que escribe otra persona— en vez
de una prueba de `Process.Run`.

**De dónde sale la ruta.** Un programa NativeSharp no sabe buscar en el PATH: no hay API para eso
todavía. Así que la da quien la conoce, el arnés. `run_cases.py` gana `linker_path()`, que **se lo
pregunta al propio compilador** con `nsc --print-linker` —que llama a `LldLinker::discover()`, la
misma función que usa para enlazar— y exporta el resultado como `NSC_CC1_LINKER`. En la pasada 2
ni hace falta: `nslink_pipeline.py` ya pone `NSC_LLD_LINK` apuntando a `nslink.exe`, que es el
segundo sitio donde c26 mira.

(La primera versión de `linker_path()` reimplementaba la búsqueda en Python y **no funcionó** en
la pasada 1. Lo que pasó y por qué, más abajo: «la cuarta copia de una regla».)

Resultado: **c26 lanza el mismo enlazador que acaba de enlazarlo a él.** lld-link en la pasada 1,
NSLink en la pasada 2.

Si no hay ninguno, avisa por su salida y **no falla**. Una fixture que se pusiera roja en una
máquina sin enlazador estaría midiendo la máquina.

### 3. El programa escribe sus propias entradas

Bajo `run_cases.py` el directorio de trabajo es la raíz del repositorio, pero bajo ctest
(`nsc.cc1.correctness`) no tiene por qué serlo, y una fixture que dependa del `cwd` falla en un
runner y pasa en el otro. Escribiendo lo que va a leer, mide la cadena entera sin depender de
dónde la lancen.

Los nombres salen de `args[0]`, igual que en c24 y c25: los 90 trabajos corren 16 en paralelo en
el mismo directorio, y con nombres fijos `-O0` y `-O2` de esta misma fixture se pisarían — un rojo
intermitente, que es la peor clase de prueba que hay.

El orden de `FindFirstFile`/`FindNextFile` no está garantizado por Win32, así que **nada** depende
de él: los registros se escriben en el orden en que llegaron y se comprueban contra ese mismo
array. La propiedad medida es que el viaje de ida y vuelta conserva lo escrito, no que el sistema
de archivos ordene alfabéticamente.

## Verificación hecha en el contenedor

El frontend real, con las bibliotecas reales:

```
./fe tests/cc1/c26_driver_minimo/c26_driver_minimo.nsproj --stage=llvm   ->  OK
./sweep.sh llvm                                        ->  OK=170  FAIL=5
```

Los 5 son los negativos intencionados de siempre (`ForeachNegativeTest`, `BorrowEscape`,
`BorrowedToOwned`, `ImplicitOwnedCopy`, `UseAfterMove`). El corpus pasa de 169 a 170.

Y lo que de verdad quería comprobar antes de tocar Windows — **que `Process.Run` baja a la
operación núcleo y no al método NativeSharp que devuelve `-1`**. En el IR emitido aparece:

```
__nativesharp_process_run
__nativesharp_file_read_bytes
__nativesharp_file_write_all_bytes
__nativesharp_file_get_size
__nativesharp_directory_get_files
__nativesharp_directory_get_file_count
__nativesharp_environment_get_variable
```

y **no** aparece ningún `__nsc_method_...Process__Run`. Ese es el bug de las dos listas de
`ProjectOopHirModel.cpp` (doc 101): estar sólo en la primera deja la llamada en el método normal,
que devuelve `-1`, y el programa daría una respuesta plausible y falsa. Aquí se ve que no pasa.

Lo que el contenedor **no** puede verificar: que `CreateProcessA` lance de verdad. Eso lo mide
`all.bat` en Windows, y es la primera vez.

## Un error primerizo, para que no se repita

`raii string comillas = "\"";` no compila:

```
RAII ownership cannot be copied or borrowed; initialize with new, null, move(other),
or an owned-returning call
```

Un literal de cadena **no** es una llamada que devuelva `owned`, así que la local no puede ser
`raii`. `raii` es para lo que devuelve `String.Concat`, `Path.Combine`,
`Path.GetFileNameWithoutExtension` — lo que asigna. Un literal es un `string` normal.

El diagnóstico es correcto y no dice la línea, que es lo que costó el minuto.

## Registrado en los tres sitios

| Dónde | Qué hace con c26 |
|---|---|
| `run_cases.py` | Lo **ejecuta**. Espera 42. Le pasa `NSC_CC1_LINKER` |
| `nslink_pipeline.py` | Lo ejecuta en la pasada de NSLink (tabla propia, ver abajo) |
| `tests/unit/Cc1CorrectnessTests.cpp` | Sólo **compila y enlaza**: `checkFixture` ejecuta bajo Wine, que en Windows no está |

Quien de verdad mide `Process.Run` es `run_cases.py`. ctest comprueba que el programa se construye.

**Las dos tablas de casos siguen duplicadas** —`run_cases.py` y `nslink_pipeline.py` tienen cada
una la suya— y esa es la misma enfermedad de los docs 102 y 103. No se ha unificado hoy para no
mezclar dos cambios, pero hay una red que ya existe: la sección 4 de `COMPARACION_CON_LLD.txt`,
«FALLA Y NO ESTÁ EN LA REFERENCIA», está escrita exactamente para cuando las dos tablas se
separen. Comprobado a mano que hoy son idénticas, caso por caso: 45 y 45.

De paso, las menciones a «los 84 trabajos» en `all.bat`, `nslink_solo.bat` y `nslink_pipeline.py`
pasan a «los trabajos del corpus». Eran prosa, no lógica —los contadores se calculan— pero un
número escrito a mano en un comentario es exactamente lo que se queda viejo, y esta ronda ya ha
costado dos builds por números escritos a mano.

## Números esperados en la próxima corrida

```
ctest       245   (c26 entra como caso nuevo dentro de nsc.cc1.correctness, que ya existia:
                   244 -> 244, el conteo de ctest NO cambia)
trabajos     90   (45 casos x 2, eran 88)
triage    16/16
```

Corrección sobre la línea de arriba: **ctest sigue en 244**. c26 se añade *dentro* de
`nsc.cc1.correctness`, que es una sola prueba de ctest; lo que crece es el número de
comprobaciones dentro de ella, no el número de pruebas.

## Lo que midió la corrida, y la cuarta copia de una regla

La pasada 1 y la pasada 2 salieron **verdes las dos**:

```
Passed: 92   Failed: 0
ctest:  100% tests passed out of 244
cases:  90 OK, 0 FAIL
build:  limpio con /WX     triage: 16/16
[ OK ] cc1_c26_driver_minimo -O0 -- exit 42
[ OK ] cc1_c26_driver_minimo -O2 -- exit 42
```

Pero el log del caso dice dos cosas distintas según la pasada. En la **pasada 2**, con NSLink:

```
c26: enlazador lanzado, codigo 2
[exit 42]
```

Eso es el criterio del doc 69 cumplido literalmente: un programa NativeSharp lanzó `nslink.exe`,
esperó a que terminara y leyó su código de salida. **Primera vez que `CreateProcessA` se ejecuta
en este proyecto**, y funcionó.

En la **pasada 1**, con lld-link:

```
c26 AVISO: sin NSC_CC1_LINKER ni NSC_LLD_LINK, no se lanzo el enlazador
[exit 42]
```

**El paso 6 no se ejecutó, y la corrida salió verde igual.**

### Por qué

`linker_path()`, en `run_cases.py`, reimplementaba la búsqueda en Python: `NSC_LLD_LINK` y si no
`shutil.which("lld-link")`. Escribí en este mismo documento que era *"la MISMA regla que
findLldLink()"*. No lo era: en esa corrida `nsc` encontró el enlazador y enlazó los 90 trabajos
sin una queja, y `shutil.which` no encontró nada.

Es la **cuarta copia de una regla que se desincroniza en silencio** en esta ronda —después de las
62 entradas de KERNEL32 (doc 102), de las tres copias en las pruebas de host (doc 103) y de las
dos tablas de casos. Y esta vez el síntoma fue el peor de los cuatro: no un rojo, sino un **verde
que no medía lo que decía medir**.

### El arreglo

`nsc --print-linker` **ya existía**, y llama a `LldLinker::discover()`, que es *la* función.
`linker_path()` deja de buscar y se lo pregunta:

```python
probe = subprocess.run([nsc, "--print-linker"], ...)
for line in probe.stdout.splitlines():
    if line.startswith("linker:"):
        ...
```

Por construcción no se puede desincronizar: c26 lanza exactamente el enlazador que `nsc` usaría.
Se resuelve **una vez** en `main()`, no dentro de los 90 hilos.

Y se **imprime**:

```
[CASOS] enlazador para c26_driver_minimo: <ruta>
```

o, si no hay:

```
[CASOS] AVISO: `nsc --print-linker` no devolvio ningun enlazador.
[CASOS] c26_driver_minimo NO va a lanzar el enlazador -- el paso 6 de su
[CASOS] criterio de cierre se saltara, y el caso saldra verde igualmente.
```

Esa es la otra mitad del arreglo, y la más importante: el aviso estaba, pero enterrado en el log
de un caso entre noventa. **Un aviso que hay que ir a buscar no es un aviso.** Ahora sale en la
salida del paso, que es lo que se lee.

### Lo que sigue sin poderse comprobar solo

Que el fixture *deba* lanzar un enlazador es algo que el fixture no puede saber: si no hay
ninguno en la máquina, ponerse rojo sería medir la máquina. Lo que sí se puede es no dejarlo
callado, y eso es lo que ahora hace el arnés.

---

## Con esto, el Bloque 4 queda cerrado

Fases A, B y C implementadas, medidas y en verde con los dos enlazadores (docs 100, 101, 103). La
fase D --`NativeLibrary`, SHR-035-- **no existe**: salió del roadmap el 2026-09-02 y el motivo y
sus cuatro condiciones de reapertura están en el doc 99 §1. Y el criterio de cierre del doc 69,
que era lo único que faltaba, es esta fixture.
