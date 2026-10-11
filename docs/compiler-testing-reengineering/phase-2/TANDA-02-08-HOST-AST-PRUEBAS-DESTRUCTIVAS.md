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

El módulo forma parte de la lista de fuentes del target consolidado ordinario. Por ello, el
reemplazo global de `operator new` y el registro de los cinco casos destructivos están compilados
solo cuando un target dedicado define `NSC_AST_DESTRUCTIVE_TESTS_ENABLED`. Sin ese macro, el
binario normal conserva su asignador y no publica `TA-AST-014`, `TA-AST-016`, `TA-AST-215`,
`TA-AST-223` ni `TA-AST-224`.

La infraestructura es del harness. No se modificó `AstArena`, `AstDumper` ni ningún otro símbolo
del producto.

## Oráculos implementados

| Caso | Entrada destructiva | Oráculo canónico |
|---|---|---|
| `TA-AST-014` | fallo de la primera reserva del registro, después de construir `T` | `bad_alloc` se propaga, pero una asignación que queda viva fuera del arena produce `FAIL`; la prueba limpia el huérfano solo después de registrar el defecto |
| `TA-AST-016` | destructor de la última entrada lanza | `std::terminate` o la omisión de la destrucción restante producen `FAIL`; solo una salida normal con limpieza completa satisface el contrato |
| `TA-AST-017` | dos hilos mutan el mismo `AstArena` sin sincronización | código de fallo o reporte `ThreadSanitizer: data race` producen `FAIL`; solo ausencia del reporte y salida normal satisfacen el contrato |
| `TA-AST-215` | `AstDumper` recibe un argumento genérico `TypeSyntax* == nullptr` | terminación o timeout producen `FAIL`; solo una salida normal satisface el contrato de robustez |
| `TA-AST-223` | `first()` apunta al mismo nodo | timeout o terminación por stack overflow producen `FAIL`; solo finalización normal antes del watchdog satisface el contrato |
| `TA-AST-224` | dos nodos forman un ciclo mediante `children()` | timeout o terminación por stack overflow producen `FAIL`; solo finalización normal antes del watchdog satisface el contrato |

Los casos disponibles se registran como `KnownProductDefect`. Reproducir el defecto genera el
`FAIL` que el ejecutor transforma en defecto conocido; si el producto empieza a satisfacer el
contrato, el `PASS` se vuelve `UNEXPECTED_PASS` y obliga a retirar la cuarentena.

## Integración TSAN pendiente y explícita

El cuerpo y el registro de `TA-AST-017` existen únicamente cuando el runner define instrumentación
ThreadSanitizer (`__has_feature(thread_sanitizer)` o `__SANITIZE_THREAD__`). El binario ordinario no
expone el identificador, porque sin sanitizer no puede ejecutar ni validar ese contrato.

No se modificó el `CMakeLists.txt` compartido ni manifiestos en esta tanda. La integración debe
añadir por separado un target `nsc_component_tests` instrumentado con TSAN que compile el mismo
módulo; no se debe reinterpretar una ejecución sin sanitizer como validación del contrato.

## Validación realizada

- Revisión estática de los caminos padre/hijo y de la restauración del entorno.
- Revisión estática de los códigos de terminación y de los marcadores capturados.
- `git diff --check` de los archivos de la tanda.

Por instrucción de alcance no se configuró, compiló ni ejecutó ninguna prueba.
