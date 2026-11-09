# Arreglos accesibles por un préstamo de solo lectura

Un objeto de clase extraído mediante `Rc.Get()` o `Arc.Get()` es un préstamo de
solo lectura. La implementación anterior conservaba esa condición al acceder a
campos de clase, pero la perdía si el campo era un arreglo nativo. En ambas
familias el análisis aceptó `alias.Data[0] = 10`; con `Rc` aceptó además guardar
`alias.Data` en un local fuera del alcance del dueño. Los comandos y resultados
anteriores están en `A:/nsc-fixes-evidencia/public-readonly-array-field.log`,
`public-readonly-array-field-Arc.log` y `public-readonly-array-escape-Rc.log`.
Las reproducciones se limitaron al análisis semántico, sin ejecutar el escape.

La corrección conserva el préstamo al leer un campo de arreglo desde ese alias,
impide escribir sus elementos y conserva el préstamo en elementos de referencia.
También rechaza guardar el arreglo en destinos sin seguimiento del préstamo y
mantiene la marca al convertir explícitamente a un arreglo nativo. El análisis de
la conversión previamente aceptó `((int[])alias.Data)[0] = 10`; la salida de
esa primera ejecución se observó, pero su archivo fue sobrescrito al repetir la
prueba. La nueva regresión conserva esa expresión como caso negativo.

La regresión independiente `nsc.compiler.readonly-borrowed-array` usa los
controles local y atómico ya existentes. Comprueba escritura, inicialización y
reasignación de un destino, referencia mutable a elemento, condicional,
coalescencia, conversión explícita y acceso a elementos de clase; verifica la
lectura por el préstamo y la escritura ordinaria en O0/O2. Pasó 1/1 en
`A:/nsc-fixes-evidencia/readonly-array-ctest.log`. Los controles de las familias
públicas registran también rechazos para retorno y argumento y pasaron en
`public-readonly-array-regressions.log`, `public-readonly-array-escape-regressions.log`
y `public-readonly-array-class-escape.log`. Las pruebas existentes de referencias
locales y retornos pasaron 2/2 (`readonly-array-reference-tests.log`).

La compilación del compilador fue incremental y está documentada en
`readonly-array-build.log`; el sistema regeneró la configuración y la identidad
de versión, por lo que reconstruyó más objetivos que los cinco archivos de
frontend modificados. Las pruebas de punteros públicos se hicieron desde
fuentes; no acreditan consumo binario, seguridad concurrente, todos los caminos
de conversión ni acceso mediante código `unsafe`.

## Estructuras anidadas

Tras `764137d50`, una regresión con un arreglo de estructuras que contienen un
campo de clase mostró que `Child saved = alias.Data[0].Item` seguía siendo
aceptado (`readonly-array-struct-before.log`). La condición de préstamo se
perdía en el elemento struct antes de llegar al campo de clase. Se propagó al
elemento y a los campos struct anidados, así como a una conversión explícita;
los destinos no rastreados rechazan copiar esos valores prestados. Esta regla es
conservadora: también puede rechazar la copia por valor de una estructura que
solo contiene escalares. No cambia la lectura de campos escalares: la prueba
ejecuta una lectura de esa forma en O0/O2 para ambos controles.

La regresión dirigida verifica escape mediante campo, inicialización,
reasignación y conversión de la estructura, además de las comprobaciones
anteriores. El resultado final está en `readonly-array-ctest.log`. Los casos
de referencia a través de otras construcciones de valores no se consideran
cubiertos por esta prueba.

## Carga struct con campo de clase

Un control posterior usó `SharedLocalHandle<Box>.Get()` con retorno
`ref readonly Box`. La escritura directa `Get().Item.Value = 10` se rechazaba,
pero `Child saved = Get().Item` se aceptaba, perdiendo el préstamo del dueño
(`readonly-struct-payload-before.log`). Se conserva ahora el préstamo para un
retorno struct de instancia por referencia de solo lectura sin vida estática;
el campo de clase derivado ya no puede guardarse en un local sin préstamo.
La comprobación recorre los campos de estructuras anidadas; si solo encuentra
escalares conocidos, permite copiarlas por valor. Para tipos desconocidos,
precompilados o que pueden contener referencias conserva el préstamo de manera
conservadora.

La prueba dirigida cubre acceso directo y mediante `SharedBorrow`, tanto con el
control local como el atómico; además comprueba lectura escalar O0/O2. La
selección anterior de uso público pasó y las dos pruebas de referencias
existentes pasaron (`readonly-array-ctest.log`,
`public-readonly-array-existing-controls.log`, `readonly-array-reference-tests.log`).
La regresión ejecuta copias de estructuras escalares directas y anidadas en
O0/O2 y rechaza la copia del struct que contiene una clase. No acredita todos los
tipos de campos, modos de retorno ni la vida de módulos precompilados.
