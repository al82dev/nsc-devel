# Fase 2 — Host.AST: pruebas destructivas aisladas

Fecha: 2026-10-10

Alcance ejecutable: `TA-AST-014`, `TA-AST-016`, `TA-AST-215`, `TA-AST-223` y
`TA-AST-224`.

Trabajo futuro fuera del pipeline y del manifiesto: `TA-AST-017`.

## Causa

Los cinco contratos ejecutables existían en el inventario como `InfrastructurePending`: no tenían
un oráculo ejecutable y no era correcto simular un `PASS`. Todos requieren un límite de proceso
separado porque provocan fallo de asignación, terminación anómala o ejecución sin límite.

## Infraestructura añadida

`IsolatedProcessTestSupport.h` vuelve a ejecutar el mismo proceso con un único
`--case`, transmite un modo privado por entorno, captura `stdout`/`stderr` sin bloquear el pipe y
aplica un tiempo límite externo. El proceso padre nunca ejecuta directamente una operación que
pueda terminarlo o desbordar su pila.

El módulo forma parte de la lista de fuentes del target consolidado ordinario. Por ello, el
reemplazo global de `operator new` y el registro de los cinco casos destructivos están compilados
solo cuando un target dedicado define `NSC_AST_DESTRUCTIVE_TESTS_ENABLED`. Sin ese macro, el
binario normal conserva su asignador y no publica `TA-AST-014`, `TA-AST-016`, `TA-AST-215`,
`TA-AST-223` ni `TA-AST-224`.

El target `nsc_component_tests_destructive` compila únicamente el registro, el módulo AST
destructivo y las fuentes AST mínimas requeridas. Su `main` registra solo esos cinco IDs; no carga
los otros casos del host ni acepta `--list-pending`. El controller, el gate del inventario, el gate
de `Host.AST` y NSC Builder 3 resuelven el ejecutor por su nombre propio. Las cinco filas del
manifiesto son `KnownProductDefect`, perfil `NIGHTLY`, y quedan fuera del flujo PR ordinario.

La infraestructura es del harness. No se modificó `AstArena`, `AstDumper` ni ningún otro símbolo
del producto.

## Oráculos implementados

| Caso | Entrada destructiva | Oráculo canónico |
|---|---|---|
| `TA-AST-014` | fallo de la primera reserva del registro, después de construir `T` | `bad_alloc` se propaga, pero una asignación que queda viva fuera del arena produce `FAIL`; la prueba limpia el huérfano solo después de registrar el defecto |
| `TA-AST-016` | destructor de la última entrada lanza | `std::terminate` o la omisión de la destrucción restante producen `FAIL`; solo una salida normal con limpieza completa satisface el contrato |
| `TA-AST-215` | `AstDumper` recibe un argumento genérico `TypeSyntax* == nullptr` | terminación o timeout producen `FAIL`; solo una salida normal satisface el contrato de robustez |
| `TA-AST-223` | `first()` apunta al mismo nodo | timeout o terminación por stack overflow producen `FAIL`; solo finalización normal antes del watchdog satisface el contrato |
| `TA-AST-224` | dos nodos forman un ciclo mediante `children()` | timeout o terminación por stack overflow producen `FAIL`; solo finalización normal antes del watchdog satisface el contrato |

Los casos disponibles se registran como `KnownProductDefect`. Reproducir el defecto genera el
`FAIL` que el ejecutor transforma en defecto conocido; si el producto empieza a satisfacer el
contrato, el `PASS` se vuelve `UNEXPECTED_PASS` y obliga a retirar la cuarentena.

## Trabajo futuro no integrado

`TA-AST-017` no pertenece al target destructivo, no tiene fila en el manifiesto y no se publica
como caso pendiente del runner ordinario. Su posible prueba con ThreadSanitizer queda documentada
únicamente como trabajo futuro; esta integración no añade target, flags ni infraestructura TSAN.

## Validación realizada

- Revisión estática del límite entre el target ordinario y el destructivo.
- Revisión estática de los caminos padre/hijo, los códigos de terminación y los watchdogs.
- Revisión estática del enrutamiento por ejecutor en controller y NSC Builder 3.

Por instrucción de alcance no se configuró, compiló ni ejecutó ninguna prueba.
