# Tracking — Fase 2

| Ola | Alcance | Estado | Casos previstos | Implementados | Verificados |
|---:|---|---|---:|---:|---:|
| 0 | Infraestructura mínima | LISTA | 0 de producto / 3 sintéticos | 3 | 3 |
| 1 | Fundaciones host | EN CURSO | 747 | 743 | 736 |
| 2 | Sintaxis y semántica frontal | EN CURSO | 1,197 | 110 | 0 |
| 3 | IR y optimización | PENDIENTE | 1,784 | 0 | 0 |
| 4 | Sesión, frontend y method bodies | PENDIENTE | 920 | 0 | 0 |
| 5 | Backend, ABI, runtime y enlace | PENDIENTE | 1,600 | 0 | 0 |
| 6 | Producto, driver y servicio | PENDIENTE | 1,112 | 0 | 0 |
| 7 | Compilador self-host | PENDIENTE | 1,027 | 0 | 0 |
| 8 | Infraestructura y oráculos self-host | PENDIENTE | 726 | 0 | 0 |
| 9 | Build, instalación y paquete de NSC | PENDIENTE | 474 | 0 | 0 |

Estados permitidos: `PENDIENTE`, `EN CURSO`, `LISTA`, `BLOQUEADA`.

Los conteos separan implementación de verificación: un caso rojo por defecto
conocido cuenta como implementado, se reporta `KNOWN_FAILURE` y no aumenta la
columna `Verificados`. No se elimina ni se convierte en `PASS`; tampoco bloquea el
flujo ordinario. Los casos que requieren infraestructura todavía inexistente no
cuentan como implementados.

| Último corte | Estado acumulado | Siguiente caso |
|---|---:|---|
| Tanda 02.01 — Host.Lexer materializado | 110/1,197 | verificar Lexer e integrar Host.AST |

Desglose de la ola 1: `736 PASS`, `7 KNOWN_FAILURE` implementados y ejecutables
de forma individual, y `4` casos pendientes de infraestructura especial. Los
siete defectos permanecen rojos sin interferir con el flujo ordinario.

La ola 1 conserva cuatro limitaciones de infraestructura (`TS-SRC-008`,
`TS-SRC-040`, `TS-SRC-041`, `TS-SRC-049`). Permanecen en el denominador y en el
registro de pendientes, pero no bloquean la implementación independiente de la
ola 2. La ola 1 no se declara `LISTA`.

Desglose provisional de `Host.Lexer`: `110/110` casos implementados, con
evidencia dirigida de `109 PASS` y `1 KNOWN_FAILURE` (`TL-LEX-075`). La columna
`Verificados` permanece en cero hasta ejecutar el target y el gate integrados
desde el commit de la tanda. No se declaró `PASS` por resultados aislados.
