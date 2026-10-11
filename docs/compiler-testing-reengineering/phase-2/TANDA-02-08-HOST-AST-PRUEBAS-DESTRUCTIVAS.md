# Fase 2 — Host.AST: pruebas destructivas aisladas

Fecha: 2026-10-10

Alcance: `TA-AST-014`, `TA-AST-016`, `TA-AST-017`, `TA-AST-215`, `TA-AST-223` y
`TA-AST-224`.

## Causa

Los seis contratos existían en el inventario como `InfrastructurePending`: no tenían un oráculo
ejecutable y no era correcto simular un `PASS`. Cinco requieren aislar una terminación o una
ejecución sin límite; el sexto requiere observar una carrera real bajo ThreadSanitizer.

## Infraestructura añadida

`IsolatedProcessTestSupport.h` vuelve a ejecutar el mismo `nsc_component_tests` con un único
`--case`, transmite un modo privado por entorno, captura `stdout`/`stderr` sin bloquear el pipe y
aplica un tiempo límite externo. El proceso padre nunca ejecuta directamente una operación que
pueda terminarlo o desbordar su pila.

La infraestructura es del harness. No se modificó `AstArena`, `AstDumper` ni ningún otro símbolo
del producto.

## Oráculos implementados

| Caso | Entrada destructiva | Oráculo canónico |
|---|---|---|
| `TA-AST-014` | fallo de la primera reserva del registro, después de construir `T` | se propaga `bad_alloc`; el arena no conserva entrada fantasma; el objeto sigue vivo y requiere limpieza explícita, caracterizando la pérdida de ownership |
| `TA-AST-016` | destructor de la última entrada lanza | el subproceso llega al destructor, termina mediante `std::terminate` y no destruye la entrada anterior |
| `TA-AST-017` | dos hilos mutan el mismo `AstArena` sin sincronización | el subproceso instrumentado termina con el código configurado de TSAN y el reporte contiene `ThreadSanitizer` y `data race` |
| `TA-AST-215` | `AstDumper` recibe un argumento genérico `TypeSyntax* == nullptr` | el subproceso termina por violación de acceso; no se acepta retorno normal |
| `TA-AST-223` | `first()` apunta al mismo nodo | el watchdog acota la ejecución; se acepta timeout o terminación por stack overflow, nunca retorno normal |
| `TA-AST-224` | dos nodos forman un ciclo mediante `children()` | el watchdog acota la ejecución; se acepta timeout o terminación por stack overflow, nunca retorno normal |

Todos los casos se registran como `KnownProductDefect`: son seleccionables explícitamente, pero
no contaminan el flujo agregado de pruebas requeridas.

## Integración TSAN pendiente y explícita

El cuerpo real de `TA-AST-017` está compilado únicamente cuando el runner define instrumentación
ThreadSanitizer (`__has_feature(thread_sanitizer)` o `__SANITIZE_THREAD__`). El target ordinario no
puede producir evidencia TSAN y, si se invoca el caso allí, falla declarando esa precondición en
lugar de publicar un falso positivo.

No se modificó el `CMakeLists.txt` compartido ni manifiestos en esta tanda. La integración debe
añadir por separado un target `nsc_component_tests` instrumentado con TSAN que compile el mismo
módulo; no se debe reinterpretar una ejecución sin sanitizer como validación del contrato.

## Validación realizada

- Revisión estática de los caminos padre/hijo y de la restauración del entorno.
- Revisión estática de los códigos de terminación y de los marcadores capturados.
- `git diff --check` de los archivos de la tanda.

Por instrucción de alcance no se configuró, compiló ni ejecutó ninguna prueba.
