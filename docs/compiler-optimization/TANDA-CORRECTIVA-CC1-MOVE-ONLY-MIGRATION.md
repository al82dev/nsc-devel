# Tanda correctiva: migración CC1 a move-only por defecto

## Problema

La implementación de NS3-T06-F01 hizo move-only por defecto a los valores declarados por el
usuario. Tres fixtures positivos anteriores siguieron usando copia implícita y comenzaron a fallar
antes de probar la característica para la que fueron creados.

La auditoría confirmó que los diagnósticos eran correctos según el contrato de 3.0; el defecto
estaba en las fixtures que no fueron migradas por el commit que activó la regla.

## Cambios

- `c55_ns3_calls_initializers` declara `ReadOnlySpan<T>` y `Pair` como Copy; `GenericPair<T>` es
  Copy únicamente bajo `where T : copy`. Son carriers POD y la prueba necesita copiarlos.
- `c56_ns3_null_with` declara `Pair` como Copy, porque el caso valida `with` sobre un valor POD y
  no la transferencia de ownership.
- `c67_ns3_optional_coalescing` declara `Optional<T> where T : copy` y `LargeValue` como Copy. El
  protocolo comprobado por E017 usa payloads POD; extraer un T move-only desde el field interno
  requiere movimientos parciales de fields, cuyo contrato pertenece a NS3-T06-F02 y no debe
  simularse con un `move(...)` que el lenguaje todavía rechaza correctamente.

## Archivos

- Modificado: `tests/cc1/c55_ns3_calls_initializers/Main.ns`.
- Modificado: `tests/cc1/c56_ns3_null_with/Main.ns`.
- Modificado: `tests/cc1/c67_ns3_optional_coalescing/Main.ns`.
- Nuevo: este documento de trazabilidad.

## Mejora

Las fixtures vuelven a medir llamadas, inicializadores, `with` y coalescencia opcional sin violar
el contrato de movimiento. La corrección no relaja el compilador ni convierte implícitamente tipos
move-only en Copy. La ampliación futura de Optional a payloads move-only queda documentada como
dependencia de NS3-T06-F02.

## Validación permitida

Los tres proyectos se compilan y ejecutan individualmente en O0 con NSLink. No se ejecuta baseline,
CTest general ni matriz de optimizaciones.

Resultado: `c55`, `c56` y `c67` compilaron individualmente y terminaron con exit 42. Baseline,
CTest general y matrices no ejecutados.
