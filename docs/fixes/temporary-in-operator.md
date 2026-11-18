# Temporal `in` en operadores

En `ab01b9237`, la regresión binaria volvió a rechazar
`private-owned-temporaries` y `constructor-throw-temporaries` en O0 y O2;
los demás casos no se registraron como fallidos. El comando está en
`A:/nsc-fixes-evidencia/operator-in-binary-now-20261113.cmd`. Su log se
sobrescribió durante la comprobación posterior, por lo que la ejecución previa
queda como observación sin log independiente; la reproducción anterior se
conserva en `operator-in-binary-current.cmd` y su log, mencionados abajo.

El productor marcó inicialmente un operador estático solo cuando su MIR demuestra que
todos sus parámetros `in` se usan mediante lecturas de campos y que los únicos
resultados o escrituras son escalares `int`/`bool`. En esa etapa, llamadas,
alias y formas desconocidas no recibían la marca. Esta ocupa el bit 26 de registros de métodos;
en registros de campos conserva su significado anterior. La ausencia significa
desconocido y mantiene la guarda. El consumidor solo usa la marca del método
binario, no el resumen histórico `NoEscape`; la verificación comprueba su firma.
La marca modifica la metadata y puede modificar su huella, aunque no cambia el
ABI físico. No introduce una numeración nueva de metadata. Los consumidores
anteriores no se probaron con la marca nueva.

Con el compilador incremental build hash `322e3788496d8583`, la regresión
binaria pasó, incluidos los dos temporales antes rechazados y el rechazo de un
operador cuyo cuerpo llama a otro. También comprobó que cambiar el productor a
un cuerpo no acreditado invalida el resultado de consumo en caché; el log está
en `A:/nsc-fixes-evidencia/operator-in-binary-certified-20261113.log`.
Pasaron la prueba de escape histórico Direct/Alias/ViaCall y la prueba dirigida
de duración desde fuentes (`operator-in-legacy-escape-fixture.log` y
`70-temporary-current-lifetime.log`). Los casos inseguros no se ejecutaron.
Esa primera versión no cubría llamadas, alias, retornos de clase ni otros
escalares; esas formas seguían rechazadas cuando dependían de la guarda binaria.

El certificado recorre ahora el grafo de llamadas directas a métodos fuente
estáticos cuyos parámetros son `in`, siguiendo sus cuerpos MIR y visitando cada
función una sola vez por comprobación. Solo permite reenviar parámetros directos;
los métodos importados, cuerpos ausentes, argumentos calculados, alias y nodos
desconocidos conservan el rechazo. Una desigualdad que llama a la igualdad
acreditada ejecutó correctamente en O0/O2, con destrucción de ambos temporales.
Al cambiar la igualdad a un cuerpo no acreditado, tanto ella como la desigualdad
que la invoca se rechazaron; también pasó la invalidación de caché. La regresión
completa y el fixture histórico de escapes pasaron: comandos y salidas en
`operator-in-binary-now-20261113.cmd` y `.log` y
`operator-in-legacy-escape.cmd` y `operator-in-legacy-escape-fixture.log`,
en `A:/nsc-fixes-evidencia`. Esa prueba no acreditaba llamadas dinámicas,
binarias o con argumentos que no fueran parámetros directos.

En `1840f0951`, un operador binario seguro con un operando `in` propietario y
un `int` por valor se rechazaba en ambos órdenes y niveles; la salida está en
`operator-in-binary-mixed-before-20261113.log`. El productor acredita ahora
también parámetros escalares `int`/`bool` por valor y sus expresiones al
reenviarlos a una llamada directa acreditada. Los dos órdenes, reenvío de un
parámetro y reenvío de una constante ejecutaron 42 en O0/O2; se conservó el
rechazo para un argumento por valor de clase, un cuerpo llamado sin acreditar y
los escapes históricos. La prueba y los comandos están en
`A:/nsc-fixes-evidencia/operator-in-binary-mixed-after-20261113.log` y
`operator-in-legacy-escape-fixture.log`; el compilador incremental informó
build hash `826eb112150ff95b`. La regla no certifica otros tipos por valor,
llamadas dinámicas ni cuerpos precompilados.

El caso original del informe se rechazó en la base por falta de sobrecarga
viable. La decisión adoptada para esta tarea es materializar el operando una
vez, mantenerlo durante la llamada, limpiarlo en salidas normales y
excepcionales e impedir que escape. La investigación completa y los ensayos
anteriores están en `A:/nsc-fixes-evidencia/70-temporary-directed-scope.md`.

Con el compilador incremental del árbol actual, el ejemplo original sin cambios
ejecutó con salida 0 en O0/O2; los controles de uno y dos temporales y locales
ejecutaron 42. La regresión de duración también terminó con código 0. Comandos
y salidas: `A:/nsc-fixes-evidencia/70-temporary-current-original.cmd` y `.log`,
y `70-temporary-current-lifetime.cmd` y `.log`. El compilador informa build
hash `dbe260b6b7e51aed` y contiene otros cambios locales. Estos resultados no
certifican todas las formas de operandos, ni representan un commit aislado.

El control `operator_in_temporary_class.py` antes se detenía en el primer fallo.
Su ejecutor ahora reúne todos los fallos sin cambiar las expectativas ni
ejecutar casos rechazados. El retorno de clase sin calificador desde un owner
local se rechaza en O0/O2 porque no declara el préstamo; la prueba esperaba
aceptarlo. El control de tupla escalar ejecutó 42 en ambos modos; las tres
variantes con temporales fueron rechazadas. Los negativos también reciben un
diagnóstico en el cuerpo del operador, por lo que el resultado no demuestra
aisladamente que la llamada bloquee el escape. Comando y salida:
`A:/nsc-fixes-evidencia/70-temporary-current-class-all.cmd` y `.log`.

La lectura con un dueño local aún necesita un contrato de retorno que conserve
vida y solo lectura en el llamador; retirar el rechazo del cuerpo no sería
suficiente. La guarda actual también rechaza temporal `in` con almacenamiento
owned para operadores precompilados: el resumen `NoEscape` de bibliotecas
antiguas no demuestra por sí solo la ausencia de retención. No se considera
cerrado el caso ni se ha ejecutado una batería completa.

Para separar el rechazo de la llamada del rechazo del cuerpo, la misma prueba
incluye un operador que devuelve un objeto global, sin extraer un campo de sus
operandos. El control con dos dueños locales ejecutó 42 en O0/O2; las llamadas
con uno y dos temporales se rechazaron en la selección de sobrecarga en ambos
niveles. Estas llamadas concretas son seguras respecto de la vida del objeto
global, por lo que el resultado muestra también la pérdida de precisión de la
guarda conservadora actual. La selección sigue fallando por `local-owner`.
Comando y salida: `A:/nsc-fixes-evidencia/70-temporary-class-global-controls.cmd`
y `.log`. Los casos rechazados no se ejecutaron; el control no acredita todos
los posibles retornos globales ni cambia el contrato de firmas binarias.

Las pruebas de la reproducción y de duración quedaron registradas como
`nsc.compiler.operator-in-temporary` y `nsc.compiler.operator-in-temporary-lifetime`.
Los tres archivos de la reproducción se copiaron a
`tests/integration/operator_in_temporary_fixture` y se compararon con las
copias de `cases/70-temporary` mediante `fc /b`: no hubo diferencias. El
ejecutor copia esos archivos a un temporal antes de compilarlos. Tras regenerar
la configuración del build aislado, la selección CTest pasó 2/2
(`A:/nsc-fixes-evidencia/70-temporary-ctest-current.cmd` y `.log`). No se
registró como aprobada la prueba de retorno de clase que permanece fallando;
estas dos entradas tampoco cierran los pendientes de operadores precompilados.

El control unitario `nsc.ns3.t05.operator-traits` esperaba históricamente que
un literal para un parámetro `in` de un operador fuera rechazado. La expectativa
se actualizó a aceptación para el contrato elegido, sin quitar el ejemplo ni
alterar los demás controles. Su objetivo se compiló incrementalmente y pasó
sus 20 comprobaciones; también pasaron 2/2 las pruebas dirigidas de ejecución y
duración (`A:/nsc-fixes-evidencia/operator-in-trait-test-current.cmd` y logs
`operator-in-trait-test-{build,current,regressions}.log`). Esto corrige una
expectativa obsoleta, no los pendientes de retorno prestado o de binarios.

Una reproducción independiente intentó expresar el retorno del campo mediante
`public static ref readonly Payload operator +(in Owner a, in Owner b)` y
`return ref a.item`. El análisis sintáctico rechazó esa declaración porque la
forma de retorno por referencia solo se admite para un método ordinario
(`A:/nsc-fixes-evidencia/70-ref-readonly-operator/check.cmd` y `.log`). No se
alcanzaron la comprobación de vida del llamador ni la ejecución. Por tanto esa
escritura no es una alternativa funcional para el control pendiente; habilitarla
exigiría definir y verificar por separado el contrato de vida de los operandos
y su publicación binaria. El control original de retorno sin calificador no
se cambió ni se presenta como resuelto.

Con el compilador aislado SHA-256
`40566c10872f48dd5c12909c68983b1f972493d3639ec65997e3c190502d41bd`,
`operator-in-binary-current.cmd` volvió a fallar en cuatro combinaciones:
los temporales con campo `owned` y los temporales con constructor que lanza se
rechazan en O0/O2. Los casos escalares, locales y controles de caché de esa
misma prueba no figuran entre los fallos. El productor actual rechazó además
las tres variantes inseguras Direct, Alias y ViaCall; comandos y diagnósticos
en `70-binary-escape-variants/producer-recheck.log`. Esto no convierte en
fiables los resúmenes de productores arbitrarios. La guarda sigue vigente y
el caso seguro binario permanece sin resolver.

Se añadió `tests/integration/operator_in_legacy_escape.py` para comprobar la
guarda frente a la biblioteca anterior conservada en
`A:/nsc-fixes-evidencia/70-binary-escape-variants/`. El log de producción de
esa biblioteca registra build hash `14717501b74375ce` y aceptación de tres
operadores que publican un campo del operando por acceso directo, alias o
llamada. El compilador aislado actual rechazó las tres llamadas con temporales
y aceptó el análisis de las tres con locales
(`A:/nsc-fixes-evidencia/operator-in-legacy-escape.cmd` y `.log`). No se
ejecutaron los programas: la aceptación de locales no demuestra seguridad de
sus efectos posteriores. SHA-256: biblioteca
`e0c89507924728b9524647b518c62e11733951b960336ce97e19dd887b8ed836`, metadata
`7122789f4d72bad33a225549c9ee166f3178a821d59533a3a3ead1a92c7619c0`.
Esta regresión depende de esos artefactos de evidencia externos al repositorio;
no demuestra confiabilidad general de `NoEscape` ni habilita el caso seguro.

Posteriormente se copiaron la fuente y los dos artefactos identificados al
fixture `tests/integration/operator_in_legacy_escape_fixture`; la prueba verifica
sus hashes antes de usarlos y ya no necesita la carpeta de evidencia para
ejecutarse. Quedó registrada como `nsc.compiler.operator-in-legacy-escape` y
pasó 1/1 con las seis comprobaciones semánticas
(`A:/nsc-fixes-evidencia/operator-in-legacy-registered.log`). La configuración
aislada se regeneró sin reconstrucción completa. El fixture conserva un binario
de la base de la campaña, no demuestra compatibilidad con todos los productores
anteriores ni corrige el rechazo del operador seguro.
