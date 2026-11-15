# NSMeta: pruebas unitarias por contenido, no por etiqueta (NSC-META-001)

## Problema

Después de convertir `stage48_2.library-metadata` y `stage50_9.metadata-host-compatibility`
(commit 3d941258d), quedaban 16 suites unitarias que todavía esperaban que `formatMinor` seleccionara
reglas:

- que el escritor rechazara un modelo etiquetado con una versión anterior (1.16, 1.26 a 1.42);
- o que el lector devolviera la etiqueta decodificada.

El códec ya no codifica esa etiqueta y siempre escribe la estructura completa. Esas expectativas
fallaban por la etiqueta, no por el contenido.

## Decisión

Las pruebas comprueban lo que se escribe y se lee:

- Una etiqueta numérica, por sí sola, no rechaza nada ni borra contratos. El archivo escrito con la
  etiqueta es idéntico byte a byte al escrito sin ella (`metadataLabelChangesNothing`).
- Cada contrato que antes "no se podía representar" bajo la etiqueta se escribe, se lee y se conserva:
  volatile, noheap, marcador de portador anónimo, variantes, ítems asociados, extensiones, resultados
  opacos, traits, dyn, lifetimes, Copy, drop glue, resúmenes de escape, throws tipados, contratos de
  raíz, receptores candidatos e imports de plantillas. Cuando el modelo trae huella ABI, la huella
  decodificada coincide (`metadataSurvivesLabel`).
- Los modelos sin un contrato, que antes se etiquetaban como versiones anteriores, se leen por su
  contenido. Ejemplos: sin drop glue, sin receptores, sin imports o sin resúmenes de escape. La
  comprobación de la etiqueta decodificada se sustituye por la comprobación del contenido.
- El resumen de escape ausente se representa en el archivo como `Unknown` para cada parámetro. Es el
  contrato conservador que hidrata el consumidor, y la prueba lo verifica explícitamente.
- Los controles negativos que ya existían se conservan sin cambios: contratos de ciclo de vida para
  tipos desconocidos, recuento de escape distinto de la firma, ámbitos inválidos, etc.

Los dos ayudantes están en `tests/support/MetadataLabelControl.h`. Los mensajes de las pruebas nuevas
describen el criterio por contenido.

## Validación (compilación incremental, ejecutables dirigidos)

| Suite | Resultado |
|---|---|
| cross_library_inlining, item308, item313, item322 | 0 fallos |
| library_metadata_version_tolerance, namespace_function_metadata, namespace_state_model | 0 fallos |
| ns3 t02, t03, t05 (associated, extension, opaque, trait declaration, trait object) | 0 fallos |
| ns3 t06 (drop glue, escape summary, lifetime, move only), t07 error, t12 generic ownership | 0 fallos |
| item326 anonymous member promotion | 36/37: queda el fallo descrito abajo |

## Límite pendiente (fuera de alcance)

Item326 "LibraryReference reconstructs recursive promoted aliases…" falla con NS3020 "parameter 1 has
an unknown escape contract". No depende de NSMeta, porque el consumidor usa el archivo original del
productor:

- El productor publica `Unknown` para `export extern C int Read(Packet value)`.
  `initializeParameterEscapeSummaries` trata cualquier `isExternC` como conservador, aunque tenga
  cuerpo.
- El consumidor materializa el método exportado como un método NativeSharp precompilado. Al recibir
  un `struct` local por valor, lo rechaza.

La corrección toca la semántica de funciones `extern C` exportadas (interoperabilidad C), así que
queda registrada y no se modifica aquí.

No se cambia código del compilador. Las suites de interoperabilidad (Item331, Item344,
`compiler-suite/host/modules/Interop*`) quedan fuera de esta tarea.
