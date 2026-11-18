# Investigación aislada: SIMD en agregados, Future e iteradores

Estado: integrado como trabajo en curso, sin compilación ni pruebas de esta versión.
La integración se adelantó a la validación por instrucción del usuario; los
riesgos y las verificaciones pendientes descritos abajo siguen abiertos.

Base del clon: cbe3158305ce72b28ef6b7cd60a5cc6f76f04444. La ejecución de referencia
usó `A:\nsc-fixes-evidencia\build\bin\nsc.exe`; no se ha determinado que ese binario
corresponda exactamente al commit del clon. Los resultados y comandos de las pruebas
están en `A:\nsc-fixes-evidencia\simd-aggregate-future-baseline2-20261112`.

La comprobación MIR de tuplas, generadores síncronos, generadores asíncronos y Future
con `simd<int,4>`, `simd<int,8>` y `simd<int,16>` falla por ausencia de layout
físico; no se ejecutaron esos programas. El informe original y sus ejemplos siguen
intactos. El primer intento Future falló antes por await síncrono; la segunda versión
de la prueba corrige únicamente esa nueva reproducción.

La función `valueLayoutForTypeKey` carece del caso SIMD ya modelado en el layout de
campos normales. Sin embargo, agregarlo directamente no es suficiente: las direcciones
de las funciones de avance y destrucción de iteradores están codificadas como 24 y 32,
aunque el layout real depende del tamaño del valor. Para vectores anchos, la alineación
física se limita a 16 mientras LLVM puede requerir más; una carga o escritura sin
alineación explícita puede atribuir al puntero garantías que no tiene. Se identificaron
accesos en resultados Future, inicialización de agregados y desestructuración, además
de los accesos de iterador. El prototipo busca esos offsets en el layout MIR y reduce
algunas alineaciones a 1 conservadoramente, pero se requiere revisar todos los caminos
de acceso, las referencias, la máscara y la representación del frame antes de certificar.

Pendiente: compilación incremental aislada del prototipo, resolver cualquier fallo de
compilación, correr las reproducciones originales y pruebas O0/O2 con valores distintos
en carriles extremos, varios avances y destrucción, tuplas anidadas/máscaras, Future
suspendible y no suspendible, y controles escalares; examinar IR para garantizar la
alineación y el acceso al offset; verificar revisión y dependencias del binario.

## Auditoría estática posterior

Las operaciones intrínsecas de iterador reciben la clave del binding: HIR la copia a
`elementTypeKey` en `HirCoreCallFrameLowering.cpp`, y el lowering MIR conserva ese
campo en `MirNodeMachine.cpp` para MoveNext, MoveNextAsync y Drop. El registro de
bindings `cancel-prototype/owned-cancel/body-current.log` muestra `element=async-iterator:...`.
El prototipo consulta ahora esa clave en vez de inferirla de la expresión receptora.
En los cuerpos de destrucción sintetizados, la clave procede del primer parámetro MIR;
el prototipo elimina el sufijo de puntero si existe. La validez dinámica de ambos casos
sigue pendiente de pruebas con locales, campos, referencias y distintos genéricos.

El campo Current empieza en 16 para los tipos físicos admitidos con alineación máxima
16; los punteros MoveNext/Drop deben leerse por ordinal de layout por estar después del
elemento. Future usa consulta de layout para sus funciones poll y drop; el acceso fijo
24 del poll en el destructor del generador se refiere exclusivamente a `Future<bool>`.

La captura de parámetros y los locales de corrutina también usan el layout de valores
(`MirFunctionLowerer.cpp`, `frameValueLayout`). Por ello habilitar SIMD incluye slots
del frame. Se añadieron alineaciones conservadoras a cargas/escrituras vectoriales de
capturas, inicialización y restauración de locales, y a los accesos de resultado Future,
Current, campos, punteros y la inicialización de tuplas. Los spill de replay ya declaran
alineación 8. Esta enumeración es una auditoría del código, no prueba de que todo camino
generado sea correcto; faltan revisar IR y ejecutables, especialmente referencias,
conversiones, arreglos y fronteras de módulos. No se hicieron builds ni pruebas tras la
instrucción de aplazarlas. Los casos nuevos de parámetros capturados no se han ejecutado.

## Referencias, arreglos y módulos

La revisión estática encontró que las lecturas/escrituras de elementos vectoriales de
arreglos, incluidas inicialización, asignación compuesta y accesos condicionales, podían
heredar alineación LLVM superior a la física. El prototipo usa alineación conservadora
en esos sitios y en cargas por puntero, campos y parámetros capturados. Quedan por
validar arreglos multidimensionales, vistas, referencias y alias con varios anchos.

El materializador de metadatos para tuplas admitía numéricos y especializaciones, pero
rechazaba explícitamente elementos SIMD o máscara aunque ya existe un conversor para
su identidad directa. Se añadió esa ruta y casos unitarios, sin ejecutar. Sigue
pendiente una prueba binaria completa productor/consumidor y referencias reales. El
caso de Future o Iterator con tuplas que contienen `|` no se reprodujo en esta campaña:
los lectores de esas identidades separan en el primer `|`, por lo que necesita una
investigación y corrección independiente antes de ampliar su aceptación. La ABI no
queda certificada por el test unitario.
