# Cancelación diferida del ejemplo 88

El usuario eligió que la limpieza ocurra al siguiente `MoveNextAsync()` después
de `Cancel()`. La prueba conserva los seis archivos originales del ejemplo en
`tests/integration/fixtures/example88_original`; una comparación byte por byte
con el árbol de referencia confirmó que son idénticos. El proyecto histórico
se conserva como evidencia y no se ejecuta: el test crea un proyecto temporal
propio en A con un consumidor que comprueba el contrato acordado.

La prueba registrada `nsc.compiler.async-example88-deferred-cancel` pasó 1/1
con el compilador incremental aislado: ocho ejecuciones O0/O2 devolvieron 42.
Comprueba la limpieza diferida y única después del primer valor, y después del
segundo comprueba la traza 87 y que no se repita con avances posteriores ni
`Drop()`. Dos variantes instrumentadas comprueban las construcciones; los
originales no se modifican. Log y comando en
`A:/nsc-fixes-evidencia/example88-deferred-integrated-20261111.log` y `.cmd`.
El compilador contenía otros cambios locales, por lo que no es una validación
de revisión limpia ni una demostración de todos los recorridos async.
