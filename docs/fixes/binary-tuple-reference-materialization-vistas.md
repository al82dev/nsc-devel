# Firmas binarias de tuplas por referencia

## Alcance y estado

Esta corrección pertenece al trabajo de vistas y punteros en la rama
`development-fixes-limitaciones-vistas`. La base de referencia del equipo es
el checkpoint WIP `d00b3a8698d7750c80887d029d64fda481d67211`, que no está
certificado. No se modifican interoperabilidad C/C++, `src/Interop`, otras
copias, el toolchain ni la instalación en C:. Solo se utilizan compilaciones
incrementales y pruebas dirigidas en A:; no se ejecutó batería completa.

El coordinador de este chat autorizó y coordinó expresamente el uso de
`PublicSurfaceBuilder.cpp` para esta causa; no se atribuye esa coordinación al
integrador de la otra rama. Aunque se comprobó su coincidencia con el
checkpoint común en las otras ramas al reservarlo, es un archivo compartido
que podría entrar en conflicto al integrar cambios posteriores.

## Problema y causa

Una biblioteca que declara un parámetro `in` o `ref` de una tupla con vista
publicaba el miembro en `.nsmeta`, pero el consumidor no lo encontraba. En la
reproducción dirigida, las mismas llamadas se analizaban en fuente; controles
binarios con un método estático ordinario y con vista escalar sí resolvían.
El registro previo a la corrección está en
`A:/nsc-vistas-evidencia/metadata-signature-control-fr74j4h_/results.json`.

La clave interna de la tupla por referencia contiene un `*` final. El mapeo
consultaba si la clave completa era tupla, pero esa comprobación exige un `)`
final; después, el fallback no reconstruía el tipo `tuple:`. Esto hacía que la
comprobación de materialización descartara la declaración. Al reconstruirla,
el texto directo `in (T, U)` tampoco era aceptado por el parser: un alias de
tipo sí funciona. La observación y cadena estática están en
`A:/nsc-vistas-evidencia/metadata-tuple-ref-hallazgo.md`.

## Cambio

`MetadataSourceMapping.cpp` reconoce el portador de un solo puntero sobre una
tupla reconstruible y conserva ese portador. `PublicSurfaceBuilder.cpp`
genera un alias de tipo determinista basado en SHA-256 de la clave de la tupla
para escribir las firmas por referencia con un nombre aceptado por el parser.
El hash identifica el alias generado: no autentica metadata ni binarios.

`EscapeAnalyzer.cpp` conserva la barrera para las tuplas con vistas por valor y
la aplica también a los parámetros por referencia de métodos precompilados.
La presencia de la firma no demuestra la procedencia de sus vistas: sin un
contrato productor-binario verificado se rechazan también usos válidos de esos
parámetros. Esto es una limitación conservadora, no soporte completo de vistas
binarias. No se altera `Unknown` para convertirlo en prueba de no escape.

## Verificación dirigida

La compilación incremental y generación del Core de la copia terminaron con
código 0. El compilador probado tuvo SHA-256
`a29e639a62d0d7b47780d47d35563b3959072c794e325441e443b31cc730b363` y Core
`565bf2177c85f88d94057ebe8616f2caeb71e01b7e39610bf815c15b685221c6`.

- `A:/nsc-vistas-evidencia/binary-tuple-modes-7fb8v27c/results.json`: 48
  análisis completados; 24 casos binarios alcanzaron el diagnóstico de barrera
  y no el de miembro incompatible. Los controles fuente vivos se aceptaron y
  los escapes/reemplazos fuente se rechazaron. No se ejecutaron consumidores.
- `A:/nsc-vistas-evidencia/metadata-signature-control-39jfgyma/results.json`:
  los controles `Ping` y `Scalar` resolvieron desde binario; la tupla con vista
  alcanzó la barrera.
- `A:/nsc-vistas-evidencia/numeric-tuple-control-_44srxgo/results.json`:
  tuplas numéricas `in`, `ref` y `out` compilaron y devolvieron 42 en O0/O2.
- `A:/nsc-vistas-evidencia/binary-shared-view-value-after.log` y
  `primitive-tuple-value-after.log`: pasaron los controles existentes por valor,
  anidados y de nombres homónimos. Las copias de ejecución y sus diffs en A:
  cambian solamente el cwd de los subprocesos a la raíz del repositorio.
- `A:/nsc-vistas-evidencia/shared-binary-pointer-after.log`: la prueba dirigida
  de punteros compartidos binarios terminó con código 0.
- `A:/nsc-vistas-evidencia/tuple-view-dispatch-p5q49x9c/results.json`:
  namespace, instancia e interfaz con entradas por valor, `in` y `ref`
  alcanzaron la barrera en 27 análisis binarios; los casos fuente de escape o
  reemplazo se rechazaron. Los productores para `out` de una tupla con vista
  fallaron antes del consumidor con `tuple containing a borrowed view cannot
  be stored outside a tracked local`. Ese guard permanece sin debilitar y la
  barrera binaria para `out` no queda verificada por esta selección.
- `A:/nsc-vistas-evidencia/tuple-alias-collision-sl19pqn_/results.json`:
  dos bibliotecas con la misma tupla numérica y un tipo público que comparte el
  nombre sintético se analizaron en los casos probados. Un control con función
  interna de namespace compiló, pero el nombre de esa función no se encontró en
  su `.nsmeta`; no demuestra el tratamiento de metadata que sí la contenga.

## Pendientes y límites

La prueba `tests/integration/vistas_binary_tuple_modes_probe.py` conserva la
matriz dirigida y registra comandos, resultados y hashes; sus programas
potencialmente inseguros se analizan, no se ejecutan.

El control `A:/nsc-vistas-evidencia/generic-local-view-bsqe7so2/results.json`
produjo una biblioteca con un método genérico y un `borrowed struct` local;
el análisis aceptó las formas de escape y reemplazo tanto en fuente como en
binario. Son aceptaciones potencialmente inseguras; no se ejecutaron los
programas. Ese resultado corresponde a un estado anterior. Los volcados
`A:/nsc-vistas-evidencia/generic-local-post-dump-compilation.log` y
`generic-local-post-dump-0.log` identifican una especialización procedente de
una unidad de plantilla reconstruida.

En un contraste incremental posterior se retiró temporalmente el diferencial
de los tres archivos de materialización y se restauró, manteniendo los demás
cambios idénticos mediante comprobación de hashes de otros 11 archivos. Sin el
diferencial (`generic-local-view-uvhc2k2k/results.json`) y con él
(`generic-local-view-f4mu3pb6/results.json`) se obtuvieron las mismas seis
clasificaciones: ambos casos vivos aceptados y los cuatro análisis de escape
y reemplazo rechazados. No se ejecutaron los negativos. Las dos ejecuciones
generaron productor y consumidor con la misma variante, por lo que no aíslan
por separado publicación y consumo; además, ambas incluyen los otros cambios
presentes en el árbol. Esta comparación no cubre todos los genéricos ni
demuestra seguridad general. La procedencia explícita de plantillas binarias
sigue pendiente.

Un productor genérico cuyo alias de tupla contiene `SharedView` externo pasó
el análisis de cuerpos, pero falló al generar metadata; los controles con tipo
básico, vista directa y alias de tupla numérica sí produjeron biblioteca.
Comandos y resultados están en
`A:/nsc-vistas-evidencia/generic-type-origin-p8guf1sd/results.json` y
`generic-type-origin-source-check.log`. Los consumidores de ese productor
permanecen bloqueados; no se modificó `src/Interop`.

También quedan pendientes la
procedencia de vistas a través de bibliotecas y salidas compuestas, el `out`
antes descrito, los artefactos históricos y DLL, y el contrato que relaciona
el cuerpo analizado con el binario efectivamente enlazado. La propuesta de
investigación está en
`A:/nsc-vistas-evidencia/propuesta-procedencia-binaria-vistas.md`.

La evidencia dirigida no acredita seguridad general, compatibilidad completa,
ni cierre de toda la campaña. Debe revisarse el diff independientemente antes
de integrar la corrección.

`clang-tidy` con `clang-analyzer-core.*` terminó con código 0 sobre los tres
archivos de la corrección sin diagnósticos, según el log
`A:/nsc-vistas-evidencia/binary-tuple-clang-tidy.log`. Es una pasada dirigida;
no demuestra ausencia de defectos en otros analizadores o en todo el producto.
# Validación en la rama integradora

Tras integrar `a992cacb7`, la compilación incremental del compilador pasó.
La prueba observacional de tuplas se ejecutó con el compilador de
`A:/nsc-fixes-evidencia/build/bin/nsc.exe` y el Core previamente construido
en `A:/nsc-fixes-evidencia/integracion-vistas-sdk/`. El wrapper externo solo
adapta la ruta autorizada del compilador; no cambia las fuentes de los casos.
El productor compiló y se ejecutaron los 48 análisis: ocho positivos fuente
aceptados, ocho positivos binarios rechazados por la barrera conservadora y
32 negativos rechazados. No hubo timeouts ni se ejecutaron negativos. Un
rechazo no demuestra por sí solo seguridad. El resultado detallado y los
hashes de compilador, Core y metadata constan en el JSON señalado por
`A:/nsc-fixes-evidencia/tuple-binary-integration-20261112.log`.

Ese Core tiene procedencia documentada y se había usado en pruebas de
punteros, pero no se ha demostrado que corresponda al HEAD actual. Los ocho
positivos binarios rechazados y la procedencia entre módulos siguen pendientes;
esta ejecución no equivale a soporte completo de tuplas prestadas binarias.

