# Decisiones de trabajo y primeros cambios propuestos

Estado: **propuesta derivada de lectura de código, sin implementación ni pruebas mixtas**. Complementa el [plan principal](PLAN-CXX-NATIVESHARP.md), que contiene la evidencia y las referencias de origen. Los puntos aún no resueltos se enumeran al final: no deben convertirse en promesas de compatibilidad completa.

## Respuesta a la pregunta principal

La dirección propuesta es construir un sistema de compilación mixto alrededor de los dos compiladores existentes. El frontend C++ sigue siendo Clang; se le añade una integración para ver declaraciones y especializaciones NativeSharp. NSC necesita una integración equivalente para consultar tipos y operaciones C++ en el AST y Sema de Clang, expresar esas operaciones en sus modelos y emitir llamadas y ciclo de vida correctos. Ambas direcciones comparten una representación de contratos y un coordinador de instanciaciones, no necesariamente un único AST ni una sola representación de objetos.

El código disponible identifica puntos de integración concretos en Clang, pero todavía **no demuestra qué parches al núcleo de Clang son inevitables**. La primera implementación debe utilizar y contrastar esos puntos; si no pueden expresar el comportamiento necesario, modificar el fork en el punto que se demuestre insuficiente. Evitar tanto prohibir cambios a Clang como modificarlos sin identificar el problema real.

## Propuesta por componente

| Componente | Cambio concreto propuesto | Por qué / dependencia |
|---|---|---|
| Proyecto y conductor NSC | Añadir entrada C/C++ con cabeceras, configuración de compilación y bibliotecas, manteniendo `LibraryReference` NS sin cambiar su significado. Crear una sesión mixta opt-in. | `ProjectLoader.cpp:538,1046,1076` actualmente exige artefactos y metadata NS; una librería C++ necesita además sus declaraciones y opciones para instanciar templates. |
| Integración NSC–Clang | Añadir componente enlazado a Clang y LLVM de la misma revisión, con propiedad explícita de `CompilerInstance`, AST, Sema, diagnósticos y finalización. | Se necesita consultar semántica C++ real y no generar una lista manual de bindings; comparar con Carbon y fijar versión y opciones de ABI. |
| Tipos y resolución NSC | Introducir identidad para entidades C++ y bindings que preserven tipo, qualifiers, categoría, overload seleccionado, representación, ownership y dependencia del módulo. Conectar lookup y análisis de expresiones con Sema. | El modelo NS existente tiene piezas útiles pero no transporta todo el contrato C++; la resolución de sobrecargas no puede basarse únicamente en nombres y tamaños. |
| HIR, MIR y backend NSC | Ampliar los modelos y lowering para llamadas, construcciones, destrucción, temporales y excepciones C++. Seleccionar emisión de Clang o adaptador generado cuando corresponda. Versionar o invalidar cachés afectadas. | La llamada tiene que preservar el ABI y las operaciones reales, incluidos los casos que el modelo NS `Direct`/`Indirect` no describe completamente. |
| Compilador C++ propio | Añadir un modo opt-in al driver/frontend que cargue la superficie NS y exponga un proveedor de declaraciones y tipos al AST/Sema, coordinado con NSC. Mantener el modo C/C++ ordinario. | `ExternalASTSource` y Sema brindan puntos candidatos para búsqueda, definición y layout; se debe comprobar su cobertura real con PCH/módulos y demandas reentrantes. |
| Genéricos y templates | Crear servicio de demandas con identidad canónica, estados, detección de ciclo y publicación síncrona; adaptar fases e índices NS para aceptar tipos C++ y exponer especializaciones NS a Clang. | Clang vuelve a consultar una especialización luego de pedirla; el modelo NS observado construye índices por fases. No basta con convertir nombres o agregar una función pública. |
| Vida y excepciones | Definir protocolos por operación y módulo: construcción, destrucción, copia/move, asignador, préstamo/ownership, propagación, captura, cleanup y rethrow; implementar la emisión correspondiente en ambos frontends. | Los filtros y representaciones de excepciones encontrados difieren; los descriptores NS y los destructores/ABI C++ no son intercambiables por defecto. |
| Enlace y arranque | Registrar dependencia de artefactos C++, su cierre y selección COMDAT/ODR; coordinar entry point, CRT, estáticos, TLS y DLL. Preservar la ruta NS existente para programas no mixtos. | El ejecutable NS observado usa entrada propia; enlazar un objeto no demuestra ejecución de los inicializadores C++. El runtime puede tener estado por módulo. |
| Verificación y herramientas | Construir y probar futuras versiones exclusivamente en A:, comparar contra la referencia congelada y comprobar el binario de NSC realmente invocado. | Los ejecutores existentes pueden usar por defecto un compilador en C:; una prueba debe seleccionar y aislar sus artefactos de manera explícita. |

Las rutas y funciones concretas de estas propuestas, y la evidencia que les da soporte, están en las trazas [C++ desde NS](TRAZA-IMPORTACION-CXX-EN-NS.md), [NS desde C++](TRAZA-NATIVESHARP-EN-CXX.md), [enlace](ENLACE-Y-ESPECIALIZACIONES.md) y [vida y excepciones](DISENO-EXCEPCIONES-Y-VIDA.md).

## Qué se hace primero al implementar

1. Fijar una configuración reproducible de las bibliotecas Clang/LLVM y del NSC de A:, y asegurar que el modo ordinario NS conserva sus resultados. Preparar el componente mixto y la entrada de proyecto sin desplegar en C:.
2. Integrar una sesión Clang real con su semántica y diagnósticos, y hacer que NSC consulte declaraciones C++ mediante identidades persistentes. La primera meta es probar el recorrido completo de información; no declarar que unas pocas declaraciones cierran el lenguaje.
3. Conectar esa identidad a resolución de llamadas, ABI y generación de código; en paralelo fijar un mecanismo para proyectar entidades NS en el AST de Clang. Introducir los protocolos de vida y excepciones que correspondan a cada recorrido, desde el inicio del diseño.
4. Abrir especialización recíproca, compilación separada y cierre de enlace; ampliar por contratos de la matriz y validar estática y DLL en ambos sentidos. La secuencia puede solaparse cuando una característica dependa de otra.

## Decisiones que no se deben fingir cerradas

- El protocolo completo de excepciones cruzadas, incluyendo capturas tipadas y `rethrow`, requiere elegir cómo se representan y mantienen las identidades en cada frontera.
- El modelo concreto de una clase NS dentro de C++ debe resolver los significados de valor, referencia, copia, `new`, destrucción y herencia cruzada sin inventar equivalencias.
- La reentrada del coordinador, serialización de PCH/módulos, evaluación constante de cuerpos NS en contextos C++ y coherencia incremental requieren diseño y validación específicos.
- La inicialización y terminación mixta, la vida entre DLL y la política de CRT/estado por módulo requieren verificaciones con artefactos reales.
- El alcance por reglas semánticas aún debe completarse en la [matriz](MATRIZ-CONTRATOS.md). Un diagnóstico claro para un caso no implementado registra una brecha; no lo convierte en interoperabilidad cumplida.
