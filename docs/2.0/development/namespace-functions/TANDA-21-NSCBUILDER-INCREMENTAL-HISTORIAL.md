# Tanda 21: incremental visible e historial de NSCBuilder

## Objetivo

Hacer visible el comportamiento incremental real de `nscbuilder.py` y convertir la evidencia ya conservada en un historial utilizable desde la interfaz, sin limpiar ni modificar las corridas anteriores.

## Diagnostico

El motor ya construia incrementalmente, pero la interfaz no mostraba las corridas anteriores ni separaba el costo de preparacion del costo de ejecutar pruebas. Eso hacia parecer que las 2 horas y 25 minutos de Gate 0 correspondian a una reconstruccion completa.

La corrida aprobada `20260928-001159-28d2391b` demuestra lo contrario:

- preparacion incremental total: 22.100 s;
- ejecucion de CTest, fixtures y bug triage: 8648.385 s;
- certificacion final: 18.921 s;
- tiempo total: 8708.282 s.

Los planes efectivos de compilador y arboles CTest estaban marcados como `incremental`. El tiempo dominante era la matriz completa, que debe volver a ejecutarse en un baseline certificado.

## Cambios

- Cada corrida futura escribe `summary.json`, un indice pequeno con estado, duracion, fallos, modos, enlazadores y desglose de tiempos.
- La lectura del historial admite las corridas antiguas que solo tienen `metadata.json` y las convierte en memoria sin reescribir su evidencia.
- La interfaz agrega una pestana `Historial` con corrida, inicio, estado, operacion, tiempo total, preparacion, pruebas, fallos, modo y enlazadores.
- Un doble clic abre el `report.md` conservado; si no existe, abre el directorio de evidencia.
- El historial se actualiza al iniciar la interfaz, manualmente y al terminar una corrida.
- Gate 0 explica que conserva los tres arboles de build y prepara incrementalmente, pero ejecuta de nuevo toda la matriz porque no reutiliza resultados para certificar.
- `Limpiar consola` no actua cuando la pestana seleccionada es el historial.

## Archivos modificados

- `nscbuilder.py`: contrato compacto, lector retrocompatible, escritura por corrida e interfaz de historial.

## Archivo nuevo

- `docs/2.0/development/namespace-functions/TANDA-21-NSCBUILDER-INCREMENTAL-HISTORIAL.md`: documenta el diagnostico, alcance, archivos y verificacion.

No se crearon directorios de trabajo ni se alteraron `_nscbuilder/history`, `_nscbuilder/work`, `_nscbuilder/temp` o `_abi/gates/G00`.

## Mejora obtenida

El usuario puede comprobar si una corrida fue incremental y distinguir inmediatamente tiempo de preparacion y tiempo de pruebas. Las 18 corridas existentes aparecen sin migracion destructiva y las nuevas tienen un resumen rapido que evita cargar `metadata.json` de cientos de kilobytes.

La semantica de Gate 0 no cambia: incremental se refiere a compilacion y preparacion de artefactos; la certificacion continua ejecutando todas las pruebas seleccionadas por el contrato del baseline.

## Verificacion

- Importacion headless de `nscbuilder.py`: correcta.
- Lectura retrocompatible: 18 corridas encontradas.
- Ultima corrida detectada: `20260928-001159-28d2391b`, estado `passed`.
- Modos detectados: compilador incremental y CTest incremental.
- Desglose calculado: 22.100 s de preparacion y 8648.385 s de pruebas.
- Interfaz Qt fuera de pantalla: la pestana mostro 18 filas y la ultima como correcta e incremental.
- `python -B nscbuilder.py --help`: correcto en modo headless.
- Construccion incremental exclusiva del target `nsc`: correcta.
- No se ejecuto baseline, CTest, fixtures ni bug triage.
- No se limpiaron ni modificaron temporales o evidencia historica.
