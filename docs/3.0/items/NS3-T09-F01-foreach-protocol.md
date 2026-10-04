# NS3-T09-F01 — Protocolo `foreach` 3.0 / Foreach protocol 3.0

## Estado

Implementado y comprobado mediante la prueba dirigida `nsc_ns3_t09_foreach_protocol_tests`.
No se ejecutó baseline ni una suite general.

## Superficie del lenguaje

NativeSharp 3.0 distingue de forma explícita el modo de enlace del elemento:

```nativesharp
foreach (T value in source) { }          // snapshot por valor
foreach (ref T value in source) { }      // alias de sólo lectura
foreach (ref mut T value in source) { }  // alias mutable exclusivo
```

Los dos modos por referencia requieren `<Language>3</Language>`. Un rango entero no ofrece
almacenamiento estable y, por tanto, sólo admite iteración por valor.

## Resolución del protocolo

- Los arreglos conservan el lowering indexado existente.
- `NativeSharp.Core.Span<T>` y `ReadOnlySpan<T>` se reconocen por su identidad canónica. Se
  recorren mediante `Length()` y `Data()[index]`; el elemento usa una dirección cruda y no una
  comprobación de bounds administrada adicional.
- `ReadOnlySpan<T>` rechaza `ref mut`.
- Los tipos de usuario se resuelven estáticamente con el patrón `GetEnumerator`, `MoveNext` y
  `Current`. Un miembro aportado por un `impl` de trait participa en la misma resolución; no hay
  reflexión ni búsqueda por nombre en runtime.
- Un `Current` por valor sólo admite iteración por valor. `ref`/`ref mut` exigen almacenamiento
  direccionable y `ref mut` exige además almacenamiento asignable.

## Ownership y cleanup

- La variable por valor conserva la regla de sólo lectura del `foreach` 2.x.
- `ref` es una referencia local de sólo lectura y conserva el origen de escape.
- `ref mut` reutiliza el análisis de préstamos exclusivos y los place keys de referencias
  locales; no introduce una segunda implementación de aliasing.
- Un Span prestado puede copiarse únicamente al snapshot interno e imposible de escribir
  `$__foreach_source`; no se abre una excepción para otros valores move-only.
- Los iteradores de referencia siguen almacenándose como RAII. Su destructor se ejecuta en
  salida normal, `break`, `return` y propagación mediante el mismo grafo de cleanup de los loops
  ordinarios.

## Cambios realizados

- `src/Parser/Parser.cpp`
  - parseo de `ref` y `ref mut`;
  - candidatos separados para array, Span y protocolo de enumerador;
  - rechazo de referencias sobre rangos sin storage;
  - metadato interno de modo de iteración.
- `src/Frontend/ProjectMethodBodyModel.cpp`
  - referencias foreach readonly/mutables sobre el modelo existente de locales `ref`;
  - selección canónica de Span/ReadOnlySpan;
  - protección de ReadOnlySpan y snapshot prestado del source;
  - resolución ordinaria de miembros y witnesses de trait.
- `src/Frontend/ProjectOopHirModel.cpp`
  - selección idéntica del candidato ya validado por Sema.
- `tests/unit/NativeSharp3T09ForeachProtocolTests.cpp`
  - diez verificaciones dirigidas O0/NSLink y negativas.
- `CMakeLists.txt`
  - target aislado `nsc_ns3_t09_foreach_protocol_tests`.

## Evidencia puntual

`PASS: 10 checks`

La prueba cubre arrays, Span, valor, `ref`, `ref mut`, edición 2/3, move-only, rango sin storage,
trait de usuario, cleanup por `break`, `Current` no direccionable y MIR con
`raw-element-address` sin `array-element-address` en el recorrido de Span.
