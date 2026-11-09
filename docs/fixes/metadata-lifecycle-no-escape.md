# Contrato binario de no escape del lifecycle

## Motivo y alcance de esta etapa

El productor de una clase precompilada puede publicar su receptor en un constructor o destructor. El consumidor no dispone necesariamente de esos cuerpos. Las reproducciones y la restricción provisional están descritas en `publicacion-receptor-punteros.md`: se rechazó también una clase binaria segura, y una base precompilada todavía puede publicar el receptor de una clase declarada en fuentes.

La metadata NativeSharp dispone desde 1.43 de una garantía de ausencia de limpieza para ciertas raíces. Esa garantía no acredita que su constructor no publique `this`. La investigación de `A:\nsc-fixes-evidencia\core-lifecycle-mir.log` muestra un constructor raíz vacío en las fuentes examinadas, pero esa observación no certifica un binario arbitrario. El resumen existente de escape describe parámetros explícitos y no constituye una prueba del receptor implícito.

## Representación 1.44

La metadata 1.44 añade `nonEscapingLifecycleTypeKeys`, un catálogo de identidades de clases al que el productor podrá agregar una garantía cuando exista un verificador adecuado. La ausencia de identidad significa **desconocido**. El codec serializa el catálogo solo desde 1.44 y rechaza escribirlo en una versión anterior; las versiones anteriores se leen con el catálogo vacío. La mezcla lo ordena y deduplica. La verificación rechaza identidades inexistentes, tipos que no son clases, clases estáticas, duplicados y contratos con versión anterior a 1.44.

El productor acredita ahora un subconjunto deliberadamente estrecho: clases fuente raíz no estáticas con constructor MIR disponible, sin campos salvo tipos escalares admitidos y con todos sus constructores y destructores en una forma simple sin llamadas. Solo se admiten bloques, sentencias, retornos sin publicar el receptor y escrituras de literales en campos directos del propio objeto. Cualquier nodo no reconocido, dirección usada fuera del destino de una escritura, cuerpo ausente, campo no escalar o base deja el resultado como desconocido. El análisis agrupa los cuerpos y detecta declaraciones sin cuerpo antes de recorrer los tipos; cada cuerpo aceptado se revisa con una pila explícita y sin recursión. Es una prueba conservadora de esta forma concreta, no un análisis general de escape.

**El consumidor todavía no usa las garantías.** La contención que rechaza clases de carga precompiladas se conserva, y la publicación por bases precompiladas sigue abierta. No se habilita `Send` ni `Sync` por este cambio.

## Evidencia y siguientes pasos

`A:\nsc-fixes-evidencia\build-lifecycle-metadata.cmd` compiló incrementalmente `nsc` y la prueba dirigida con identidad `3.0.0+build.20261109T213916Z.dfac470fe0f472c1`. `A:\nsc-fixes-evidencia\lifecycle-metadata-test.cmd` ejecutó `nsc.ns3.t12.exported-generic-ownership`: 30 comprobaciones aprobadas, incluidas la escritura y lectura 1.44, rechazo de escritura en 1.43 con datos, lectura 1.43 sin garantía, identidad inválida y mezcla. El build coexistió con cambios ajenos sin registrar; repetir la validación sobre la revisión integrada. No se verificó compatibilidad de consumidores antiguos que solo admitan hasta 1.43.

El build incremental final `3.0.0+build.20261109T214558Z.6b73e1370617e667` y la prueba dirigida del productor pasaron: `lifecycle-metadata-test.log` registra 31 comprobaciones. La fixture acredita una raíz vacía y otra con campo escalar, y no acredita publicación en constructor o destructor, llamada opaca, clase derivada ni campo de referencia. La compilación coexistió con cambios ajenos sin registrar; repetir sobre la revisión integrada.

Falta ampliar la prueba conservadora para bases y dependencias, transportar la garantía al modelo del consumidor y usarla solo después de demostrarla. Deben cubrirse cuerpos ausentes, versiones antiguas, operaciones indirectas, inicializadores, excepciones y seguridad de la cadena de bases; no se puede inferir seguridad por ausencia de un cuerpo ni a partir del contrato de cleanup. Los archivos `include/nativesharp/Interop/LibraryMetadata.h` y `src/Interop/LibraryMetadata*.cpp` pertenecen a la metadata NativeSharp y pueden coincidir con la rama de interoperabilidad, aunque esta etapa no modifica la funcionalidad de interoperabilidad C/C++.
