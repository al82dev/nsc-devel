# Host.Interop — NSMETA, API/ABI, headers C y cierre de bibliotecas

## 1. Estado y procedencia

- **Clasificación:** `IMPLEMENTACIÓN VIGENTE`, salvo hallazgos y riesgos marcados.
- **Repositorio:** `A:\nsc-devel`.
- **Revisión congelada:** `3bb175443a18b2049c20d75443dd6743e2993903`.
- **Universo:** 12 archivos, 8,423 líneas y 387,816 bytes.
- **Inventario:** 178 símbolos físicos, 48 contratos y 384 casos diseñados.
- **Validación realizada:** lectura estática de los encabezados y archivos pequeños; índice AST de las seis
  implementaciones; revisión por rangos del modelo, escritor, lector y verificador NSMETA; extracción de
  diagnósticos, compuertas de formato y segunda auditoría documental.
- **No realizado:** build, ejecución, baseline, cobertura, instalación, despliegue o modificación del producto.

## 2. Frontera del componente

Host.Interop publica el contrato reutilizable de una biblioteca NativeSharp. Recibe el modelo semántico/MIR de
`Host.Frontend` y produce o consume cinco superficies observables:

1. archivos `.nsmeta` versionados y con fingerprint;
2. headers C para la ABI pública;
3. manifiestos, huellas y diferencias de API/ABI;
4. reportes y auditorías de capacidades de bibliotecas instaladas;
5. fusión determinista de fragmentos de metadata.

No enlaza objetos ni decide semántica nueva. Conserva identidades, layouts, firmas, ownership, genéricos,
dispatch, lifetimes, escape, typed throws y contratos FFI ya resueltos por productores anteriores.

## 3. Generación de headers C

`CHeaderGenerator` construye nombres C estables, separa niveles de puntero, resuelve tipos NativeSharp y calcula
el cierre de tipos alcanzables desde exports. Emite forward declarations, delegates, structs/unions, bitfields,
agregados anónimos y exports de miembros o funciones de namespace.

La salida ordena definiciones por dependencias de valor. Un ciclo por valor se rechaza; dependencias por puntero
se satisfacen con declaración opaca. También se rechazan layouts ausentes, offsets explícitos que C no puede
expresar, arrays fijos no representables y bitfields sin ancho físico. La publicación del archivo ocurre sólo
después de construir el texto completo.

## 4. Reporte ABI

`LibraryAbiReporter` imprime un formato textual `NativeSharp ABI report v1`. Incluye target, tamaños de puntero,
versiones ABI, fingerprint, tipos, campos, delegates, asociados, miembros, parámetros, dispatch y exports. Antes
de imprimir ordena las colecciones por identidad, posición o firma; el reporte no depende del orden incidental
de los vectores de entrada.

Las convenciones observables distinguen `managed`, `win64-c`, `vectorcall` y `native-win64`. Ownership y paso de
parámetro tienen vocabularios cerrados con fallback `unknown` para valores fuera del enum.

## 5. Superficies y diferencias API/ABI

`LibraryApiDiffer` construye dos mapas canónicos independientes:

- **source surface:** nombres, accesibilidad, tipos, parámetros, defaults, ownership, pass kind, lifetimes,
  escape, contratos genéricos y fuente materializable;
- **ABI surface:** layout, calling convention, símbolos físicos, dispatch, exports y versiones binarias.

Cada campo se serializa con longitud para evitar ambigüedad. Los manifiestos se ordenan; las huellas usan SHA-256.
La comparación clasifica adiciones, rupturas de fuente y rupturas ABI. Un cambio puede conservar ABI y romper
fuente, por lo que ambos resultados permanecen separados.

## 6. Auditoría de capacidades

La auditoría reconoce superficies nominales requeridas en uno o varios módulos y produce presencia y faltantes
por capacidad. La variante de directorio enumera exclusivamente `.nsmeta`, exige un directorio legible y al
menos un archivo, lee cada modelo con `LibraryMetadataCodec` y conserva el primer error causal con su ruta.

## 7. Esquema NSMETA vigente

El formato público actual es `NSMETA 1.39`. El modelo contiene identidad de productor y módulo, target/puntero,
contratos ABI y las colecciones de tipos, miembros, funciones/estado de namespace, templates, especializaciones,
tipos externos, dispatch, exports, dependencias binarias e imports de plataforma.

Los tipos conservan layout, enum subyacente, delegates, variantes, traits, asociados, dyn ABI, lifetimes,
drop-glue y reference return. Los callables conservan ownership, pass kind, defaults, params, dyn, lifetimes,
escape, typed throws y FFI. Las plantillas conservan fuente canónica, dependencias, clases de parámetros,
restricciones nominales/de igualdad/de capacidad y outlives.

El historial de comentarios del header no está ordenado cronológicamente y repite los minors 23, 34 y 38. Es
proveniencia vigente, no una tabla normativa adicional. La lógica ejecutable usa las constantes y compuertas del
códec.

## 8. Primitivas binarias y límites

`ByteWriter` escribe enteros little-endian, bytes y strings con longitud. `ByteReader` nunca avanza fuera del
span y limita strings a 16 MiB. El formato limita cada catálogo a 1,000,000 registros y el payload a 128 MiB.
El archivo tiene magic `NSMETA1\0`, marcador endian `0x01020304`, header fijo de 100 bytes, tamaño de payload,
checksum y fingerprint ABI.

Un checksum correcto sólo demuestra integridad de bytes. Después de decodificar, `verifyCurrentCompiler`
valida coherencia estructural y relacional.

## 9. Construcción del modelo

`buildMetadataModel` exige nombres, miembros, layouts y MIR completos, identifica el proyecto raíz y fija target
Windows x64. Publica sólo la superficie permitida, pero incorpora el cierre privado necesario para materializar
templates y enlazar dependencias.

La construcción calcula claves canónicas, IDs estables, flags, firmas físicas, defaults literales, fuente de
templates, closure genérica, dispatch, exports, dependencias binarias e imports. Rechaza identidades físicas
ausentes, referencias externas no canonizables, defaults no literales y cierres reutilizables incompletos.

## 10. Escritura y proyección ABI

La codificación es aditiva por `formatMinor`: cada catálogo se escribe sólo desde el minor que lo introdujo.
Antes de publicar se normalizan resúmenes de escape, se valida target y contrato ABI, y se comprueba que el minor
solicitado pueda representar todos los features presentes.

La proyección ABI elimina información sólo semántica y privada, conserva lo físicamente observable y calcula su
SHA-256. Desde 1.25 la clausura binaria privada no contamina el fingerprint ABI público. La publicación del
archivo es atómica.

## 11. Lectura y compatibilidad de formato

El lector valida tamaño, magic, header, endian, longitud, checksum y fingerprint antes de entregar el modelo.
`decodePayload` reconstruye de 1.0 a 1.39 usando las mismas compuertas aditivas; cuenta registros antes de
reservar, valida enums y longitudes paralelas, y rechaza bytes sobrantes.

Desde minor 23 existe un byte histórico de revisión de fuente. El lector actual lo consume y descarta; no usa
ese byte para rechazar una edición de lenguaje. En cambio, la verificación conserva una ventana explícita para
productores conocidos incompatibles mediante `producerVersion`; esta política es independiente del byte legado.

## 12. Verificación semántica del modelo

`verifyCurrentCompiler` comprueba formato soportado, target Windows x64, puntero 8/8, triple, ABI congelada e
identidad de módulo. Después valida, entre otros:

- flags permitidos por minor y combinaciones type/trait/variant/opaque/Copy/drop-glue;
- unicidad de tipos, miembros, símbolos, dispatch, namespace, exports, dependencias e imports;
- layouts de referencia, firmas delegate y marcadores dyn;
- tablas de dispatch, slots, métodos virtuales/abstractos y destrucción;
- templates materializables, restricciones, closure y especializaciones;
- referencias locales, externas y cerradas con su dependencia declarada;
- inline bodies, volatile, reference return, noheap, bitfields, variadics y params;
- lifetimes, outlives, escape summaries, typed throws y contratos FFI.

Los errores se producen en la frontera que posee el dato y no se sustituyen por defaults silenciosos.

## 13. Fusión de fragmentos

`LibraryMetadataMerger` exige al menos un fragmento y headers idénticos. Concatena catálogos, normaliza orden y
colapsa duplicados exactamente iguales. Un mismo key con contenido diferente es conflicto. La salida no depende
del orden de workers y borra el fingerprint de entrada para que el escritor lo recalcule sobre el modelo unido.

## 14. Dependencias y ownership

- `Host.Frontend`: modelos de proyecto, símbolos, layouts, MIR y templates.
- `Host.Language` y `Host.Sema`: tipos, ownership y escape.
- `Host.Basic`: SHA-256 y rangos de fuente.
- `Host.Runtime`: versión ABI del runtime.
- filesystem: lectura de directorios y publicación atómica de `.nsmeta`/headers.

Los modelos usan valores y contenedores propietarios. Los spans de lectura no sobreviven a la operación. Los
errores no publican modelos o archivos parciales.

## 15. Invariantes observables

1. Misma superficie lógica produce manifiesto, reporte, header, payload y fingerprint deterministas.
2. Source compatibility y ABI compatibility son decisiones independientes.
3. Un minor antiguo sólo puede escribirse si representa toda la superficie del modelo.
4. Ningún checksum sustituye la validación del modelo.
5. Duplicados exactos se colapsan; duplicados divergentes fallan.
6. Layout, orden de variant cases y slots de dispatch son observables.
7. El byte legado de revisión de fuente no selecciona semántica del lector actual.

## 16. Riesgos y hallazgos conservados

1. `LibraryMetadata.cpp` concentra construcción, códec y verificación en 5,181 líneas; una futura modularización
   debe preservar estos contratos, no sus fronteras internas actuales.
2. El comentario de minor 23 todavía habla de edición 2/3, mientras el lector descarta el byte como legado.
3. La verificación sí conserva una ventana de `producerVersion`; debe distinguirse de edición de lenguaje.
4. Los comentarios históricos de minors están duplicados y fuera de orden.
5. Header C no puede representar todos los layouts NativeSharp; los rechazos son explícitos.
6. La compatibilidad con minors anteriores amplía considerablemente la matriz del lector y escritor.

No se corrigió ninguno de estos puntos en esta fase.

## 17. Estrategia futura de pruebas

Las pruebas unitarias usarán builders de metadata mínimos y oráculos estructurales independientes. El códec se
probará por catálogo y por minor, con truncamientos en cada frontera, límites, permutaciones y round-trip. Las
pruebas de integración usarán proyectos NativeSharp mínimos para header C y construcción de metadata, sin
duplicar todo Frontend dentro de Interop.

Los casos se agrupan en un ejecutable unitario consolidado y fixtures `.ns` externos. El nightly reserva matrices
de escala, permutaciones amplias y compatibilidad de todos los minors.

## 18. Estado de cierre

Los 12 archivos y 178 símbolos están trazados a 48 contratos con 384 casos diseñados. `Host.Interop` queda
`AUDITADO` y `RECONSTRUIBLE` para la revisión congelada. Esto no afirma que las pruebas estén implementadas ni
ejecutadas.
