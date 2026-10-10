# Validación 163: reejecución de fallos históricos

## SCOPE LOCK

- Objetivo: exponer en `Historial` una acción visible `Reejecutar fallos` para
  la corrida cargada.
- Autoridad dirigida:
  `_nscbuilder3/history/20261010-142113-a514d35c`.
- Proyección obligatoria:
  `retry_variants(loaded.variants, loaded.outcomes)`.
- Universo correctivo esperado para la autoridad: `439` claves exactas,
  formadas por `274 FAIL`, `4 INFRASTRUCTURE_ERROR` y `161 BLOCKED`.
- Archivos autorizados: `nscbuilder3.py` y este documento.
- Fuera de alcance: modificar `TRACKING.md`, cambiar el ejecutor, alterar la
  corrida histórica, ejecutar las `439` variantes o iniciar una corrida
  integral.
- Comportamiento preservado: reejecución individual, reejecución completa,
  filtros históricos, evidencia inmutable y reejecución de fallos activos.
- Prohibido: incluir `PASS` o `KNOWN_FAILURE`, reconstruir una selección más
  amplia desde los filtros de la interfaz o inferir variantes no registradas.

## Causa

Builder 3 ya conserva en una corrida histórica las variantes exactas y sus
resultados, y `retry_variants()` ya proyecta de forma determinista únicamente
los estados terminales reejecutables. La interfaz de `Historial` sólo exponía
acciones para una fila individual o para la corrida completa; el filtro
`Mostrar sólo fallos` no iniciaba una reejecución.

## Corrección

La página de la corrida histórica incorpora el botón `Reejecutar fallos`. La
acción calcula su plan directamente con
`retry_variants(loaded.variants, loaded.outcomes)`, conserva el orden y las
claves `caso|build|optimización` de la corrida cargada, restaura su
configuración y entrega exclusivamente ese plan al ejecutor existente.

El botón queda deshabilitado si no hay estados terminales reejecutables y
durante cualquier otra operación activa. `PASS` y `KNOWN_FAILURE` siguen fuera
del conjunto porque no pertenecen a `TERMINAL_FAILURES`.

## Contrato de validación

La implementación se commitea antes de ejecutar cualquier comprobación. Tras
ese commit se permiten únicamente:

1. validación sintáctica de `nscbuilder3.py`;
2. pruebas unitarias dirigidas de la proyección de reintentos;
3. comprobación estática de que la corrida autoridad produce `439` claves
   únicas sin `PASS` ni `KNOWN_FAILURE`;
4. `--gui-smoke`, que construye y cierra la interfaz sin configurar, compilar
   ni ejecutar pruebas.

No se ejecutará ninguna de las `439` variantes en esta tanda.

## Estado previo a validación

- Implementación: terminada.
- Commit funcional previo a pruebas: preparado en esta tanda; la evidencia de
  validación se registrará después de crearlo.
- Validación sintáctica y dirigida: pendiente.
- Reejecución de las `439` variantes: no autorizada en esta tanda.
- Unidad `C:` modificada: no.
