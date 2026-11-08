# Diseño autorizado: punteros compartidos y AddressSanitizer

Este documento fija la dirección de trabajo; ninguna de estas dos capacidades
se considera implementada ni validada por aparecer aquí.

## Rc, WeakRc, Arc y WeakArc

El contrato `NSSMARTPTR` publicado por el compilador exige clonación explícita,
ausencia de copia implícita, upgrade sin resurrección, rechazo de overflow del
contador y mantenimiento de las funciones de destrucción y liberación del
módulo productor mientras exista el bloque de control. `Rc` y `WeakRc` son
locales; `Arc` y `WeakArc` pueden cruzar hilos solo cuando el payload satisface
Send/Sync. Las familias y fábricas pertenecen a `NativeSharp.Core`, que es el
namespace usado por la reproducción, y deben respetar el contrato publicado.

Cada bloque conserva contadores fuertes y débiles, origen del allocator,
destructor, liberador y procedencia del módulo. El último fuerte destruye el
payload una vez y libera la referencia débil implícita; el bloque solo se
libera al desaparecer el último débil. Upgrade incrementa el fuerte solo si
es distinto de cero, mediante compare-exchange en Arc. Las operaciones que
no puedan incrementar deben fallar antes de desbordar. Clone y Downgrade son
explícitos; un valor expirado nunca vuelve a poseer el payload.

La implementación debe acreditar además la fábrica de una sola asignación
exigida por el manifiesto, el movimiento sin doble liberación, el pin del módulo
y la imposibilidad de copiar o compartir Rc de forma indebida. El SDK contiene
`ReferenceCounts` atómico y hooks en `NativeSharp.Core.Threading`, pero no
provee los cuatro tipos ni demuestra las demás garantías. No basta añadir
declaraciones vacías ni tratar esos contadores como una implementación completa.
Es necesario investigar y probar el soporte efectivo del compilador para
propiedad genérica, destrucción, restricciones Send/Sync y módulo productor.

## AddressSanitizer

El usuario aprobó exposición explícita y desactivada por defecto. La opción
debe seleccionar instrumentación de las unidades LLVM y enlazar la DLL y las
bibliotecas de runtime compatibles suministradas externamente; su ubicación
debe ser configurable y validarse sin alterar `C:/native-sharp` ni Clang.
El entorno contiene Clang 23 y los artefactos Windows `clang_rt.asan_dynamic`
en `C:/clang-llvm/lib/clang/23/lib/windows`, únicamente como referencia.

`LlvmNativeSanitizer` ya inserta el atributo y aplica el pipeline `asan`; la
prueba existente verifica IR. Falta conectar el indicador desde el driver a
cada unidad generada, garantizar que las claves o la desactivación de caché
impidan reutilizar objetos sin instrumentar, resolver la interacción con
ThinLTO y las bibliotecas precompiladas, y enlazar y localizar el runtime al
ejecutar. Las combinaciones aún no soportadas deben rechazarse de forma clara,
no producir un binario aparentemente instrumentado.

La validación mínima debe incluir una ejecución válida, una escritura fuera
de límites que ASan diagnostique realmente, O0/O2, ausencia de instrumentación
sin la opción, falta de runtime con diagnóstico, y aislamiento de cachés y
resultados. No se ha demostrado todavía el enlace ni esa ejecución negativa.
