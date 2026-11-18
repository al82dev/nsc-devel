# Operadores `in`: reproducciones pendientes y propuesta

## Alcance y evidencia

El fixture original conservado es `A:\nsc-limitaciones-vistas\tests\integration\operator_in_provenance_vistas.py`. El resultado dirigido íntegro es `A:\nsc-vistas-evidencia\operator-in-provenance-vistas-v3.log`; el comando, hashes y resumen están en `operator-in-provenance-vistas-v3-resultado.md`. El fixture salió 1. Las fuentes de sus casos se generan temporalmente desde sus constantes; no se alteraron para obtener el resultado. Los casos negativos solo se analizaron.

Comando desde la copia de vistas: `call A:\nsc-vistas-evidencia\vistas-env.cmd python -B tests\integration\operator_in_provenance_vistas.py A:\nsc-vistas-evidencia\build\bin\nsc.exe`. El SHA-256 del compilador antes y después fue `86e5ceae12e054389adcbe9f3140e7b811e5e4c7f3a98d99a3ebe3ba1c38fdbd`; el del fixture fue `2d1e47fa25713253522599600a84cccdb45fa00f5288ce41bab8a3773b6c3b92`. Las rutas de evidencia en `A:\nsc-vistas-evidencia` son locales a esta campaña y no acompañan necesariamente a la rama.

## Resultados fuente

La matriz usa `Owner`, que posee un `Payload`, y `operator +(in Owner a, in Owner b)` que retorna `a.item`. El caso `local-owners` crea dos dueños locales y lee el resultado antes de salir de su ámbito. En O0 y O2 falla la compilación en el cuerpo del operador: `borrowed value cannot escape through an unqualified return type; declare the return type borrowed`.

El caso `borrowed-local-owners` cambia el retorno a `borrowed Payload` y declara la variable consumidora `borrowed Payload`. En O0 y O2 falla con `readonly borrowed object cannot be returned through a mutable borrowed contract`; además, se rechaza el origen de la variable prestada y se informa que carece de enlace válido de vida. Estos resultados exigen decidir la firma y reglas de solo lectura aplicables antes de clasificar cada rechazo como defecto; no basta declarar que todo retorno desde `in` es mutable o que la vida está demostrada.

Los negativos `escape-two-temporaries`, `escape-one-temporary` y `borrowed-escape-temporaries` comprueban el cuerpo, sin ejecutarlo. Todos encuentran `no viable Stage 38 overload of 'op_Addition'`, además de diagnósticos del productor. Por eso el fixture los registra explícitamente como **bloqueados por resolución de sobrecarga**. No prueban que el análisis de tiempo de vida rechace una llamada resuelta ni que el escape quede impedido si la resolución cambia.

## Resultados binarios

El fixture construye un productor con `FieldOwner`, un campo `owned Part` privado y `operator ==(in FieldOwner a, in FieldOwner b)`. Los productores O0 y O2 terminan correctamente. En las cuatro combinaciones de optimización productor/consumidor, el caso de dos operandos locales compila y ejecuta con código 42, verificando orden de construcción, llamada y destrucción codificado en el fixture.

En las mismas cuatro combinaciones, el caso de dos operandos temporales falla al compilar con `no viable Stage 38 overload of 'op_Equality'`. No hay evidencia de comportamiento de destrucción de esos temporales porque el programa no se generó ni ejecutó. La comparación es relevante, pero la causa precisa del rechazo requiere rastrear resolución de sobrecargas y parámetros `in` con metadata binaria; no se ha atribuido a un archivo concreto.

Los controles anteriores `Direct`, `Alias` y `ViaCall` se rechazan durante análisis por sobrecarga no viable. Esto acredita una barrera conservadora en esta configuración, no el análisis de vida de una llamada aceptada. El fixture verifica hashes del productor anterior antes de usarlos; no ejecuta esos consumidores.

## Propuesta acotada para una corrección posterior

1. Mantener intactos el fixture y el log como reproducción; no convertir las barreras de sobrecarga en éxito de lifetime ni eliminar los casos que fallan.
2. Investigar en forma independiente por qué la resolución binaria acepta dos locales `in` y rechaza los dos temporales. Antes de cambiarla, aislar resolución y materialización de firmas con controles equivalentes fuente/binario y combinaciones local/temporal, O0/O2. Establecer por inspección qué archivos requieren cambio y reservarlos con el integrador; no se ha demostrado que sea necesario tocar interoperabilidad.
3. Si se permite la llamada con temporales, ejecutar solo ejemplos seguros que comprueben orden de evaluación y destrucción. Mantener los escapes como análisis sin ejecución y verificar que el diagnóstico provenga del tiempo de vida, no de una barrera anterior.
4. Para el retorno de clase derivado de `in`, establecer ejemplos cuya firma exprese correctamente el préstamo y solo lectura vigentes. Probar por separado origen en cada parámetro, raíces múltiples, receptor local y temporal, alias y escape; incluir un control válido. Cualquier posible ampliación del contrato requiere evidencia y respetar la decisión previa de preservar el préstamo y la restricción de solo lectura en aliases.
5. Integrar el fixture como prueba aprobada solo cuando los positivos y negativos tengan resultados atribuibles al contrato esperado. Mientras tanto puede conservarse como reproducción pendiente fuera de una puerta que se anuncie como aprobada.

El fixture está actualmente sin registrar en Git. Puede incluirse explícitamente como reproducción pendiente, manteniendo su salida no cero y sin agregarlo a una batería que deba pasar; referencia el productor heredado ya registrado en `tests/integration/operator_in_legacy_escape_fixture` y verifica sus hashes. Resuelve la raíz del repositorio desde la ubicación de su propio script; aún requiere un compilador y directorios temporales en A y un entorno de herramientas adecuado. En la revisión textual de `CMakeLists.txt` y `tests` no se encontró una referencia que lo ejecute automáticamente; esta comprobación no demuestra que ningún sistema externo lo descubra.

Tras cambiar exclusivamente la ruta y el encabezado del fixture se repitió el comando anterior, con salida 1 y el mismo resumen de fallos; el registro está en `A:\nsc-vistas-evidencia\operator-in-provenance-vistas-v4.log`. El SHA-256 del fixture adaptado es `d7a6a8043bf8ed1894ed350ac8cff9fe26bc5ae38a3a8853c420eb04d3a747bc`. El SHA-256 del compilador leído después de v4 es `875372ae0fa8e0e6f98f81eccdf6f6a7c197ccefa12bfae213ee13617381ace7`; no se registró su hash inmediatamente antes de esa ejecución. El coordinador confirmó que no se reenlazaría durante la prueba. Los negativos bloqueados por sobrecarga siguen sin demostrar lifetime.

Una nueva ejecución dirigida con el mismo comando produjo salida 1 y el mismo resumen (`A:\nsc-vistas-evidencia\operator-in-provenance-vistas-v5.log`). En esta ejecución el SHA-256 del compilador se comprobó antes y después y fue `875372ae0fa8e0e6f98f81eccdf6f6a7c197ccefa12bfae213ee13617381ace7` en ambos momentos. No se ejecutaron los negativos.

## Límites

Este informe documenta una ejecución del compilador aislado identificado en el resumen v3. No constituye una validación de todas las formas de operador, de otras plataformas o de todos los contratos de lifetime. El fallo histórico de extracción textual no puede atribuirse con certeza sin la copia exacta del fixture que produjo v2; el fixture actual sí extrae los diagnósticos al volver a ejecutar.

## Contrato readonly y comprobaciones de superficie

Los positivos fuente originales siguen conservados en el fixture. Su expectativa necesita distinguirse del contrato actual: el operador que declara retorno `Payload` sin cualificador y retorna `a.item` desde `in Owner a` encuentra el rechazo de escape de un préstamo mediante retorno sin cualificar. La variante que declara `borrowed Payload` intenta devolver el objeto readonly mediante un contrato prestado que permite mutación. No se debe retirar ninguna de esas barreras solo para hacer pasar esos ejemplos.

Se preparó un sondeo **semántico únicamente**, sin ejecutar programas, en `A:\nsc-vistas-evidencia\operator_in_readonly_surface_probe.py`. Las fuentes y salidas completas están en `operator-in-readonly-surface.log` y `operator-in-readonly-declared-lifetime.log` de esa carpeta. Los dos sondeos emplearon `A:\nsc-vistas-evidencia\build\bin\nsc.exe`, con SHA-256 `eb4f6bd73e846b86207549a8cf4dcd2eec7467c3af8f9752b063099ee4190294` antes y después:

- Un método ordinario de instancia `public ref readonly int Get() { return ref data; }` pasó el análisis; esto no es una prueba de ejecución ni cubre todos los retornos de clase.
- Un operador `int operator +(in Owner a, in Owner b)` que devuelve la suma de campos escalares pasó el análisis. Los operadores `in` no están rechazados en general.
- Un operador `ref readonly Payload operator +(...)` fue rechazado en el parser con `ref return is valid only on an ordinary method`. Esta forma concreta no está disponible como operador.
- Un método estático `ref readonly Payload Pick(in Owner a)` que retorna `ref a.item` fue rechazado porque no se permite exponer ese parámetro readonly. La variante con vida declarada explícitamente, `ref readonly[a] Payload Pick<lifetime a>(in[a] Owner a)`, recibió el mismo rechazo. Estas observaciones no demuestran la ausencia de toda superficie posible de lifetime o readonly.
- Una variante inicial omitía la declaración `lifetime a` y falló por esa razón de preparación; se conserva el log y no se utiliza como evidencia del caso con lifetime declarado.

La recomendación es mantener la expectativa de los dos ejemplos fuente originales como incompatible o pendiente de diseño, mientras se estudia una superficie de retorno readonly para operadores con vida ligada de manera verificable a los operandos. Cualquier ampliación debe especificar la semántica de readonly, el alcance de la referencia y temporales, y cubrir aliases, escape, reemplazo y negativos; no hay una implementación aprobada ni un defecto corregido en esta sección.

La barrera de los temporales **binarios** es independiente de ese retorno: en `CallAnalyzer.cpp`, la selección de un operador precompilado `in` descarta un operando temporal no direccionable cuyo tipo posee almacenamiento transitivamente, incluso si el operador devuelve `bool`. La prueba de locales binarios y el fallo de temporales están en el registro v3. El retorno escalar por sí solo no demuestra que el operador no publique un alias como efecto lateral; no se recomienda quitar la barrera hasta contar con una prueba o contrato de efectos fiable. El fixture heredado contiene controles de rechazo de posibles escapes, que deben permanecer sin ejecutar.
