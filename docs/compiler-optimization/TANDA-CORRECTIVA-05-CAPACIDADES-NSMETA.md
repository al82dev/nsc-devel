# Tanda correctiva 05: capacidades NSMETA y procedencia de decodificacion

## Problema

El cambio que hizo informativas las etiquetas de version retiro tambien
fronteras que no eran etiquetas:

- un modelo en memoria podia afirmar un formato futuro sin demostrar que sus
  campos fueran entendidos;
- una politica de instalacion vacia era aceptada solo porque su encabezado era
  sintacticamente valido;
- se perdio la incompatibilidad conocida de productores legacy cuyos formatos
  no expresaban todas las capacidades actuales;
- la salida del comando publico dejo de conservar su decision positiva
  ejecutable.

Eso mezclaba cuatro conceptos distintos: etiqueta, contenido decodificado,
capacidades instaladas y contrato historico conocido.

## Correccion

- `LibraryMetadata` conserva una procedencia transitoria, no serializada, que
  solo establece el lector despues de decodificar y autenticar todo el payload.
- Una etiqueta NSMETA futura o diferente se acepta cuando esa procedencia
  demuestra que el contenido real fue entendido por el lector actual.
- Un modelo construido en memoria no puede escribir ni validar por simple
  afirmacion un formato futuro.
- Los productores `0.9.0` y `1.0.1`, cuyas carencias de contrato son conocidas
  y no estaban representadas en sus schemas, conservan el rechazo legacy. Las
  etiquetas desconocidas no se convierten en una frontera artificial.
- La verificacion generica de una politica sigue siendo extensible; la
  verificacion de una instalacion exige que esten presentes, con sus valores,
  todas las capacidades que necesita el producto actual.
- El comando de compatibilidad conserva simultaneamente la palabra
  `satisfies` y la aclaracion de que las etiquetas son informativas.

## Archivos

- `include/nativesharp/Interop/LibraryMetadata.h`
- `src/Interop/LibraryMetadata.cpp`
- `include/nativesharp/Driver/CompatibilityPolicy.h`
- `src/Driver/CompatibilityPolicy.cpp`
- `src/Driver/InstallationVerifier.cpp`

No se modificaron pruebas, fixtures, proyectos, bibliotecas ni instalaciones.

## Validacion dirigida

Se compilaron solamente los seis ejecutables afectados y se ejecutaron sus
seis casos CTest:

- `nsc.stage48_2.library-metadata`;
- `nsc.library-metadata-version-tolerance`;
- `nsc.stage50_9.metadata-host-compatibility`;
- `nsc.stage63.installation-verifier`;
- `nsc.stage66.e9-e10-release-contract`;
- `nsc.item407.official-promotion`.

Resultado: **6/6 passed**, 6.49 segundos. No se ejecuto baseline, minibaseline ni
matriz general.

