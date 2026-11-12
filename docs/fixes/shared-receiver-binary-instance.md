# Publicación mediante método de instancia binario

Un productor estático declara `Base`, un método público `Publish()` que asigna
`this` a un campo estático y un método `Safe()` vacío. El consumidor crea una
clase fuente `Payload : Base` mediante Rc o Arc y llama al método desde su
constructor o destructor. Los programas se analizaron y emitieron en O0; no se
ejecutó ninguno.

La forma `this.Publish()` o `this.Safe()` no alcanzó la guarda: los ocho
consumidores binarios fallaron en análisis porque no se resolvió el miembro
heredado. El método fuente publicador se rechazó con el diagnóstico de la
guarda, mientras que los controles fuente inocuo y sin llamada emitieron.
`A:/nsc-fixes-agente-2-evidencia/receiver-instance-binary-20261112/run-20261112T052555982401Z/results.json`
contiene comandos, fuentes y hashes. Este rechazo previo no demuestra que la
guarda haya inspeccionado el método binario.

La variante independiente `base.Publish()` sí superó el análisis y la emisión
en cuatro combinaciones (Rc/Arc y constructor/destructor), al igual que
`base.Safe()` y los controles fuente inocuo y sin llamada. El publicador fuente
se rechazó en las cuatro combinaciones. Registro:
`A:/nsc-fixes-agente-2-evidencia/receiver-base-binary-20261112/run-20261112T052700452377Z/results.json`.
La emisión aceptada y el cuerpo del productor muestran que esta forma de
publicación indirecta no fue rechazada; no se ejecutó una lectura posterior a
la destrucción ni se demostró una carrera. Los hashes se mantuvieron estables;
el compilador y Core corresponden al árbol con cambios de trabajo.

Por lectura, `directReceiverCallees` conserva el destino y el recorrido solo
continúa cuando encuentra su cuerpo MIR. No debe rechazarse cualquier destino
ausente sin distinguir el constructor de una base binaria certificada. La
política para ese caso y las rutas virtuales y de interfaz permanecen fuera de
esta evidencia; no se ha implementado aquí una corrección.

Se preparó un candidato que, ante un destino directo de receptor sin cuerpo
MIR verificable, rechaza por incertidumbre. Conserva únicamente la excepción
para un constructor de base precompilada de la cadena cuyo ciclo de vida esté
certificado; un método binario inocuo sin cuerpo verificable se rechaza también
de forma conservadora. La excepción exige que la función MIR encontrada por
símbolo coincida en ordinal, símbolo y categoría de constructor, y que su dueño
sea ancestro comprobado del constructor que se analiza; si falta un layout o
hay una identidad no demostrada no se aplica la excepción. Despacho virtual,
interfaz y retornos opacos siguen fuera de esta corrección.

La regresión comprueba Rc/Arc, constructor/destructor, método binario
publicador e inocuo, sobrecarga inocua, método fuente publicador e inocuo,
control sin llamada y base binaria sin certificado. Después de la compilación
incremental del árbol de trabajo, pasó 26 análisis y 26 comprobaciones de
emisión O0; ningún consumidor se ejecutó. Los métodos binarios publicadores y
seguros sin cuerpo verificable se rechazaron conservadoramente con el
diagnóstico específico; los métodos fuente publicadores y las bases sin
certificado también se rechazaron con sus diagnósticos. El control sin llamada,
con base binaria certificada, emitió para Rc y Arc en las cuatro combinaciones.
El resultado demuestra esa aceptación, pero la prueba no instrumentó el punto
de excepción para acreditar por sí sola qué rama interna se ejecutó.

Pasaron además 24 comprobaciones de emisión de argumentos de receptor y 16 de
despacho fuente virtual/interfaz, todas O0 y sin ejecutar consumidores. Los
logs individuales `receiver-instance-validation-{instance,argument,dispatch}-20261112.log`
y la compilación `receiver-instance-build-20261112.log` están en
`A:/nsc-fixes-evidencia`. El compilador usado mantuvo SHA-256
`6866e0d770e46bdb7ad29d8297d6a846d083a1f72dc96d58a5c4aa1e207991eb`, y
el Core binario preexistente y su metadata mantuvieron respectivamente
`f91d95c248f477531d38a093605f45343f6be9736d15e2097acd452db4bb54e0` y
`774db7b3a80d64cc594c1b05c448a8dcda7b7258abc729d09984df5d8478718c`.
Las pruebas se iniciaron desde el repositorio principal con temporales en A;
el Core no se regeneró y el build contiene otros cambios de trabajo. No se
certifican O2, ejecución de los controles ni todas las rutas de despacho.

Como controles basales adicionales, la sobrecarga binaria inocua `Safe(int)`
se emitió en las cuatro combinaciones, y una base binaria cuyo constructor
publica el receptor fue rechazada en ambas familias por falta de certificado.
El registro `A:/nsc-fixes-agente-2-evidencia/receiver-base-binary-20261112/baseline-overload-uncertified.log`
contiene comandos y hashes. No se ejecutaron consumidores. El volcado HIR/MIR
del control con base certificada mostró `member=#940` y el símbolo del
constructor importado también listado como función MIR sin cuerpo. La
validación posterior confirmó la emisión, sin instrumentar esa excepción.
