# Parte XIII. Live code al estilo Deitel, con filosofía NativeSharp

> **Edición NativeSharp 1.0.1-dev, Stage 67.** Este suplemento forma parte de la documentación
> versionada actual. Su inclusión no afirma un baseline 1.0.1 nuevo; los ejemplos conservan la
> evidencia de su edición de origen hasta la siguiente certificación explícita.

Esta parte adopta la fortaleza pedagógica del enfoque Deitel: presentar una aplicación funcionando,
desmontarla línea por línea, hacerla crecer y terminar con ejercicios que obligan a modificar el
diseño. La adaptación no copia programas ni explicaciones. Conserva la disciplina de aprendizaje por
código vivo y reemplaza las suposiciones del CLR por contratos propios de NativeSharp.

En cada laboratorio aparecen seis elementos:

1. **Prueba de manejo:** salida y comportamiento antes de estudiar la implementación.
2. **Modelo:** clases, structs, arrays, delegates o recursos que forman la solución.
3. **Construcción incremental:** versiones pequeñas que agregan una responsabilidad por vez.
4. **Auditoría nativa:** ownership, RAII, layout, costo, errores y fronteras de plataforma.
5. **Pruebas:** resultado conocido, checksum y casos de borde.
6. **Ejercicios:** cambios suficientemente grandes para exigir razonamiento, no edición cosmética.

## 66. La filosofía live-code de NativeSharp

### 66.1 Familiaridad sin fingir un CLR

Una clase, una propiedad o un `foreach` deben resultar familiares para un programador C#. Esa
familiaridad es una puerta de entrada, no una promesa de semántica administrada. NativeSharp produce
código nativo, destruye de manera determinista y expresa transferencia mediante `owned`, `borrowed`,
`raii` y `move`.

Cada ejemplo responde, además de “¿qué imprime?”, estas preguntas:

- ¿Qué bloque de memoria contiene cada valor?
- ¿Quién destruye cada objeto, array, string, handle o delegate?
- ¿Qué operaciones asignan memoria o cruzan el sistema operativo?
- ¿Qué cambia entre O0 y O2 y qué debe permanecer idéntico?
- ¿Qué parte del comportamiento pertenece al lenguaje y cuál a Windows o a una DLL?

### 66.2 De la demostración al programa editorial

Una demostración de diez líneas sirve para una regla aislada. Un capítulo de programación necesita
estado, invariantes, errores, varias clases y un resultado verificable. Por ello, las versiones finales
se publican como proyectos `.nsproj` completos y no como fragmentos inconexos.

El proyecto 26 contiene seis aplicaciones conectadas por un checksum. Los proyectos 1 a 25 ofrecen
las ampliaciones de algoritmos, estructuras, interoperabilidad y diseño orientado a objetos.

### 66.3 La secuencia de una lección

El lector ejecuta primero. Después predice una modificación, cambia el código, recompila y compara.
La explicación se introduce donde el programa la necesita. Una excepción aparece al validar una hora,
no en una lista abstracta de keywords; un destructor aparece cuando una cuenta posee un nombre; un
atomic aparece cuando dos hilos comparten un contador.

## 67. Aplicación 1: cuenta bancaria nativa

### 67.1 Primera versión: nombre y saldo

`BankAccount` comienza con dos campos privados. El nombre se copia porque la cuenta lo posee. El
saldo se expresa en centavos `long`; no se usa `double` para dinero. El constructor rechaza nombre
vacío y saldo negativo antes de establecer la invariante.

La primera prueba crea una cuenta y consulta propiedades. El lector debe identificar que la referencia
local `account` es el dueño RAII y que la propiedad `Owner` presta el string interno. Destruir el valor
devuelto sería un error de ownership.

### 67.2 Segunda versión: depósito y retiro

`Deposit` exige cantidad positiva y falla mediante excepción si el contrato se viola. `TryWithdraw`
usa resultado booleano porque “fondos insuficientes” es una posibilidad normal del dominio. La API
distingue error de programación de decisión comercial.

Después de depositar 27,550 centavos y retirar 32,000, el saldo conocido es 120,550. Un segundo retiro
demasiado grande debe dejar el saldo intacto. El resultado se incorpora al checksum, por lo que no
basta imprimir una frase correcta.

### 67.3 Tercera versión: estado y ownership

`StatementLine` devuelve `owned string`: crea una representación que el llamador debe poseer o mover.
El destructor libera el nombre interno exactamente una vez. Este pequeño ejemplo establece la regla
que después gobierna árboles, colecciones y respuestas de APIs nativas.

### 67.4 Ejercicios de ampliación

- Agregar identificador inmutable y detectar cuentas duplicadas en un ledger.
- Implementar transferencia atómica entre dos cuentas bajo un `Mutex`.
- Registrar movimientos en un array dinámico y producir estado de cuenta.
- Definir una política de sobregiro separada de la clase de almacenamiento.
- Serializar centavos en little-endian y validar versión del archivo.

## 68. Aplicación 2: control de flujo con políticas reales

### 68.1 Calificaciones, sentinelas y clasificación

Un flujo de calificaciones valida rango 0..100, acumula total y clasifica en once buckets. La versión
interactiva puede usar sentinela; la versión automatizada usa un array para que las pruebas sean
reproducibles. `switch` clasifica políticas discretas; `if` expresa rangos y condiciones compuestas.

No se mezcla lectura con cálculo. Un productor obtiene datos, `GradeBook` valida y conserva, y una
capa de presentación imprime. Esta separación permite ejecutar el mismo dominio desde consola, GUI o
host C sin duplicar estadísticas.

### 68.2 Interés compuesto sin redondeo accidental

El ejemplo clásico de interés es útil para loops, pero un libro NativeSharp debe discutir
representación. Un simulador financiero serio no acumula dinero en `double`. El laboratorio usa
centavos o fixed-point, define la regla de redondeo por periodo y comprueba conservación.

Ejercicio: compare redondear mensualmente con redondear sólo al final. Documente cuál contrato usa una
institución y pruebe valores cercanos al límite de `long`.

### 68.3 Pólizas mediante `switch`

Una póliza de automóvil clasifica región y tipo de vehículo. La primera versión usa `switch`; la
segunda extrae una estrategia. La evolución enseña que una construcción de control es adecuada para
un conjunto cerrado, mientras el polimorfismo favorece políticas extensibles.

### 68.4 Operadores lógicos y cortocircuito

Las condiciones se escriben para que una lectura peligrosa aparezca después de su guardia:
`index < length && data[index] == value`. El cortocircuito es parte de la seguridad del acceso. Los
operadores booleanos no cortocircuitados evalúan ambos lados y sólo se usan cuando eso forma parte del
contrato.

## 69. Aplicación 3: azar reproducible, baraja y craps

### 69.1 Generador y semilla

`Random(20260906L)` produce una secuencia reproducible. Una prueba que usa auto-semilla puede fallar
una vez y volverse imposible de repetir. La semilla explícita se registra junto al compilador y nivel
de optimización.

El ejemplo lanza dados en `[1, 7)` y construye histogramas. La discusión incluye sesgo por módulo,
diferencia entre PRNG y CSPRNG y por qué ningún resultado de `Random` debe proteger secretos.

### 69.2 Craps como máquina de estados

`CrapsStatus` distingue continuar, ganar y perder. El primer lanzamiento decide resultado o punto;
los siguientes terminan al repetir punto o sacar siete. La implementación evita booleanos ambiguos
como `done` más `won` que pueden representar estados imposibles.

La prueba ejecuta mil partidas con semilla fija. Los triunfos no se comparan con una probabilidad
redondeada: se comparan con el resultado exacto de esa secuencia. Otro test estadístico separado puede
evaluar distribución sobre muchas semillas.

### 69.3 Baraja y Fisher-Yates

`Deck` crea 52 objetos `Card`, posee el array y destruye cada carta. Fisher-Yates recorre desde el
final y elige uniformemente entre posiciones todavía disponibles. Intercambiar cada posición con una
posición de todo el array introduce sesgo y no es una variante equivalente.

`Deal` presta una carta cuyo dueño sigue siendo la baraja. El llamador puede leerla, pero no
destruirla ni conservarla más allá de la vida del deck. El ejemplo hace explícita una relación que el
GC de C# suele mantener automáticamente.

### 69.4 Ejercicios

- Evaluar manos de póquer sin depender del orden recibido.
- Simular blackjack con un shoe de varias barajas y carta de corte.
- Comparar Fisher-Yates con un shuffle sesgado mediante frecuencias de permutaciones pequeñas.
- Guardar y restaurar estado del PRNG para reproducir una partida.

## 70. Aplicación 4: `GradeBook`, arrays y excepciones

### 70.1 Construcción y copia defensiva

El constructor recibe un array prestado, valida cada calificación y copia a almacenamiento propio.
Guardar directamente la referencia permitiría que el llamador cambiara las notas sin pasar por la
invariante y crearía ambigüedad sobre quién ejecuta `delete[]`.

Una entrada inválida lanza antes de publicar el objeto. El proyecto prueba mínimo, máximo, promedio
multiplicado por cien e histograma. El promedio entero se presenta como decisión deliberada, no como
equivalente silencioso de formato decimal.

### 70.2 Histogramas y visualización textual

Once buckets representan 0-9 hasta 100. El gráfico textual separa cómputo de salida y permite validar
el array de conteos antes de dibujar. Para millones de datos, el número de buckets permanece fijo y
el costo es O(n).

### 70.3 Rectangular frente a layout plano

El runtime actual favorece arrays nativos contiguos. Una matriz `rows × columns` puede almacenarse en
un array plano con `row * columns + column`. Esto hace explícito el layout que una DLL C espera y
evita confundir un array de referencias con una matriz contigua.

### 70.4 Manejo de excepciones

El ejemplo diferencia una calificación inválida, un conjunto vacío y un fallo de asignación o archivo.
El `catch` sólo captura lo que puede resolver; `finally` y RAII garantizan limpieza. No se usa una
excepción para una búsqueda que simplemente no encontró un elemento.

## 71. Aplicación 5: una clase de tiempo correcta

### 71.1 Representación canónica

`TimeOfDay` almacena segundos desde medianoche. Una sola representación evita estados como minuto 90
o dos combinaciones distintas para la misma hora. Las propiedades derivan hora, minuto y segundo.

El constructor vacío y las sobrecargas de una, dos y tres componentes terminan en la misma validación.
No se duplica lógica. El test principal avanza 23:59:59 y exige 00:00:00.

### 71.2 Encapsulación y comparación

El valor interno no se expone para escritura. `CompareTo` ordena por segundos. Una versión como
`struct` sería adecuada si se desea copia por valor; una clase permite identidad y extensión. El
capítulo obliga a justificar la elección en lugar de convertir todo tipo pequeño en clase por hábito
de C#.

### 71.3 Excepciones y fronteras

Se prueban 24:00:00, minuto 60, segundo negativo y último instante válido. El mensaje humano puede
mejorar, pero la invariante no depende del texto de la excepción. En una frontera `extern C`, la
excepción se captura y se traduce a código de error; nunca cruza el ABI.

## 72. Aplicación 6: nómina, facturas e interfaces

El proyecto 2 implementa una jerarquía de empleados con pago polimórfico. La ampliación estilo Deitel
agrega una interfaz `Payable` compartida por empleados y facturas. El reporte recibe elementos
heterogéneos y solicita importe sin conocer la fórmula concreta.

La filosofía NativeSharp agrega tres auditorías:

- La colección define si posee cada `Payable`.
- La destrucción a través de base o interfaz debe invocar el destructor dinámico correcto.
- Los importes se representan en centavos y se comprueba overflow antes de multiplicar.

Ejercicio: agregar impuestos como estrategia, generar una DLL con función `extern C` que calcule el
total y consumirla desde un host C# mediante P/Invoke.

## 73. Aplicación 7: consultas funcionales sin ocultar costo

### 73.1 De LINQ a pipelines explícitos

Los tutoriales 15, 16 y 17 muestran colecciones, `foreach`, delegates, lambdas y closures. Un pipeline
NativeSharp puede filtrar, proyectar, ordenar y agregar, pero debe documentar si es lazy o materializa
un array nuevo. “Una línea” no significa “una pasada” ni “sin asignaciones”.

El laboratorio de empleados ejecuta:

1. filtro por salario;
2. proyección a registro de reporte;
3. orden por departamento y apellido;
4. agrupación y total por departamento;
5. liberación de cada resultado materializado.

### 73.2 Genéricos AOT

Cada instancia cerrada se monomorfiza. Esto permite código especializado, pero puede aumentar tamaño
del binario. Constraints expresan capacidades necesarias. Un comparador o hasher delegado evita
suponer reflection, boxing o métodos virtuales universales.

### 73.3 Paralelismo explícito

No se presenta PLINQ como magia. Primero se mide la versión secuencial; después se particiona por
rangos, cada hilo produce un acumulador local y una reducción combina resultados. Compartir un
contador por elemento generaría contención aunque fuera correcto.

## 74. Aplicación 8: archivo de crédito con formato estable

### 74.1 Registros binarios

`CreditFileDemo` escribe pares de `int` como little-endian: número de cuenta y saldo en centavos.
No serializa el layout privado de una clase. El formato se puede leer desde otra versión del
compilador o desde C porque tamaño, orden de bytes y offsets están definidos.

### 74.2 Consulta de tres vías

La lectura clasifica saldos en crédito, débito y cero. La lógica recibe bytes ya validados; la capa de
archivo comprueba longitud antes de recorrer. Un archivo truncado no debe producir una lectura fuera
de límites ni un registro parcial aceptado.

### 74.3 Evolución del formato

La siguiente versión agrega magic, versión, cantidad de registros y checksum. Una migración lee la
versión anterior y escribe la nueva en un archivo temporal antes de reemplazar el original. La
durabilidad requiere distinguir “la función devolvió” de “los bytes llegaron al almacenamiento”.

### 74.4 Ejercicios

- Implementar consulta por cuenta con índice hash separado.
- Agregar tombstones y compactación.
- Exportar el lector como biblioteca estática y DLL.
- Fuzzear archivos truncados y campos fuera de rango.
- Comparar texto delimitado, registros fijos y formato versionado.

## 75. Aplicación 9: eventos y una interfaz nativa

Los capítulos GUI de C# enseñan controles y event handlers. NativeSharp no debe inventar Windows Forms.
El equivalente conceptual separa modelo, evento y adaptador de plataforma. El tutorial 18 implementa
publicación, suscripción y desuscripción mediante delegates nativos.

Una UI Win32, SDL o una DLL host puede traducir un mensaje externo a un método del modelo. El handler
no posee automáticamente al publisher ni al receptor. El programa debe garantizar que ningún
delegate invoque un objeto destruido.

El laboratorio comienza con una interfaz de consola que dispara los mismos eventos. Después se crea
un adaptador C ABI. Sólo al final se conecta una toolkit gráfica. De esta forma, la lógica permanece
probable sin clicks automatizados y la dependencia visual no contamina el dominio.

## 76. Aplicación 10: dos hilos nativos y estado atómico

### 76.1 Del `async` administrado al trabajo nativo

`async` y `await` dependen de Task, scheduler y convenciones que NativeSharp stage 57 no promete.
La adaptación usa `Thread`, `AtomicInt32`, mutexes, eventos y semáforos disponibles. No renombra un
hilo como “async”; explica bloqueo, join y memoria compartida.

### 76.2 Orden de vida

`CounterWorker` se declara antes que los objetos `Thread`. La destrucción inversa garantiza que los
hilos se unan antes de destruir el worker si se abandona el scope. El ejemplo llama `Join`
explícitamente y el destructor de `Thread` conserva auto-join como red de seguridad.

Cada worker captura sus excepciones: una excepción que escapa del entry point de un hilo puede terminar
el proceso. Dos hilos incrementan un `AtomicInt32` diez mil veces cada uno y el resultado exacto debe
ser 20,000.

### 76.3 Ejercicios

- Sustituir atomic por incremento ordinario y observar la carrera bajo muchas repeticiones.
- Particionar suma de un array sin compartir escritura durante el loop.
- Implementar productor-consumidor con `Semaphore` y buffer circular.
- Propagar error del worker mediante estado protegido y traducirlo en el hilo principal.
- Comparar join explícito, auto-join y detach, documentando riesgos de vida.

## 77. Doce talleres live-code

Los siguientes talleres forman una secuencia de estudio. Cada uno comienza con un ejecutable, agrega
una decisión de diseño y termina con una auditoría nativa.

| Taller | Aplicación | Conceptos NativeSharp | Implementación base |
|---:|---|---|---|
| 1 | diagnóstico de compilación | proyecto, `Main`, consola, exit code | tutorial 1 |
| 2 | calculadora de enteros | precedencia, overflow, división y errores | tutoriales 2-4 |
| 3 | cuenta bancaria | clase, propiedad, centavos, ownership | proyecto 26 |
| 4 | libro de calificaciones | arrays owned, estadísticas, histograma | proyecto 26 |
| 5 | simulador de interés | loops, fixed-point, redondeo | capítulo 68 |
| 6 | craps reproducible | enum, switch, PRNG y máquina de estados | proyecto 26 |
| 7 | baraja | objetos en array, Fisher-Yates y préstamos | proyecto 26 |
| 8 | tiempo del día | sobrecargas, validación y representación canónica | proyecto 26 |
| 9 | nómina y facturas | abstract, override, interface y destrucción | proyecto 2 |
| 10 | consultas de empleados | genéricos, delegates, materialización | tutoriales 15-17 |
| 11 | archivo de crédito | bytes, formato, versión y corrupción | proyecto 26 |
| 12 | contador paralelo | threads, atomics, excepciones y join | proyecto 26 |

## 78. Correspondencia editorial con la progresión Deitel

Esta tabla registra qué se conserva y qué se adapta de cada capítulo de *C# 6 for Programmers*.

| Capítulo Deitel | Propósito pedagógico | Tratamiento NativeSharp |
|---:|---|---|
| 1 | panorama de tecnología | código nativo, ABI, ownership y toolchain |
| 2 | IDE y programación visual | `.nsproj`, diagnósticos, build y adaptadores UI |
| 3 | primera aplicación | tutoriales 1-3 con exit codes verificables |
| 4 | clases y cuenta | capítulos 67 y proyecto 26 |
| 5-6 | control de flujo | tutorial 3 y capítulos 68-69 |
| 7 | métodos, azar y recursión | tutoriales 4 y 20, proyectos 25-26 |
| 8 | arrays y excepciones | tutoriales 5 y 19, capítulo 70 |
| 9 | LINQ y List | pipelines explícitos del capítulo 73 |
| 10 | clases profundas y tiempo | capítulo 71 |
| 11-12 | herencia, interfaces y polimorfismo | proyecto 2 y capítulo 72 |
| 13 | excepciones profundas | RAII, unwinding y fronteras nativas |
| 14-15 | GUI y eventos | modelo de eventos más adaptador de plataforma |
| 16 | strings y caracteres | bytes, UTF-8, ownership y algoritmos de texto |
| 17 | archivos y streams | capítulo 74 y `NativeSharp.Core.IO` |
| 18 | genéricos | monomorfización AOT y constraints |
| 19 | colecciones y funcional | costos visibles, delegates y partición explícita |
| 20 | bases de datos | ABI de driver, transacciones y mapeo sin ORM supuesto |
| 21 | async/await | threads, atomics, eventos, semáforos y join |

## 79. Criterio de aceptación de un ejemplo live-code

Un ejemplo de esta parte no se considera terminado sólo porque compila. Debe cumplir:

- el comportamiento inicial puede ejecutarse antes de leer la explicación;
- cada incremento introduce una sola responsabilidad conceptual;
- el resultado se valida por datos o checksum, no sólo por salida plausible;
- se prueban al menos un límite y un fallo;
- ownership y destrucción se explican donde aparecen;
- no se importan garantías de GC, reflection, Task, LINQ o Windows Forms que no existen;
- la versión editorial exacta compila en O0 y O2;
- ambas ejecuciones coinciden;
- el código respeta Allman, cuatro espacios, `Funcion()` y 120 columnas.

Con este criterio, “estilo Deitel” describe una forma rigurosa de enseñar mediante programas vivos;
“filosofía NativeSharp” determina las decisiones de memoria, rendimiento, ABI y plataforma.
