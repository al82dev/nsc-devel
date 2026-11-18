# Despacho de override sellado

El informe observó que `RunSealedOverride` del ejemplo 43 conservaba una
llamada indirecta en O2. El ejemplo describe ese resultado como observación,
no como diferencia funcional. Se adopta la optimización para el caso probado:
si el método seleccionado es `sealed` y su destino HIR está disponible, la
llamada de instancia usa el destino directo. El chequeo del receptor nulo,
argumentos y excepciones se conserva en la ruta directa. Una llamada a un
método virtual abierto continúa usando el slot; las interfaces no cambian.

Antes del cambio, `sealed-dispatch-probe/baseline-mir.log` muestra llamadas
virtuales tanto al override sellado como al método base abierto. Tras el
cambio, `sealed-dispatch-probe/original-mir.log` muestra una llamada directa
al `Mix` sellado dentro de `RunSealedOverride`, con los fuentes originales de
la reproducción. El proyecto de la reproducción referencia el Core de esta
copia; este volcado no es una medición de rendimiento ni una ejecución del
benchmark.

La compilación incremental del objetivo `nsc` pasó. Las regresiones dirigidas
`nsc.compiler.sealed-override-dispatch` y su variante `-binary` pasaron 2/2
en `A:/nsc-fixes-evidencia/sealed-dispatch-probe/ctest.log`. Comprueban MIR,
despacho abierto y override sellado desde fuentes y biblioteca estática
NativeSharp; el productor y el consumidor binarios se cruzan en O0/O2. El
control desde fuentes ejecutó una subclase, captura de excepción y llamadas
con receptor nulo: la llamada sellada y la abierta terminaron con el mismo
código de trap 135 en O0/O2 (`test.log`). No se ha medido rendimiento ni
acreditado DLL, interfaces o desvirtualización por el tipo de clase sellada
cuando el método seleccionado no está sellado.

El compilador y el Core se validaron con otros cambios locales presentes; esta
evidencia no certifica por sí sola una revisión limpia ni cierra la campaña.
El archivo MIR se comparte con otras ramas y puede requerir reconciliación
posterior. No se ha ejecutado una batería completa.
