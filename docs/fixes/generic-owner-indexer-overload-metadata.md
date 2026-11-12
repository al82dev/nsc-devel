# Clausura de métodos para indexadores sobrecargados

## Reproducción y causa

En `12c3975b9`, un productor declara `GenericLookup<N>` con indexadores
`int` y `long`, ambos devuelven `Fixture.Own<N>`, y una llamada genérica
`this[0].At<20, 42>()`. Si otro tipo declara un método `At` homónimo, la
publicación estática rechaza la clausura tanto en O0 como O2. La fuente,
comandos y diagnósticos están en
`A:/nsc-fixes-agente-2-evidencia/indexer-overload-closure-before/`.

El ajuste anterior solo obtenía la dependencia cuando existía un indexador.
El colector ahora revisa los getters con la aridad escrita y, si todos sus
retornos se resuelven a la misma declaración genérica, conserva los métodos
de esa declaración. No selecciona una sobrecarga: la resolución semántica
posterior sigue siendo responsable de la llamada. En el caso de sobrecargas,
getters con parámetros opcionales, variádicos o por referencia, retornos
desconocidos o declaraciones de retorno distintas mantienen la ruta anterior.
El tratamiento anterior para un indexador único se conserva.

## Validación y límites

El mismo productor se publica en O0/O2 después del cambio; véase
`indexer-overload-closure-after-final/results.log` en la carpeta de evidencia. La
regresión `generic_owner_indexer.py` produce y consume el productor amplio
en estática y DLL con productor y consumidor O0/O2. Incluye homónimos, un
control de método inexistente, retornos a tipos distintos que se conservan
sin resolver y las pruebas anteriores de indexadores. Pasaron además diez
regresiones dirigidas; logs en `generic-indexer-overload-closure-final/`.

El ejecutable experimental combina tres objetos de la copia con bibliotecas
del build de campaña; `build.log` conserva comandos y hashes. No certifica un
build limpio y falta la validación integrada. No se demuestra selección
general de sobrecargas, herencia ni publicación cuando los tipos de retorno
son distintos; la ruta anterior también puede tener otros límites.
Verificación integrada, 2026-11-11: la compilación incremental y
`nsc.generics.owner-method-indexer` pasaron; CTest ejecutó 1/1 sin omisión.
El log `A:\nsc-fixes-evidencia\generic-indexer-overload-metadata-20261111.log`
conserva las variantes positivas y rechazos esperados. El compilador contenía
otros cambios locales y no acredita un árbol limpio. Los retornos diferentes
y la herencia permanecen fuera de la cobertura de este cambio.
