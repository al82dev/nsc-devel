# Tanda universal 48: selección explícita de perfiles self-host

## SCOPE LOCK

- Objetivo: hacer que la batería CTest respete literalmente los perfiles seleccionados en NSC
  Builder, comenzando por O0.
- Naturaleza: corrección de infraestructura y trazabilidad de ejecución.
- Comportamiento protegido: una certificación sin filtro continúa validando O0/O1/O2/O3; los
  contratos funcionales y sus expectativas no se reducen.
- Archivos protegidos: compilador, parser, semántica, lowering, runtime, bibliotecas, instalación
  `C:` y fixtures del lenguaje.
- Prohibido: baseline certificado, deploy, ejecución O1/O2/O3 en la tarea vigente, ocultar perfiles
  no ejecutados o presentar una selección O0 como certificación completa.
- Terminación: el selector se propaga al proceso CTest, forma parte de la identidad de caché y cada
  verificador self-host consume exactamente el subconjunto pedido.

## Causa raíz

`RunPlan.optimizations` conserva la selección de la GUI para fixtures y bug triage, pero `_ctest`
no la incorporaba al entorno. Los gates SH40–SH50 volvían a leer listas fijas de cuatro perfiles en
sus contratos. Por eso una corrida solicitada como O0 ejecutaba silenciosamente O1/O2/O3 y no podía
distinguir un defecto O0 de un cierre perteneciente a otra matriz.

## Primera parte

1. `profile_selection.py` define una selección ordenada, sin duplicados y validada contra el
   contrato. Sin variable de entorno devuelve el conjunto completo.
2. NSC Builder publica `NSC_SELFHOST_PROFILES` desde `RunPlan.optimizations`.
3. La selección entra en la identidad de caché CTest; un resultado de cuatro perfiles no puede
   reutilizarse como resultado O0 ni al revés.
4. Las pruebas unitarias cubren el valor predeterminado, O0, rechazo de perfiles inválidos y la
   propagación desde NSC Builder.

## Archivos

- Nuevo: `selfhost/bootstrap/profile_selection.py`.
- Modificado: `nscbuilder.py`.
- Modificado: `tests/selfhost/test_bootstrap_infrastructure.py`.
- Nuevo: `cmake/NativeSharpSelfHostProfiles.cmake`.
- Nuevo: `cmake/NativeSharpSelfHostRuntime.cmake`.
- Modificado: `CMakeLists.txt` para entregar explícitamente el runtime y el ABI generados.
- Modificados los wrappers CMake SH40-SH44 para materializar únicamente los candidatos
  seleccionados.
- Modificados los verificadores SH40-SH48, SH50 y SH52 para consumir y publicar el conjunto
  seleccionado.
- Modificados los verificadores anidados SH05-SH07 para aceptar los candidatos explícitos que
  les entrega SH42, sin exigir O2 durante una corrida O0.
- Nuevo: este documento.

## Segunda parte

1. SH40 y SH41 recorren la selección del entorno en vez de las cuatro optimizaciones del
   contrato.
2. SH42, SH43 y SH44 reciben únicamente los ejecutables producidos por SH41 para los perfiles
   seleccionados; sus argumentos ya no exigen imágenes ausentes de O1, O2 u O3.
3. SH05 y SH06 conservan la comparación histórica O0/O2 cuando no existe filtro. Cuando el
   ejecutor declara una selección, comparan exactamente sus candidatos y no inventan perfiles.
4. SH07 conserva y valida el contrato completo de pipelines, pero ejecuta la matriz estructural,
   de estabilidad y SSA sólo con los candidatos seleccionados.
5. SH45, SH46 y SH48 calculan sus conteos esperados desde la selección real; no reutilizan el
   total contractual de cuatro perfiles para una corrida O0.
6. SH50 mantiene el presupuesto absoluto de O0. La razón O0/O2 y la distinción entre objetos
   sólo se exigen cuando esos perfiles forman parte de la misma ejecución.
7. SH52 conserva su comportamiento histórico sin filtro. En una selección O0, cancelación,
   publicación atómica y reproducibilidad se ejercitan con O0 y no invocan O2.
8. Los reportes conservan el conjunto realmente ejecutado. Una corrida O0 no se presenta como
   certificación multiperfil.

## Frontera de runtime del self-host

La primera matriz dirigida posterior al commit de implementación se detuvo antes de invocar el
compilador self-host. SH41, SH43, SH44 y SH48 deducían la biblioteca runtime desde la carpeta de
`nslink.exe`; al seleccionar el NSLink de desarrollo terminaban buscando bajo
`A:\nslink-devel\bin\lib`. Los diez resultados fueron de infraestructura: SH41 no encontró el
runtime y los otros nueve no encontraron el `nsc2-o0` que SH41 debía producir.

La resolución queda centralizada en `NativeSharpSelfHostRuntime.cmake` con esta precedencia:

1. `RUNTIME_LIBRARY` y `RUNTIME_ABI` entregados por la configuración CMake;
2. `NSC_WINDOWS_RUNTIME_LIBRARY` y `NS_LIB_ROOT` del entorno de ejecución;
3. prefijo de NSLink únicamente como compatibilidad final.

`CMakeLists.txt` entrega de forma explícita los artefactos generados por el build actual a SH41,
SH43, SH44 y SH48. Cambiar el ejecutable NSLink ya no cambia silenciosamente el runtime que se
está probando.

## Complejidad

La selección recorre una vez el conjunto de perfiles, como máximo cuatro elementos. Tiempo y
memoria son `O(P)`; no existe recursión.

## Estado

- Selector y propagación: implementados.
- Consumo por los verificadores SH40–SH52: implementado; SH49 y SH52 heredan además la selección
  en sus invocaciones CTest anidadas.
- Validación dirigida posterior al commit `31378cec1`: `3/3` pruebas unitarias aprobaron en
  `0.006 s`; cubren el valor completo predeterminado, la selección O0, el rechazo de O4 y la
  exportación desde NSC Builder.
- Validación de la segunda parte: pendiente hasta conservar primero el commit de implementación.
- Primera matriz dirigida de diez pruebas: `0/10` alcanzaron ejecución funcional; el bloqueo de
  infraestructura se confirmó en `0.31 s` por la ruta runtime derivada de NSLink. No se presenta
  como resultado funcional.
- Inventario integral vigente: diez fallos self-host abiertos.
