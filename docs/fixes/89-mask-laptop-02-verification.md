# Verificación del caso 89 en laptop 02

## SCOPE LOCK

- Rama: `development-fixes-limitaciones-laptop-02`.
- Base funcional recibida: `71cd86d35220f654140e5619e9f9db0020f0b158`;
  el commit local `d4f721cf5` sólo documentaba la infraestructura del worker.
- Alcance: confirmar en esta revisión conversión de máscara a vector,
  indexación de máscara y selección condicional por máscara. No ampliar el
  lenguaje ni cambiar las decisiones de representación ya integradas.
- Archivos de producto autorizados sólo ante causa demostrada:
  `src/LLVM/Lowering/OopLlvmFieldsAndConversions.cpp`. Los analizadores,
  headers compartidos, `OopLlvmConditionalsAndNullAware.cpp` y
  `OopLlvmOperators.cpp` permanecen protegidos.
- Archivos propios de este lote: esta nota y
  `tests/integration/simd_mask_complete_matrix.py`.
- Construcción y ejecución exclusivamente en las rutas del worker 02 en A:;
  primera construcción inicial y, después, sólo builds incrementales y
  pruebas dirigidas. Sin batería completa, push, merge, rebase ni despliegue
  en la instalación funcional.

## Contratos conservados

- Conversión: `mask<T,N>` a `simd<T,N>` conserva por lane cero o todos los
  bits a uno. También se admiten `float -> uint` y `double -> ulong` con el
  mismo número de lanes. No se admiten otras parejas cruzadas, cantidades
  diferentes ni la conversión inversa. Un destino flotante conserva el patrón
  de bits; verdadero no significa el valor numérico `1.0`.
- Índice: `mask<T,N>[i]` produce `bool`, evalúa receptor e índice una vez y
  comprueba límites antes de extraer el lane. No es asignable ni direccionable.
- Selección: una condición `mask<T,N>` selecciona por lane entre dos valores
  `simd<T,N>` o dos máscaras idénticas. Condición, rama verdadera y rama falsa
  se evalúan una vez y en ese orden; el condicional de `bool` conserva su
  evaluación perezosa.
- Formas portables: lanes `sbyte`, `byte`, `short`, `ushort`, `int`, `uint`,
  `long`, `ulong`, `float` y `double`; `N` es potencia de dos, al menos 2, y
  el ancho total no supera 512 bits.

## Entorno y construcción

- Configuración aislada: `A:/nsc-fixes-laptop-02-build`, SDK
  `A:/nsc-fixes-laptop-02-sdk`, temporales y datos de aplicación bajo
  `A:/nsc-fixes-laptop-02-temp`, caché bajo
  `A:/nsc-fixes-laptop-02-cache` y evidencia bajo
  `A:/nsc-fixes-laptop-02-evidencia`.
- CMake resolvió por nombre `clang-cl` 23.1.0 y registró
  `NSC_TEST_COMPILER=A:/nsc-fixes-laptop-02-build/bin/nsc.exe`.
- El directorio estaba vacío. La construcción inicial exclusiva del objetivo
  `nsc` generó sus objetos y terminó con código 0. Identidad ejecutada:
  `NativeSharp Compiler 3.0.0`, build `9fa5137c755705e0`.
- La comprobación incremental final regeneró sólo siete pasos de identidad,
  enlaces y contratos, no los 630 objetos iniciales. Su build
  `a0809ad51de6d854` se instaló y verificó en el SDK 02; los seis originales y
  la matriz completa se repitieron con esta identidad final y conservaron los
  resultados registrados abajo.
- La primera reproducción localizó honestamente una preparación incompleta:
  el SDK 02 todavía no contenía `NativeSharp.Runtime.Win32.lib`. CMake instaló
  y verificó los artefactos ya construidos en el prefijo aislado de A; no hubo
  despliegue en C. Después se repitió desde cero la reproducción.

## Resultados dirigidos

Los tres fixtures originales se conservaron sin cambios y
`simd_mask_report_original.py` compiló y ejecutó conversión, índice y
condicional en O0/O2: seis salidas 0.

Las regresiones integradas anteriores también pasaron con el compilador de
esta copia:

- `simd_mask_conversion.py`: diez tipos de lane con patrón alternado en O0/O2
  (20 ejecuciones con salida 42) y seis rechazos esperados para tipo o cantidad
  incompatibles.
- `simd_mask_float_unsigned.py`: `float -> uint` y `double -> ulong` en O0/O2,
  cuatro ejecuciones con salida 42.
- `simd_mask_index.py`: lanes verdaderos/falsos, evaluación única, cinco
  formas adicionales, rechazos de escritura/dirección/tipo y límites. Los
  índices dinámicos `-1`, `4` y `2^64+1` produjeron la trampa 132 en O0/O2;
  el IR comprobó el rango antes de estrechar el índice.
- `simd_mask_select.py`: selección de vectores y máscaras, lanes enteros y
  flotantes, evaluación única en orden, varios anchos, condicional escalar y
  rechazos de ramas incompatibles en O0/O2.

La regresión nueva `simd_mask_complete_matrix.py` combina conversión,
indexación y selección para cada forma admitida y observa todos los lanes:

- 43 combinaciones de tipo y cantidad, ejecutadas en O0/O2: 86/86 pasaron.
  Incluyen todas las potencias de dos permitidas hasta 512 bits para los diez
  tipos de lane.
- Cinco formas inválidas (`N=1`, `N=3`, más de 512 bits, lane `bool` y lane
  `decimal`) se rechazaron en O0/O2: 10/10.
- Los 13 tipos de índice integrales admitidos se ejecutaron en O0/O2. `bool`,
  `float`, `double` y `decimal` se rechazaron con el diagnóstico específico.
- Cuatro conversiones no autorizadas (`int -> uint`, `double -> long`,
  `float -> ulong` e inversa vector a máscara) se rechazaron en O0/O2: 8/8.

Comando de caracterización reproducible, con el entorno del worker aplicado:

```text
python.exe -B A:/nsc-fixes-laptop-02/tests/integration/simd_mask_complete_matrix.py A:/nsc-fixes-laptop-02-build/bin/nsc.exe --expect-binary-gap
```

Resultado resumido:

```text
VALID_SHAPES 86 PASS
INVALID_SHAPES 10 PASS
INDEX_TYPES 13 valid, 4 invalid: PASS
INVALID_CONVERSIONS 8 PASS
BINARY_GAP 2 producer/consumer pairs CONFIRMED
simd_mask_complete_matrix: PASS_WITH_EXPECTED_BINARY_GAP
```

## Defecto vigente: superficie binaria SIMD

La prueba construye una biblioteca estática pública en O0 y O2, elimina su
fuente y compila consumidores cruzados O2 y O0 mediante `LibraryReference`.
El productor expone cinco métodos que reciben o devuelven máscaras y vectores.
La biblioteca y `.nsmeta` se generan; `--verify-library-metadata` pasa y
`--dump-library-abi` contiene los cinco nombres. Sin embargo, el consumidor
reconoce `type:MaskProducer::MaskApi` pero informa que no tiene miembros
compatibles `CompareInt`, `ReadInt`, `ChooseInt`, `CompareFloat` ni
`BitsFloat`. Los dos cruces reproducen el mismo resultado.

La frontera causal está fuera del permiso de edición de este lote:
`src/Driver/PrecompiledSurface/MetadataSourceMapping.cpp`, función
`sourceCoreTypeForMetadataKey`, materializa builtins, tipos nominales,
iteradores y futuros, pero no las identidades canónicas `mask<...>` ni
`simd<...>`. `PublicSurfaceBuilder` usa ese mapeo para reconstruir retornos y
parámetros, por lo que los métodos presentes en NSMETA no llegan a ser
invocables desde la superficie fuente del consumidor.

Propuesta al integrador:

1. Reconocer ambas identidades mediante `language::parsePortableSimdType` en
   el mapeo de superficie y convertir recursivamente la clave del lane.
2. Emitir exactamente `mask<lane, N>` o `simd<lane, N>`, validando el contrato
   portable existente; no crear parejas, tipos ni cantidades nuevas.
3. Convertir la caracterización binaria en regresión positiva y exigir O0 a
   O2 y O2 a O0, sin fuentes del productor.
4. No cambiar la versión NSMETA: las claves y los miembros ya están presentes,
   verifican y aparecen en el informe ABI. Sólo falta su materialización.

No se modificó producto porque este arreglo tocaría el frente de metadata y
driver, fuera de los archivos autorizados. El posible archivo compartido para
integración es `src/Driver/PrecompiledSurface/MetadataSourceMapping.cpp`; puede
requerir incluir `nativesharp/Language/AbiSemantics.h`. No se demostró
necesidad de cambiar analizadores, headers públicos, lowering SIMD ni el
formato NSMETA.

## Conclusión

Los tres defectos históricos del caso 89 no están vigentes dentro de una
unidad de compilación en esta revisión: ejemplos originales, contratos por
lane, formas válidas/inválidas y O0/O2 pasan. Sí queda un defecto independiente
de consumo binario para firmas públicas `mask`/`simd`. Queda reproducido,
delimitado y propuesto al integrador, sin invadir los archivos protegidos.
# Integración y corrección de superficie binaria

En la rama integradora se reprodujo primero el fallo binario con el compilador
aislado: la matriz pasó sus controles de tipos y confirmó los dos cruces que
fallaban. Se añadió materialización de claves `mask` y `simd` de metadata,
usando la validación portátil existente y comprobando identidad y anchura.
Tras la compilación incremental, la misma matriz pasó 86 formas válidas,
10 inválidas, 13 índices válidos, cuatro inválidos, ocho conversiones inválidas
y ambos cruces binarios O0/O2 y O2/O0. El compilador posterior tuvo SHA-256
`90a64254d0627b599b2f52edb495ee840ad4e3c9e58252f783b617ff01aab479`;
los logs son `A:/nsc-fixes-evidencia/laptop02-simd-integration-20261112.log`
y `A:/nsc-fixes-evidencia/laptop02-simd-after-20261112.log`. La regresión
binaria de tuplas prestadas también conservó su resultado previo de 48 análisis
en `A:/nsc-fixes-evidencia/tuple-binary-after-simd-20261112.log`.

Un control adicional pasó para SIMD directo por parámetro binario. Las tuplas
que contienen SIMD o máscara fallaron por layout incluso en fuente única,
antes de producir la biblioteca; quedan como incidencia independiente. Una
metadata alterada fue rechazada por checksum y, tras recalcularlo, por huella
ABI: esto no ejercitó directamente la validación semántica del importador.
Fuentes, comandos y resultados están en
`A:/nsc-fixes-evidencia/simd-metadata-tuple-directed*` y
`A:/nsc-fixes-evidencia/simd-metadata-invalid-directed*`.
