# Contrato binario de no escape del lifecycle

## Motivo y alcance de esta etapa

El productor de una clase precompilada puede publicar su receptor en un constructor o destructor. El consumidor no dispone necesariamente de esos cuerpos. Las reproducciones y la restricción provisional están descritas en `publicacion-receptor-punteros.md`: se rechazó también una clase binaria segura, y una base precompilada todavía puede publicar el receptor de una clase declarada en fuentes.

La metadata NativeSharp dispone de una garantía de ausencia de limpieza para ciertas raíces. Esa garantía no acredita que su constructor no publique `this`. La investigación de `A:\nsc-fixes-evidencia\core-lifecycle-mir.log` muestra un constructor raíz vacío en las fuentes examinadas, pero esa observación no certifica un binario arbitrario. El resumen existente de escape describe parámetros explícitos y no constituye una prueba del receptor implícito.

## Representación actual

NSMETA incluye `nonEscapingLifecycleTypeKeys`, un catálogo de identidades de clases cuya ausencia significa **desconocido**. El codec escribe el catálogo y admite su ausencia al final de una metadata existente sin atribuir garantías; la mezcla lo ordena y deduplica. La verificación rechaza identidades inexistentes, tipos que no son clases, clases estáticas y duplicados. Por instrucción del usuario no se introduce una nueva versión de NSMETA para este campo.

El productor acredita ahora un subconjunto deliberadamente estrecho: clases fuente raíz no estáticas con constructor MIR disponible, sin campos salvo tipos escalares admitidos y con todos sus constructores y destructores en una forma simple sin llamadas. Solo se admiten bloques, sentencias, retornos sin publicar el receptor y escrituras de literales en campos directos del propio objeto. Cualquier nodo no reconocido, dirección usada fuera del destino de una escritura, cuerpo ausente, campo no escalar o base deja el resultado como desconocido. El análisis agrupa los cuerpos y detecta declaraciones sin cuerpo antes de recorrer los tipos; cada cuerpo aceptado se revisa con una pila explícita y sin recursión. Es una prueba conservadora de esta forma concreta, no un análisis general de escape.

**El consumidor todavía no usa las garantías.** La contención que rechaza clases de carga precompiladas se conserva, y la publicación por bases precompiladas sigue abierta. No se habilita `Send` ni `Sync` por este cambio.

## Evidencia y siguientes pasos

El diseño inicial introdujo una numeración nueva, que se retiró al recordar el usuario que el producto mantiene un único estado en evolución. El catálogo no modifica la huella de la ABI física; sí forma parte del contenido de metadata. La compilación incremental terminó y `lifecycle-metadata-test.cmd` pasó 30 comprobaciones, incluida la igualdad de huella. `nsmeta-current-check.cmd` también cargó y analizó un proyecto con el SDK aislado que existía antes de añadir el catálogo. El consumidor anterior puede no entender el campo nuevo y necesita validación o regeneración coordinada; no se afirma compatibilidad binaria sin pruebas. La compilación coexistió con cambios ajenos sin registrar y con cambios todavía provisionales del consumidor.

La prueba previa del productor acreditó una raíz vacía y otra con campo escalar, y no acreditó publicación en constructor o destructor, llamada opaca, clase derivada ni campo de referencia. Debe repetirse tras corregir el formato. La compilación coexistió con cambios ajenos sin registrar; repetir sobre la revisión integrada.

Falta ampliar la prueba conservadora para bases y dependencias, transportar la garantía al modelo del consumidor y usarla solo después de demostrarla. Deben cubrirse cuerpos ausentes, versiones antiguas, operaciones indirectas, inicializadores, excepciones y seguridad de la cadena de bases; no se puede inferir seguridad por ausencia de un cuerpo ni a partir del contrato de cleanup. Los archivos `include/nativesharp/Interop/LibraryMetadata.h` y `src/Interop/LibraryMetadata*.cpp` pertenecen a la metadata NativeSharp y pueden coincidir con la rama de interoperabilidad, aunque esta etapa no modifica la funcionalidad de interoperabilidad C/C++.
