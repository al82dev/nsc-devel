# Accesores binarios de una base del payload

Un productor NativeSharp separado declara `Base` con un getter de propiedad
que guarda `this` en un estático y otro que solo devuelve un entero. En el
consumidor, `Payload : Base` accede mediante `base` desde constructor o
destructor y se crea con Rc o Arc de Core binario. El sondeo solo analiza y
emite; ningún consumidor se ejecuta.

El productor se emitió correctamente. Para ambas familias y ambos momentos,
las dos propiedades superaron análisis semántico y la emisión rechazó tanto
el getter publicador como el inocuo con `shared payload receiver invokes a
callable with unverifiable body` (ocho cruces). Esto es una contención
conservadora, no evidencia de que se haya inspeccionado el cuerpo binario.
Los cuatro controles fuente publicadores se rechazaron por publicación;
los cuatro fuente inocuos y los cuatro sin llamada emitieron.

La prueba de `base[0]` fue rechazada en semántica con `base may only appear
as the receiver of a member access`, antes de la guarda; por tanto no prueba
la protección de indexadores binarios. Registro y fuentes:
`A:/nsc-fixes-agente-2-evidencia/receiver-binary-accessor-20261112/run-20261112T063850678608Z/results.json`.
Los hashes del compilador, Core y metadata permanecieron estables, los procesos
arrancaron en la copia aislada y los temporales estuvieron en A. El sondeo
usa O0 y un compilador WIP; no demuestra seguridad general de métodos,
despacho dinámico, bases históricas ni descarga del módulo.

Una segunda variante sustituye `base[0]` por `this[0]` y añade un indexador
inocuo `this[0l]`, ambos heredados del productor binario. El productor y la
semántica pasaron; ambas llamadas se rechazaron conservadoramente con
`shared payload receiver invokes a callable with unverifiable body` para Rc y
Arc en constructor y destructor. Los publicadores fuente se rechazaron por
publicación y los controles inocuos/sin llamada emitieron. Son 20 cruces O0;
no se ejecutó ningún consumidor. Registro y hashes estables de compilador,
Core y metadata:
`A:/nsc-fixes-agente-2-evidencia/receiver-binary-indexer-20261112/run-20261112T064048739625Z/results.json`.
El compilador usado es posterior al primer sondeo; no se considera que el
rechazo conservador acredite inspección del cuerpo binario.

La regresión `tests/integration/shared_receiver_binary_accessors.py` combina
los controles en un productor propio: exige 28 resultados de emisión y dos
rechazos semánticos `base[0]`, sin ejecutar consumidores. Pasó en O0; Core sin
configurar devolvió 77 y Core configurado inválido 1. El registro, con hashes
de compilador, Core y metadata estables, es
`A:/nsc-fixes-agente-2-evidencia/receiver-binary-accessors-regression-20261112/run-20261112T064235512256Z/results.json`.
La versión incorporada en la copia principal se repitió con los mismos 28
resultados, dos barreras semánticas y controles 77/1. Registro:
`A:/nsc-fixes-agente-2-evidencia/receiver-binary-accessors-integrated-20261112/run-20261112T064409931224Z/results.json`.
