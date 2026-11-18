# Limpieza del bloque si falla la inicialización estática

Las fábricas internas locales y atómicas reservaban el bloque y después llamaban
a `Construct` o `ConstructValue`. Aunque el constructor de instancia no
declarase `throws`, el constructor estático del tipo podía lanzar antes de que
terminara esa llamada. El IR anterior emitía una llamada sin destino de cleanup;
la liberación solo ocurría cuando la construcción regresaba `null`. Las
reproducciones públicas con `Rc` y `Arc`, para clase y struct, capturaron la
excepción y devolvieron 42 en O0/O2. El resultado de ejecución no mide fugas;
la ausencia de cleanup se observó en el IR de las cuatro rutas. Comandos y
salidas anteriores en `A:/nsc-fixes-evidencia/static-factory-leak-probe/`.

Las dos sobrecargas `Create` de cada fábrica protegen ahora exclusivamente la
construcción con `finally`. Si la llamada no retorna, liberan el bloque; si
retorna, dejan el tratamiento de `null` y la transferencia exitosa como antes.
No llaman al destructor de un objeto cuyo constructor no terminó.

La prueba `nsc.sdk.shared-static-init-cleanup` verifica las cuatro rutas: su
IR debe enviar la llamada a un cleanup que contiene la liberación del bloque,
y cada programa captura la excepción y termina con 42 en O0 y O2. Con las
fuentes anteriores del SDK la regresión falla porque esa llamada no tiene
cleanup (`A:/nsc-fixes-evidencia/static-factory-baseline/result.log`). El build
incremental y las pruebas dirigidas de limpieza, manejadores internos y
fábrica throwing pasaron 3/3 (`shared-static-init-cleanup-ctest.log`). El caso
de uso público normal pasó desde fuentes (`public-factory-safe-after-static-cleanup.log`).

La comprobación no mide la liberación en ejecución ni acredita todos los tipos
de excepción. Si una excepción implícita ocurre después de construir campos
propietarios dentro del constructor de instancia, liberar el bloque por sí solo
no demuestra que se limpien esos campos. Las bases no triviales, el consumo
binario, la vida del módulo y la transferencia pública entre hilos siguen
pendientes en esta campaña.
