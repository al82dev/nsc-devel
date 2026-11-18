# Plan de estudio y trabajo en dos equipos

Estado al preparar el reparto: la tarea principal está pausada. La base de
trabajo registrada de esta copia es `development-fixes-limitaciones`, commit
`90572120ab03fa16b8bb7986bda53693d5baa8fc`. En ese momento coincide con la
referencia remota local `origin/development-fixes-limitaciones`; esto no
comprueba cambios que hayan podido llegar al servidor después de la última
actualización de esa referencia. Hay cambios importantes sin commit, incluidos
los seis archivos públicos de punteros. Una clonación del commit por sí sola no
los contiene.

El último experimento de publicación de miembros privados para plantillas quedó
interrumpido: la última edición de `src/Driver/LibraryReferenceValidation.cpp`
no se ha compilado ni probado. La prueba anterior llegó a un error de símbolo
interno no publicado. Conservar este estado como trabajo en curso; no afirmar
que la reparación o el SDK binario funcionan.

## Reglas comunes

- Cada equipo debe leer sus lineamientos aplicables, confirmar su base y
  reproducir cada caso antes de corregirlo. Los ejemplos originales del reporte
  se conservan; para investigar se usan copias independientes.
- Compilaciones incrementales y pruebas dirigidas siempre; no ejecutar batería
  completa. Cada equipo usa su propia carpeta de build, cachés y temporales.
- No tocar interoperabilidad C/C++, Daikon, LLVM/Clang, la instalación funcional
  ni los builds de interoperabilidad. No integrar ni publicar automáticamente.
- Documentar decisiones autónomas, evidencia, comandos, resultados y límites;
  conservar las decisiones específicas ya aprobadas. `NativeSharp.Text` se
  mantiene pendiente hasta definir su superficie.
- Cada lote termina en commits coherentes, con pruebas de regresión. No se
  considera terminado porque compile ni porque un prototipo pase un ejemplo.

## Reparto recomendado

| Equipo | Lote | Estudio y resultado esperado | Dependencias y conflictos |
| --- | --- | --- | --- |
| Principal | Punteros compartidos y propiedad | Terminar `Rc`, `WeakRc`, `Arc`, `WeakArc`, vida de préstamos, limpieza de excepciones, seguridad de hilos y pruebas de clases y structs. Validar uso real del SDK binario. | Propietario de `lib/NativeSharp.Core/{Rc,WeakRc,Arc,WeakArc,SharedPointers,SharedView}.ns`, de sus pruebas y de la ruta de metadata/reconstrucción que necesitan. Sus cambios actuales no están terminados. |
| Laptop | Restricciones genéricas entre parámetros (`69-constraint`) | Reproducir `TOp : Reducer<T>`, estudiar sustitución y satisfacción posterior; elegir y documentar el contrato recomendado, implementar y probar positivo y negativo con tipos distintos en O0/O2. | Punto de partida identificado: `src/Frontend/NameLookup/SyntaxBinding.cpp`. Antes de modificar archivos de genéricos o metadata que estén en el respaldo, cotejar su estado y avisar. No usar como prueba solo el caso cerrado `Reducer<int>`, que ya funciona. |
| Laptop, después | Resolución de rutas `Compile` con `..` (`102-outside`) | Confirmar el resultado original y estudiar rutas relativas, raíz del proyecto, globs y diagnósticos; adoptar y documentar el contrato recomendado y probar límites válidos y errores. | En principio independiente de punteros. Verificar primero los archivos concretos afectados y no cambiar herramientas ni rutas de la otra máquina. |
| Laptop, después | SIMD máscaras (`89-mask-convert`, `89-mask-index`, `89-mask-select`) | Reproducir por separado conversión, índice y selección; estudiar representación, elegir contrato de cada operación y probar verdadero/falso y varios lanes en O0/O2. | Revisar al inicio archivos compartidos, en especial `src/LLVM/Lowering/OopLlvmArraysAndStores.cpp` y `OopLlvmExpressionMachine.cpp`: ya tienen cambios sin commit en principal. Si son necesarios, coordinar o dejar el lote para después de integrar. |
| Compartido, tras los lotes | Integración y revisión | Revisar diferencias, aplicar commits por lote a una rama de integración acordada y repetir solo pruebas dirigidas relevantes de ambos lados, incluidos casos que crucen áreas. Actualizar inventario, resultados y archivos compartidos. | La integración no está autorizada automáticamente por este plan. No usar `development-cpp-clang` ni modificar su build. |

Otros pendientes del reporte —métodos genéricos en tipos genéricos, temporales
`in`, `++/--`, `#run`, inspección de campos, `derive(Print)` y el objetivo de
desvirtualización— se asignarán cuando un equipo termine su lote y se revisen
sus dependencias. No se deben repartir todos a la vez: varios pueden cruzar
archivos del compilador que el otro equipo esté modificando. AddressSanitizer
ya tiene una opción y pruebas dirigidas parciales; sus límites deben revisarse
antes de asignar ampliaciones. El inventario detallado está en
`campaign-status.md` y `decisiones-pendientes.md`; algunas preguntas históricas
de esos archivos fueron sustituidas por la autorización general de tomar las
decisiones recomendadas, salvo las decisiones específicas ya establecidas.

## Intercambio y puntos de control

1. Antes de crear la rama de laptop, verificar el commit realmente disponible en
   el servidor. El árbol local sin commit no viaja con `clone` ni `checkout`.
   Si se prepara un respaldo de trabajo en curso, identificarlo expresamente
   como tal; no tratarlo como código validado ni integrar todo de forma ciega.
2. Cada equipo trabaja en su propia rama y anota por lote: base, archivos,
   decisiones, reproducción, pruebas y commit. Las evidencias locales en A no
   viajan solas con Git: incluir en la documentación comandos y resultados
   suficientes para repetirlas, y transferir logs solo si se acuerda.
3. Comunicar antes de modificar un archivo reclamado por el otro equipo. Evitar
   editar simultáneamente los documentos globales; cada lote puede llevar una
   nota propia para unirla al inventario durante la integración.
4. Integrar un lote terminado a la vez, revisar conflictos semánticos y correr
   pruebas dirigidas cruzadas. Mantener separada la certificación final de los
   resultados de cada máquina.

## Tiempo

La estimación previa de 80–160 horas efectivas es orientativa, no una medición
de tareas ya dimensionadas. Dos equipos pueden reducir el tiempo transcurrido,
pero no necesariamente a la mitad: los punteros y el SDK binario son una ruta
crítica, y la coordinación, integración y pruebas cruzadas consumen tiempo.
Reestimar tras completar el primer lote independiente y medir horas efectivas.
