# Superficie de declaraciones, telemetría de tiempos y coherencia de traits

Fichas LKF-130, 157, 243, 350, 361, 505, 591, 595 y 604.

## Defectos corregidos

- **LKF-505.** `int[0] a;` llegaba al lowering LLVM como un tipo `builtin:int[0]` no representable
  ("C2.1 cannot map fixed-array type key"). Una extensión constante cero de un arreglo local se
  rechaza ahora en el análisis: "a fixed-size array local must have a positive length", igual que la
  regla que ya tenían los campos.
- **LKF-604.** `public int A, B;` producía "expected ';', found ','" seguido de una cascada. El parser
  dice una vez que cada campo lleva su propia declaración y salta el resto de la lista. Los locales
  separados por coma siguen admitidos.
- **LKF-243.** La traza JSON de tiempos escribía 0 cuando la salida no traía una medición (memoria
  pico, lecturas de descriptores, total). Una métrica ausente se escribe ahora `null`; un cero medido
  sigue siendo 0 (`nsc.compilation-telemetry`, caso nuevo). El escape de controles es `286c64509`.
- **NSC-NEW-023.** Un `impl ILocal for Foreign` (trait local sobre clase importada) volvía a recolectar
  todas las partes de `Foreign` en `collectExternalTraitImplMembers`; los miembros propios quedaban
  registrados dos veces y `new Foreign()` fallaba con "ambiguous constructor invocation". Solo se
  recolecta la parte impl nueva (`collectTypePartMembers`).

## Fronteras con diagnóstico (sin cambio)

- **LKF-591.** `ref var x = ref a[0];` se rechaza: "reference locals require an explicit element type".
  Con el tipo escrito ejecuta.
- **LKF-595.** Una propiedad solo con setter se rechaza con la alternativa de un método; un tipo
  anidado se declara y se usa.

## Verificado

- **LKF-130.** `nsc.item350.aligned-simd-memory` y `nsc.item351.unaligned-simd-memory` aprueban en
  O0/O2; el contrato de alineación de las variantes alineadas es precondición del llamador (nivel
  crudo).
- **LKF-350.** `nsc.ns3.t05.associated-items` aprueba: constante asociada, tipo y rechazo de expresión
  no constante.
- **LKF-157 / LKF-361.** El canal de progreso detallado de genéricos sigue bajo `#if 0`; es
  instrumentación deshabilitada, no ausencia de telemetría: `--progress=text|json|files` y la traza de
  tiempos están activos (`nsc.project-commands`, `nsc.compilation-telemetry`).

## Prueba desactualizada corregida

`nsc.ns3.t05.impl-coherence` esperaba rechazar `impl ILocal for Foreign` (trait local, clase
importada). Desde `8f5b16307` (T08-F05) el módulo que posee el trait puede implementarlo para una clase
ajena; el caso huérfano real es el trait importado sobre la clase importada ("orphan impl: the module
owns neither the target type … nor the trait"). La prueba comprueba ahora ese rechazo y la ejecución
del caso con trait local.

## Prueba

`nsc.compiler.declaration-surface-boundaries`: 13 casos, cada rechazo con su control, O0/O2.
