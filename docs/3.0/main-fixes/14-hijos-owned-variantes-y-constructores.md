# Hijos `owned` en variantes y constructores del objeto local (M1-06 y M1-07)

Rama `development-main-fixes`. Continúa el documento 01. Evidencia:

- `A:\nsc-main-fixes-evidencia\claude\m1-06` (sondas a–f);
- `m1-run` (sección `Mixed`);
- `logs\m1-06-*`.

## Sondas antes y después

| Sonda | Forma | Antes | Ahora |
| --- | --- | --- | --- |
| a | `owned Child child = new Child();` (inicializador de campo) | «use of unassigned local variable» | igual (límite) |
| b | `child = new Child(3);` en el constructor | «construction or destruction is not proven safe» | aceptado |
| c | `child = new Sub();` con `Sub : Child` | «not proven safe» | aceptado |
| d | constructor del local con `throws` | no se pudo escribir la sonda: `Exception` no es visible sin la biblioteca base | sin medir (límite) |
| e | destructor del local con efecto estático escalar | aceptado | aceptado |
| f | inicializador escalar de campo | aceptado | aceptado |

## Contrato agregado (decisión delegada en Claude)

1. **`campo = new T(args)` en un constructor del objeto local.** Los argumentos se aceptan solo si son
   literales escalares (entero, booleano, flotante) con conversiones integradas: identidad, promoción
   integral, numérica o entero constante. Así no pueden llevar el receptor ni ejecutar código de usuario.
   Esto vale en las dos pruebas:
   - `classLocalOwnedChildAssignmentField` (efectos del receptor);
   - `classLocalChildConstructionIsClosed` (modelo de construcción).
2. **Tipo del hijo.** Puede ser una clase derivada del tipo declarado del campo (`classTypeKeyDerivesFrom`).
   Se destruye con su propio descriptor; su cadena de destructores conserva la prueba de no lanzar ni
   suspenderse.

## Controles que cambian (decisión 3)

Dos controles negativos pasan a positivos:

- `LocalOwnedChildDerivedValue`;
- `LocalOwnedChildArgumentValue`.

`m1-run` los ejecuta con `Mixed`. En O0 y O2 exige:

- `SubChild` se construye y se destruye una vez (también corre `~Child`);
- `Valued(7)` recibe 7 y se destruye una vez.

## Pruebas

- `nsc.class-value-origins`, `nsc.ns3.t06.escape-summaries`, `nsc.stage50_7.borrow-escape-analysis` y
  `nsc.stage56.c7-definite-assignment` pasan.
- `m1-run`: PASS O0 y O2.
- Bibliotecas: `logs\libs-out-m1-06.log`.

## Límites (M1-06 y M1-07, no cubiertos)

- **(a) Inicializador `owned` con `new` en la declaración del campo.** El tipo no se trata como objeto local
  implícito, y el diagnóstico actual es «use of unassigned local variable». No se cambió: requiere decidir
  cómo y cuándo corre el inicializador para un objeto del marco.
- **Argumentos no literales en el `new` del hijo** (parámetros, campos, expresiones): siguen rechazados.
- **Constructores del objeto local.** El constructor siempre es el de por omisión: la sintaxis `T value;` no
  admite argumentos. Un constructor con `throws` sigue rechazado por la regla previa (sin sonda ejecutada,
  ver d).
- **Destructores.** Se aceptan con efectos escalares estáticos (e). Un destructor con `throws` o con
  suspensión sigue rechazado.
