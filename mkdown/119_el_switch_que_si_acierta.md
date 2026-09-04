# 119. El `switch` que si acierta, y la bateria que medía el compilador equivocado

Dos cosas distintas cerradas el 2026-09-04. No comparten causa; comparten que las dos salieron
de la misma corrida y que las dos eran el mismo tipo de error: **medir una cosa creyendo que se
medía otra**.

---

## Parte 1 -- `ToString()` de un enum estrecho y negativo a -O2

### 1.1 El síntoma, y por qué costó tanto

`tests/cc1/c38_enum_ancho` devuelve 42 a -O0 y **39** a -O2. El 39 es esta línea:

```csharp
if (OpacoConSigno(ConSigno.MenosTres).ToString() != "MenosTres") { return 39; }
```

con `public enum ConSigno : sbyte { MenosTres = -3, Cero = 0, Tope = 127 }`.

La sonda `tests/bug_triage/b9_enum_estrecho_o2` devuelve **61**. En su tabla de códigos, 61
significa: `ConSigno.MenosTres.ToString()` devolvió **exactamente `"ConSigno"`**, el nombre del
tipo, o sea la cadena de RESERVA.

Los dos son el mismo defecto visto por dos caminos: c38 con receptor opaco, b9 con receptor
constante.

### 1.2 Lo que las capas de b9 descartan, una por una

b9 está construido en capas justamente para esto, y cada capa que pasa elimina una hipótesis:

| capa | qué mide | resultado | qué elimina |
|---|---|---|---|
| control literal (201-239) | clasificar el literal `"MenosTres"` sin enums de por medio | pasa | la indexación de cadenas y `Length` funcionan |
| 1 (10-16) | el valor de 8 bits antes de `ToString()` | pasa | el valor **sigue siendo -3** al llegar |
| 2 (60+d) | qué cadena llegó, byte a byte, sin `MismosBytes` ni `!=` | **61** | -- |
| 2b (240-243) | `MismosBytes` sobre bytes correctos | no se llega | -- |
| 3 (30-36) | el operador `!=` de cadenas | no se llega | -- |

O sea: el valor es correcto, la lectura de cadenas es correcta, y aun así `EnumName` produce la
reserva. El defecto está en la producción del nombre y en ningún sitio más abajo.

### 1.3 La discriminación que decidió el arreglo

Dentro del MISMO fichero c38, al MISMO nivel de optimización y con el MISMO compilador:

- **código 26 PASA.** Es `ClasificaConSigno(OpacoConSigno(ConSigno.MenosTres)) != 11`, y
  `ClasificaConSigno` es un `switch` del lenguaje sobre ese mismo enum con ese mismo -3. En el IR:

  ```llvm
  switch i8 %param.load, label %switch.end.4 [
    i8 -3, label %switch.case.1
    i8 0,  label %switch.case.2
    i8 127, label %switch.case.3
  ]
  ```

- **código 39 FALLA.** Es `ToString()`, que hasta hoy bajaba a una cadena de `select`:

  ```llvm
  %enum.name.is186 = icmp eq i8 %static.call185, 127
  %enum.name187   = select i1 %enum.name.is186, ptr @str_Tope, ptr @str_ConSigno
  %enum.name.is188 = icmp eq i8 %static.call185, 0
  %enum.name189   = select i1 %enum.name.is188, ptr @str_Cero, ptr %enum.name187
  %enum.name.is190 = icmp eq i8 %static.call185, -3
  %enum.name191   = select i1 %enum.name.is190, ptr @str_MenosTres, ptr %enum.name189
  ```

**El mismo valor estrecho y negativo se compara bien dentro de un `switch` y mal dentro de la
cadena de `select`.** Esa es la única discriminación que existe, y está medida en la máquina que
falla, no inferida.

Los vecinos confirman el recorte por el otro lado, y son los que descartan las explicaciones
fáciles:

| caso | tipo | valor | resultado | qué descarta |
|---|---|---|---|---|
| c38 37/38 | `enum : byte` | 200, 128 (en i8: -56, -128) | pasa | no es "un enum de un byte" |
| c38 40/41 | `long`, `int` | 5000000000, 100000 | pasa | no es el ancho grande |
| c38 48/49 | `ulong`, `long` | 2^64-1, -2^63 | pasa | **no es el signo por sí solo**: `Suelo.Min` es negativo y acierta |
| c38 50-53 | `enum : byte` | 7, 7, 9 (alias) | pasa | no es el alias ni la cadena larga |
| c38 39, b9 61 | `enum : sbyte` | -3 | **falla** | queda: estrecho **y** negativo |

### 1.4 Lo que el contenedor demuestra, y lo que no puede demostrar

El IR que emite el compilador es **correcto**. Se comprobó pasando el IR de c38 --emitido por el
mismo código fuente, verificado byte a byte contra `MANIFEST_SHA256.txt`-- por `opt -O2` de
**LLVM 18**: la función `Main` se pliega hasta

```llvm
%common.ret.op = phi i32 [ 34, ... ], [ 35, ... ], [ 36, ... ], [ 42, %if.end.257 ]
```

es decir, 42 y ningún 39. Las tres salidas 34/35/36 son las trampas de índice de arreglo, que no
se toman.

El compilador que se entrega enlaza **LLVM 23**. En este contenedor no hay LLVM 23 y no hay red
para instalarlo, así que **no se puede demostrar qué transformación de LLVM 23 rompe la cadena**.

Esto hay que decirlo entero: **el arreglo de abajo es un rodeo apoyado en una medición, no la
reparación de una causa identificada.** El sitio a mirar, si algún día se puede depurar contra
LLVM 23, sigue siendo el plegado de una cadena de `select` sobre punteros gobernada por un `icmp`
de ocho bits con una constante negativa.

### 1.5 El cambio

`src/LLVM/ProjectOopLlvmLowerer.cpp`, `lowerEnumName`. La bajada pasa de cadena de `select` a
`switch` + `phi` -- exactamente la forma que la máquina que falla demuestra que acierta:

```llvm
switch i8 %v, label %enum.name.fallback [
  i8 -3,  label %enum.name.case.0
  i8 0,   label %enum.name.case.1
  i8 127, label %enum.name.case.2
]
...
%enum.name = phi ptr [ @str_ConSigno, %fallback ], [ @str_MenosTres, %case.0 ],
                     [ @str_Cero, %case.1 ], [ @str_Tope, %case.2 ]
```

Lo que se conserva:

- **el receptor se baja UNA sola vez**, que es la propiedad por la que este nodo existe:
  `Dame().ToString()` no puede llamar a `Dame()` una vez por enumerador;
- la reserva sigue siendo el nombre del tipo, ahora como bloque `default`;
- no se añade ni un símbolo a `src/LLVM/LlvmCApi.h`. `LLVMBuildSwitch`, `LLVMAddCase`,
  `LLVMBuildPhi`, `LLVMAddIncoming`, `LLVMGetInsertBlock` y `appendBlock` ya se usaban en este
  mismo fichero (la lista de 146 sigue congelada, SHR-090).

Lo que se pierde: la propiedad de "sin saltos". Era una preferencia de diseño escrita en un
comentario, no un requisito de corrección, y un `switch` sin efectos ni llamadas es lo que un
compilador de C emite para esto de todas formas.

### 1.6 Lo que se gana de paso: el alias deja de depender del orden del bucle

Un `switch` de LLVM **no admite dos casos con el mismo valor** -- el verificador lo rechaza. Así
que los pares se deduplican en la bajada y **gana el primero declarado**, que es la regla que
documenta `ProjectOopHirModel` y que fija la fixture c34: `Solo = 1` declarado después de
`Lectura = 1` tiene que salir `"Lectura"`.

Antes esa regla se conseguía recorriendo los pares **al revés**, para que el primer par quedara
en el `select` de más afuera. Era correcto y ya se invirtió una vez por accidente: construida en
orden directo, ganaba el ÚLTIMO. Ahora la elección entre iguales está escrita como lo que es, y
no como un efecto del anidamiento.

Se comprueba en el IR de c38 con `ConAlias : byte { Primero = 7, Segundo = 7, Otro = 9 }`:

```llvm
switch i8 %v, label %enum.name.fallback.284 [
  i8 7, label %enum.name.case.285
  i8 9, label %enum.name.case.286
]
%enum.name317 = phi ptr [ @__nsc_str_23, %fallback ], [ @__nsc_str_24, %case.285 ], ...
                              ; str_23 = "ConAlias"      str_24 = "Primero"
```

Dos casos, no tres, y el 7 va a `"Primero"`. `"Segundo"` ni siquiera llega a tener global.

**La deduplicación es por PATRÓN DE BITS, no por el número escrito**, enmascarando al ancho del
tipo subyacente. Es el mismo ancho con el que `IntegerConstant` construye cada constante
(`LLVMConstInt` trunca al ancho del tipo). Deduplicar por el valor de 64 bits sin enmascarar
dejaría pasar un par que colisiona al truncar, y eso no sería un nombre equivocado: sería un
`switch` con dos casos iguales, o sea IR inválido.

### 1.7 Lo verificado en el contenedor

Contra el árbol de fuentes comprobado idéntico al del disco (SHA-256 de las 396 rutas de `src/`,
`include/` y `lib/` del manifiesto; la única diferencia era `src/Linker/WindowsRuntimeImports.cpp`,
que es del carril de NSLink y no entra aquí):

- compila sin avisos con `-Wall -Wextra`;
- `opt -passes=verify` acepta el módulo de c38;
- `opt -O2` de LLVM 18 sigue plegando `Main` de c38 hasta 42 -- sin regresión;
- b9 y c39 compilan y verifican a -O2;
- barrido completo: **185 OK, 5 FAIL**, exactamente la línea base, y los cinco fallos son los
  negativos intencionados de siempre (`ForeachNegativeTest`, `BorrowEscape`, `BorrowedToOwned`,
  `ImplicitOwnedCopy`, `UseAfterMove`).

Lo que **no** está verificado y solo lo puede decir una corrida en Windows: si LLVM 23 deja de
devolver la reserva. Hasta entonces c38 y b9 siguen siendo la prueba, y b9 sigue vivo.

### 1.8 Una alternativa que se consideró y no se tomó

Se propuso comparar los enums en `i64`, extendiéndolos según el signo del tipo subyacente. Es
semánticamente válida y no se descarta por mala; se descartó porque **ataca solo la mitad de la
forma medida**. La discriminación de 1.3 empareja dos cosas --`icmp` estrecho **y** cadena de
`select` sobre punteros--, y ensanchar la comparación deja la cadena de `select` en pie. Su
evidencia (la capa 1 de b9 demuestra que `(long)` de un `sbyte` -3 funciona a -O2) no aísla la
misma variable: esa capa no alimenta ningún `select`.

Entre dos rodeos, se eligió el que reproduce una forma **medida como buena en la máquina que
falla**, no una forma nueva que nadie ha ejecutado allí.

---

## Parte 2 -- 116 tests medían el compilador del árbol de build

### 2.1 El síntoma

`ctest`: 283 tests, 116 en `***Not Run`, todos con

```
Could not find executable A:/nsc-devel/build-clang/bin/nsc.exe
```

### 2.2 La causa, que no es el fichero ausente

Los 116 estaban escritos como `add_test(... COMMAND nsc ...)`, o sea invocando el **objetivo**
`nsc`. CMake resuelve un objetivo al binario del **árbol de build**. Eso ata la batería a un
artefacto intermedio en vez de al que se entrega: `cmake --install` copia ese binario a
`C:\native-sharp\bin`, y entre los dos caben una instalación que falló, un install que se saltó,
o un árbol de build rehecho después.

Salió a la superficie porque `test_nsc.bat` construye el objetivo `pruebas`, que **excluye `nsc`
a propósito** -- para no cambiar el compilador que se está midiendo. Con `build-clang\bin\nsc.exe`
ausente, el sitio equivocado quedó vacío y se vio. El defecto llevaba ahí desde el principio.

Y era el único consumidor que quedaba apuntando ahí: `run_cases.py`, `nslink_pipeline.py` y
`run_bug_triage.bat` ya usaban `C:\native-sharp\bin\nsc.exe`.

### 2.3 El cambio

`CMakeLists.txt`: una variable de caché `NSC_TEST_COMPILER`, y los 116 `COMMAND nsc` pasan a
`COMMAND "${NSC_TEST_COMPILER}"`.

- por omisión, en Windows: `C:/native-sharp/bin/nsc.exe`;
- fuera de Windows no hay `C:\`, así que vuelve a ser `$<TARGET_FILE:nsc>`;
- `-DNSC_TEST_COMPILER=<ruta>` manda sobre las dos.

**Es una ruta y no un objetivo, y eso es deliberado.** Mientras se escriba el nombre del objetivo,
CMake tiene que resolverlo al árbol de build por definición: no existe forma de que `COMMAND nsc`
signifique "el instalado".

Si la ruta no existe al configurar, se **avisa** y no se aborta: en un árbol recién clonado el
configure ocurre ANTES del primer install, y un `FATAL_ERROR` haría imposible la primera
construcción.

Verificado generando el árbol de ctest con `-DNSC_TEST_COMPILER=C:/native-sharp/bin/nsc.exe`:
283 `add_test`, **116 apuntando al compilador instalado, 0 al árbol de build**.

### 2.4 La contrapartida, dicha entera

`ctest` deja de garantizar que ese ejecutable esté al día. Esa garantía se mueve a quien lanza la
corrida, y hay dos sitios que la dan:

- `all.bat` instala en el paso 1, justo antes de ctest: ahí es fresco por construcción;
- `test_nsc.bat` no instala --es su promesa-- y por eso ahora compara la fecha del ejecutable con
  la de las fuentes (`build_al_dia.py --solo-nsc`) y **avisa sin abortar**. Medir a propósito una
  versión anterior es un uso legítimo; medirla sin saberlo no.

Dentro de la propia batería queda `nsc.version`, el primero de los 116: comprueba versión y stage
contra un regex, así que un compilador de otra stage se pone rojo ahí.

### 2.5 Un falso positivo que había que quitar antes

`build_al_dia.py` comparaba también la fecha de `build-clang/build.ninja`. Ninja solo lo regenera
cuando cambia `CMakeLists.txt`, así que su fecha se queda en el último configure y no se mueve con
las fuentes:

```
configure          -> build.ninja = T0
editar src/Foo.cpp -> Foo.cpp     = T1 > T0
build_nsc.bat      -> nsc.exe     = T2 > T1
```

El más antiguo era `build.ninja` (T0), `Foo.cpp` (T1) le ganaba, y el veredicto era "hay que
construir" con el compilador **recién construido**. Dentro de `build_nsc.bat` eso costaba segundos
--ninja no rehace lo que no cambió-- y por eso pasó desapercibido. Como aviso permanente en
`test_nsc.bat` habría sido un aviso que salta siempre, y un aviso que salta siempre no lo lee
nadie.

Ahora se separa **existir** de **estar al día**: `build.ninja` se comprueba por existencia (si el
árbol no está, no hay nada que reaprovechar) y no por fecha. Verificado con cuatro escenarios
sintéticos: recién construido con `build.ninja` viejo -> al día; fuente editada después ->
construir; árbol ausente -> construir; ejecutable de pruebas ausente -> `--solo-nsc` al día y
completo no.

---

## Ficheros tocados

```
src/LLVM/ProjectOopLlvmLowerer.cpp   lowerEnumName: switch + phi, dedup de alias
CMakeLists.txt                       NSC_TEST_COMPILER + los 116 COMMAND
test_nsc.bat                         aviso de compilador desfasado
build_al_dia.py                      existir != estar al dia
mkdown/119_el_switch_que_si_acierta.md
```
