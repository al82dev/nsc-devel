# Tanda operativa 13 — T06 move, borrow, lifetime y drop

## Objetivo

Auditar y reforzar la vertical productiva T06-F01..F05 en el compilador real, sin aceptar como
implementación la máquina de estados aislada `OwnershipSemantics`. El alcance operativo fue llegar
desde fuente edition 3 hasta semántica, HIR/MIR, LLVM y PE O0 con un movimiento parcial observable,
destrucción exactamente una vez y conflictos borrow diagnosticados antes del backend.

Esta tanda no intenta declarar completo todo T06. Named lifetimes, split borrows, `defer`, pinning,
escape summaries públicos y drop glue cross-DLL requieren sus propios contratos y permanecen
abiertos, principalmente en la tanda 14.

## Auditoría realizada

Se siguieron las rutas productivas de:

- creación, movimiento y uso de locals RAII y parámetros `owned`;
- estados `movedRaiiLocals_`, `maybeMovedRaiiLocals_`, `movedOwnedParameters_` y sus joins;
- `MethodLocalBorrowBinding`, procedencia, scopes e invalidación histórica;
- move total, move de elemento y move de campo owned;
- bindings method-body, HIR `MoveOwnedField`/`MoveArrayElement`, MIR y lowering LLVM;
- regiones de cleanup, destrucción automática y rutas de salida.

La base 2.x ya resolvía movimientos totales, use-after-move, joins de control, borrows léxicos,
cleanup y el load+null-store de movimientos parciales. El defecto encontrado estaba entre ambas
mitades: las operaciones parciales no consultaban los préstamos del objeto que contenía la ranura.

## Defecto corregido

Antes del cambio este patrón podía atravesar method-body analysis:

```text
owned Holder owner = ...;
borrowed Holder view = owner;
owned Payload extracted = move(owner.Left);
```

`view` conservaba un préstamo válido mientras `move(owner.Left)` mutaba el storage observable del
mismo owner. El chequeo existente protegía `move(owner)` y el overwrite de `owner`, pero no la
proyección parcial.

La corrección:

1. desenrolla `MemberExpression`/`IndexExpression` hasta el identificador raíz;
2. sólo reconoce roots que realmente son local RAII o parámetro `owned`;
3. consulta el mismo estado de borrow productivo usado por move total;
4. rechaza field o element move si existe un préstamo todavía activo;
5. guarda el ordinal del root en `MethodMoveBinding` para dumps y tooling posterior.

El control es conservador sobre todo el owner. No se afirma todavía split borrow por campos
hermanos: permitirlo exige un modelo de lugares/proyecciones y prueba de disjunción T06-F03.

## Cambios por archivo

### Modificados

- `include/nativesharp/Frontend/ProjectMethodBodyModel.h`
  - añade `partialSourceOwnerLocalOrdinal` y `partialSourceOwnerParameterOrdinal` al binding de move;
  - separa la ranura extraída del owner contenedor que permanece vivo.
- `src/Frontend/ProjectMethodBodyModel.cpp`
  - resuelve el root de proyecciones field/index;
  - aplica exclusividad borrow a movimiento parcial local y de parámetro;
  - propaga el root al binding tanto para field como para array/fixed-array/pointer element.
- `src/Frontend/ProjectMethodBodyModelDumper.cpp`
  - publica `partial-owner-local`/`partial-owner-parameter` de forma determinista.
- `tests/unit/Stage50_5LocalBorrowsLifetimesTests.cpp`
  - añade tres regresiones causales: field/local, field/parameter y element/local.
- `tests/unit/Cc1CorrectnessTests.cpp`
  - registra c63 para la certificación final que ejecutará Alberto.
- `docs/3.0/TANDA-06-OWNERSHIP-PRESTAMOS-Y-DESTRUCCION.md`
  - reemplaza la afirmación del helper aislado por el estado productivo y su deuda real.

### Nuevos

- `tests/cc1/c63_ns3_move_borrow_drop/c63_ns3_move_borrow_drop.nsproj`
  - proyecto edition 3 mínimo para la vertical.
- `tests/cc1/c63_ns3_move_borrow_drop/Main.ns`
  - extrae un campo owned, conserva utilizable el campo hermano y cuenta dos destructores exactos.
- `docs/3.0/TANDA-OPERATIVA-13-T06-MOVE-BORROW-DROP.md`
  - handoff técnico, decisiones, archivos, evidencia y límites de esta tanda/commit.

No se creó código fuente ni evidencia fuera de `A:`. No se usó ni modificó `C:\native-sharp`.

## Mejora concreta

- Un borrowed view ya no puede coexistir con una mutación por partial move del mismo owner.
- La regla es uniforme para owner local, parámetro `owned`, campo y elemento.
- El backend conserva el mecanismo eficiente ya existente: load + null-store; no añade heap,
  contador runtime ni drop flag general cuando la propia ranura nula basta.
- Dumps pueden explicar qué owner raíz gobernó la decisión, base para LSP/debugger T29.
- c63 detecta tanto double drop como cleanup perdido, no sólo que el programa enlace.

## Validación dirigida ejecutada

1. Target `nsc` construido en `build-clang`, configuración Debug/O0: correcto.
2. c63 con `A:\nsc-devel\build-clang\bin\nsc.exe --emit-project-oop-exe ... -O0`:
   emisión correcta y proceso termina en 42.
3. Dump method-body dirigido: `move-bindings=3` y el move parcial contiene
   `partial-owner-local=0`.
4. Dump MIR dirigido: contiene `move-owned-field`, ambos cleanups automáticos y ruta LIFO
   `cleanup-path=#3->#2`.
5. Tres proyectos negativos individuales mediante `--check-project-method-bodies`:
   - field de owner local prestado: exit 1 y diagnóstico exacto;
   - field de parámetro `owned` prestado: exit 1 y diagnóstico exacto;
   - array element de owner prestado: exit 1 y diagnóstico exacto.

El build principal está configurado con `NSC_BUILD_TESTS=OFF`; las tres regresiones unitarias quedan
compilables/ejecutables por el baseline final, mientras esta tanda verificó los mismos casos contra
el `nsc.exe` recién construido. No se reconfiguró el árbol ni se usó una build Release existente.

No se ejecutó baseline, CTest general, O1/O2/O3, instalación, deploy ni operación remota.

## Deuda explícita

- T06-F01 no tiene aún un trait/contrato `Copy` productivo ni move-by-default generalizado.
- T06-F02 necesita estado estático por campo, reinicialización y aggregates nested/variants.
- T06-F03 necesita place graph y split/reborrow con disjunción demostrable.
- T06-F04 necesita lifetimes nombrados, outlives, retornos prestados y NSMETA.
- T06-F05 necesita `defer` y composición con propagación/panic T07.
- T06-F06..F08 (pin, escape summary y generic/cross-DLL drop glue) pertenecen a tanda 14.
- Tuple/record/variant owning y owning `with` sólo pueden ampliarse después de esos estados.

Por ello la tanda operativa 13 queda cerrada como una vertical productiva y medible, pero el bloque
maestro T06 conserva estado `IMPLEMENTING`.
