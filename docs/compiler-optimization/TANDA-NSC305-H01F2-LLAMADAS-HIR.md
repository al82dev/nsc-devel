# NSC 3.0.5 — H01F2: extracción de llamadas HIR

## SCOPE LOCK

| Dimensión | Alcance |
|---|---|
| Objetivo | Mover `CallExpression` y sus helpers exclusivos a `HirCallLowering.cpp`. |
| Naturaleza | Movimiento físico y creación de fronteras privadas; ninguna reescritura del algoritmo. |
| Símbolos | `lowerCallExpression`, `lowerEventBackingField`, `StringCallLowering`, `stringCallLowering`. |
| Protegido | Intrinsics, resolución, receiver, dispatch, argumentos nombrados/default/packs, conversiones, borrows, ownership, cleanup y orden de evaluación. |
| Fuera de alcance | Construcción, stackalloc, collection/aggregate, async, excepciones, conversiones e iteratividad H02. |
| Terminación | Definiciones únicas, equivalencia mecánica, `nsc` construido y focos HIR 34/34 y 12/12. |

## Resultado estructural

- El caso `CallExpression` salió del dispatcher y quedó como llamada directa a
  `lowerCallExpression`.
- Las 1,467 líneas del cuerpo se compararon contra el `HEAD` anterior después de normalizar sólo
  indentación: 1,467 frente a 1,467, delta `0`.
- El helper de backing de eventos, compartido con asignaciones, pasó a
  `lowerEventBackingField`; su cuerpo de 21 líneas se conservó.
- `StringCallLowering` y `stringCallLowering`, exclusivos de la bajada de llamadas core, se
  movieron íntegros al namespace anónimo de `HirCallLowering.cpp`.
- `HirExpressionMachine.cpp` quedó en 3,781 líneas y `HirCallLowering.cpp` en 2,097 líneas.
- No se cambió sintaxis, diagnóstico, API, ABI, formato, metadata ni contrato observable.

## Incidencia controlada de frontera

La primera compilación rechazó `StringCallLowering` como identificador no declarado en el nuevo
translation unit y, simétricamente, como helper sin uso en el anterior. Eso confirmó que la tabla
era una dependencia exclusiva del bloque movido. Se trasladó su definición completa sin modificar
entradas, aridades ni nodos HIR; no se corrigió lógica ni se añadió fallback.

## Evidencia focal

- `git diff --check`: limpio.
- `cmake --build A:/nsc-devel/build-test-infrastructure --target nsc_hir_tests nsc_namespace_function_hir_tests`: exit `0`.
- `nsc_hir_tests`: `PASS 34/34`, exit `0`.
- `nsc_namespace_function_hir_tests`: `PASS 12/12`, exit `0`.
- `cmake --build A:/nsc-devel/build-clang --target nsc`: exit `0`.
- Contrato de runtime estático de `nsc.exe`: aprobado.

H01F queda cerrado. Esta tanda no afirma validación integral ni ejecuta baseline, deploy o batería
completa.
