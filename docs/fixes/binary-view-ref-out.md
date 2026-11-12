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
`A:/nsc-fixes-agente-2-evidencia/binary-view-ref-out-20261111/run-20261112T032759700080Z/results.json`.
El compilador contenía prototipos experimentales; la regresión permanente
todavía requiere ejecución integrada sobre la revisión final. El ensayo no
cubre tuplas, DLL, cruce de optimizaciones, metadata histórica,
descarga de módulos ni todos los posibles escapes.

La extensión opcional `NSC_VIEW_TEST_INCLUDE_IN=1` añade una entrada `in` con
salidas `ref` y `out`. Su ensayo aislado comprobó el productor y las firmas,
48 comprobaciones semánticas y 16 ejecuciones válidas; el registro completo
está en `A:/nsc-fixes-agente-2-evidencia/binary-view-ref-out-20261111/run-20261112T033725649320Z/results.json`.
Ese ensayo utilizó el compilador experimental y el Core binario preexistente
identificados arriba. La extensión permanente aún no se ha ejecutado, y su
independencia de cambios sin registrar en el compilador no está demostrada.
