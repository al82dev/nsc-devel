# Tanda 01.10 — Escritura transaccional de Host.Basic

## SCOPE LOCK local

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\nsc-devel` |
| Componente | `Host.Basic` |
| Contrato | `CB-BAS-015`, escritura transaccional de un archivo |
| Casos | `TB-BAS-078` a `TB-BAS-082` |
| Código de producto | Sólo compilar y observar `ArtifactTransaction.cpp`; no modificar |
| Pruebas históricas | Las 684 quedan protegidas; no modificar ni ejecutar |
| Configuraciones | Debug y Release del ejecutable consolidado |
| Acciones prohibidas | NSC, NSLink, baseline, cobertura, despliegue, remoto y `C:` |

## Contratos ejercitados

`TB-BAS-078` publica un span vacío en un destino nuevo y comprueba un archivo regular de cero bytes. El caso
`TB-BAS-079` usa NUL embebido, valores fuera de ASCII y un NUL interior adicional para demostrar que el writer
binario no traduce contenido. `TB-BAS-080` pasa CR, LF y UTF-8 a `writeTextTransactionally` y compara los bytes,
no una interpretación textual.

`TB-BAS-081` conserva un basename inválido para Win32. La transacción puede crear su raíz y directorio de
entrada, pero el stream no puede abrir el stage; el oráculo exige `false` y el prefijo estable
`cannot open staged artifact`.

`TB-BAS-082` crea primero un destino válido con los bytes `original`. Durante una única llamada instala un
fault injector local al proceso sobre el slot `WriteFile` de la tabla de imports del propio ejecutable `/MT`.
La intercepción consulta el nombre final del handle y rechaza exclusivamente el basename
`finish-failure.bin`; todas las demás escrituras delegan a la dirección original. La restauración ocurre antes
de ejecutar aserciones o emitir resultados. El caso demuestra que la apertura sí ocurrió, que el fallo pasó
por la escritura o cierre, que el error comienza con `cannot finish staged artifact` y que el destino anterior
conserva exactamente sus bytes.

La inspección de PE y el reemplazo del slot son iterativos. El recorrido cuesta O(imports) una sola vez; la
clasificación de la escritura cuesta O(longitud de ruta) sólo mientras el fault injector está activo. No se
modifican módulos del sistema, otros procesos, dispositivos DOS ni código de producto.

## Integración

- `BasicTransactionalWriterModule.cpp` registra los cinco casos en el host consolidado.
- Los cuatro tipos auxiliares tienen un encabezado por tipo; las funciones de ruta permanecen en un quinto
  encabezado sin tipos, por lo que el módulo no concentra ownership, entorno, limpieza e inyección en una sola
  unidad.
- El manifiesto conserva perfil, clase, entradas y oráculo por ID.
- El gate dedicado selecciona `CB-BAS-015` con perfil `WEEKLY` y exige `TB-BAS-078..082` en orden.
- El gate PR general incorpora `TB-BAS-078..080` porque esos tres casos pertenecen al perfil diario.
- CMake mantiene un único ejecutable host y añade un noveno gate de controlador.

## Alternativas descartadas

Un primer experimento con un alias `COM9` demostró que un nombre DOS definido dinámicamente no intercepta ese
basename dentro de la ruta ordinaria que abre el CRT. La llamada publicó el archivo normalmente; la variante
se retiró completa antes del cierre.

También se evaluó redirigir el dispositivo reservado `NUL`. Esa alternativa fue rechazada antes de ejecutarse
porque la definición sería visible para otros procesos de la sesión y podría sobrevivir a un crash. No quedó
ninguna llamada a `DefineDosDeviceW`, pipe, mutex global ni nombre reservado en la implementación final.

No se añadió una seam al API de producto, no se simuló el resultado y no se llenó el volumen para provocar un
error. El hook final modifica temporalmente una sola celda del ejecutable de prueba y verifica mecánicamente
que la escritura seleccionada lo alcanzó y que la celda fue restaurada.

## Robustez, ownership y limpieza

Los handles Win32 pertenecen a `Win32Handle`. Los árboles de caso registran hojas y padres y los eliminan en
orden descendente de longitud, sin recursión. `TEMP` y `TMP` se restauran a su estado previo aunque una aserción
falle. El fault injector no conserva recursos externos y su fallback deja la función original disponible aun
si una restauración de protección fallara; el caso exige que la restauración normal sea exitosa.

El módulo no es thread-safe mientras el hook está activo y no pretende serlo: el registro consolidado ejecuta
un caso a la vez dentro del proceso. Debug y Release usan procesos distintos, por lo que no comparten tabla de
imports ni estado mutable.

## Evidencia de cierre

- Casos dirigidos `TB-BAS-078..082`: 5/5 aprobados en Debug.
- Pruebas Python del controlador: 51/51 aprobadas.
- Debug: build con warnings como errores y CTest de la suite nueva 9/9 aprobado.
- Release: build con warnings como errores y CTest de la suite nueva 9/9 aprobado.
- Gate aislado de `CB-BAS-015`: 5/5 en ambos modos, incluido dentro de los conteos anteriores.
- Análisis `clang-tidy` con `clang-analyzer-*` y `bugprone-*`: 0 hallazgos en código propio; 283 advertencias de
  dependencias o cabeceras del sistema fueron identificadas y suprimidas por la herramienta como código ajeno.
- Imports dinámicos de VCRUNTIME, MSVCP o UCRT: 0 en Debug y 0 en Release.
- Residuos de fixtures y staging en la raíz autorizada: 0.
- Diferencial de código de producto bajo `src` e `include/nativesharp`: 0 archivos.
- Recursión añadida: 0.
- Líneas mayores a 120 caracteres en código y documentación nuevos: 0; el manifiesto conserva JSONL canónico.

La validación fue exclusiva de la suite nueva. No se ejecutaron el compilador NativeSharp, las 684 pruebas
históricas, minibaselines, baseline, cobertura, instalación ni despliegue.
