# Vistas escalares por ref/out desde una biblioteca

El control compara un `Sink` compilado con el consumidor contra el mismo `Sink`
publicado como biblioteca NativeSharp estática. El Core consumido es binario y
se selecciona mediante `NSC_TEST_CORE_BINARY`; su biblioteca y metadata deben
existir. `NSC_TEST_TEMP_ROOT`, o en su ausencia `TEMP`, debe ser un directorio
existente en A. Sin estos requisitos la prueba devuelve 77 y CTest la omite.

La matriz contempla Rc y Arc, salida `ref` y `out`, entrada por valor y por
`ref`, y productor/consumidor en el mismo nivel O0 u O2. Antes comprueba la
semántica del productor, la publicación y que las cuatro firmas binarias se
puedan resolver. Acepta y ejecuta las vistas cuyo dueño permanece vivo; analiza
sin ejecutar los casos que escapan del ámbito o reemplazan al dueño. Los
rechazos deben contener un diagnóstico de vida conocido.

El ensayo previo con el compilador SHA-256
`15b3e955f7eab1b471bdd2ecdfc82a35b3b1bd65239ffbd5ddd4d91f2cc36c26`
y Core SHA-256 `f91d95c248f477531d38a093605f45343f6be9736d15e2097acd452db4bb54e0`
pasó 96 comprobaciones semánticas y 32 ejecuciones válidas. Comandos, fuentes,
salidas y hashes están en
`A:/nsc-fixes-agente-2-evidencia/binary-view-cwd-repetition-20261112/ref-out/run-20261112T040726384706Z/results.json`.
El compilador contenía prototipos experimentales; la regresión permanente
requiere repetición integrada sobre la revisión final. El ensayo no
cubre tuplas, DLL, cruce de optimizaciones, metadata histórica,
descarga de módulos ni todos los posibles escapes.

La extensión opcional `NSC_VIEW_TEST_INCLUDE_IN=1` añade una entrada `in` con
salidas `ref` y `out`. Su ensayo aislado comprobó el productor y las firmas,
48 comprobaciones semánticas y 16 ejecuciones válidas; el registro completo
está en `A:/nsc-fixes-agente-2-evidencia/binary-view-cwd-repetition-20261112/in/run-20261112T040743251503Z/results.json`.
Ese ensayo utilizó el compilador experimental y el Core binario preexistente
identificados arriba. Su independencia de cambios sin registrar en el
compilador no está demostrada.

Una matriz diagnóstica independiente con dos entradas `in`, propietarios de
ámbitos diferentes y salida `ref` u `out` pasó 80 análisis: 16 usos con ambos
dueños vivos, 32 escapes y 32 reemplazos rechazados. Los 16 usos vivos
ejecutaron 42; los inseguros solo se analizaron. Productor semántico, las dos
bibliotecas y sus dos firmas binarias pasaron. Mitad de los consumidores usó
productor fuente y mitad binario; se probaron Rc/Arc y O0/O2 del mismo nivel.
El booleano selecciona la primera entrada; el rechazo relativo al segundo
propietario muestra la unión conservadora en esos casos concretos. Las fuentes,
comandos y hashes están en
`A:/nsc-fixes-agente-2-evidencia/binary-view-cwd-repetition-20261112/multiple/run-20261112T040752328809Z/results.json`.
Esta matriz diagnóstica no se añadió a la regresión permanente y conserva las
limitaciones del compilador WIP y del Core preexistente descritas arriba.

Se contrastó la ausencia de metadata del productor: con la metadata coincidente,
el consumidor con dueño vivo pasó el análisis y el escape se rechazó con
`NS3020 view output cannot outlive its possible owner`. Una copia aislada de
la biblioteca sin metadata se rechazó antes del análisis de vida, tanto cuando
se omitió el atributo `Metadata` como cuando se indicó una ruta inexistente;
en ambos casos el diagnóstico indicó que la metadata no existe. Se analizaron
las dos variantes de consumidor en cada configuración, sin ejecutar ninguna.
Fuentes, comandos, diagnósticos y hashes:
`A:/nsc-fixes-agente-2-evidencia/binary-view-cwd-repetition-20261112/missing-metadata/run-20261112T040804419101Z/results.json`.
Esto demuestra la barrera de importación de ese caso con el compilador WIP,
no una comprobación de vida sin metadata ni compatibilidad de productores
históricos. El productor usado ya se había generado en la campaña actual.

Estos cuatro controles se repitieron desde el repositorio aislado con los
subprocesos iniciados en ese mismo directorio. Los registros anteriores se
conservan, pero no son conformes porque sus procesos usaron como directorio de
trabajo una carpeta temporal. La prueba permanente inicia también sus procesos
desde el repositorio; con la extensión `in` activa pasó con el compilador WIP y
Core indicados arriba, y devolvió 77 al retirar la configuración de Core. Los
registros están en `A:/nsc-fixes-agente-2-evidencia/binary-view-cwd-repetition-20261112/permanent-configured.log`
y `A:/nsc-fixes-agente-2-evidencia/binary-view-cwd-repetition-20261112/permanent-missing-core.log`.
