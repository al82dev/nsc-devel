# Tanda de validación 171 — Inferencia genérica de Frontend

Fecha: 2026-10-10

## Estado

PENDIENTE DE VALIDACIÓN DIRIGIDA.

## SCOPE LOCK

Esta tanda corrige exclusivamente los seis consumidores válidos de
`CF-FRONT-021`, «Generic inference and specialization»:

- `TF-FRONT-161` — nominal mínimo;
- `TF-FRONT-162` — matriz de clases válidas;
- `TF-FRONT-163` — boundary de fuente vacía;
- `TF-FRONT-164` — límites y transiciones;
- `TF-FRONT-167` — determinismo;
- `TF-FRONT-168` — escala.

Fuera del alcance quedan el compilador, el manifiesto, Builder 3,
`TRACKING.md`, el helper compartido de ausencia y cualquier otro caso. Las
variantes inválida y de dependencia fallida (`TF-FRONT-165/166`) conservan el
comportamiento del helper y no forman parte de la corrida dirigida.

## Causa confirmada

El oráculo anterior buscaba `builtin:int` en el dump de
`ProjectGenericModel`. La identidad pública que el productor conserva en
`GenericSpecializationRecord::typeArguments` y en la clave de especialización
para un tipo builtin escrito en fuente es `int`. El fallo era del oráculo, no
del productor.

Además, las seis variantes compartían una sola fixture y una sola aserción.
Por ello no verificaban sus contratos declarados: el nominal no distinguía
inferencia de explicitud, la matriz no enumeraba clases, el boundary no
ejercía sus límites, determinismo no repetía ni permutaba entradas y escala no
creaba carga genérica. La fuente vacía ya dispone del oráculo de ausencia
validado en TANDA-170 y se conserva sin cambios.

## Corrección causal

La implementación queda confinada a
`FrontendGenericsFlowModule.cpp::genericInferenceOracle` y sus fixtures y
auxiliares locales:

1. El nominal materializa `Identity<int>` mediante una llamada explícita y
   otra inferida; exige identidad `int`, clon materializado, nombre estable,
   una sola especialización y reutilización de caché.
2. La matriz válida cubre argumento explícito e inferido, owner calificado,
   tipo esperado, arreglo, genérico anidado, lambda tipada y packs vacío y
   múltiple. Cada especialización requerida se inspecciona por su identidad
   pública.
3. El boundary vacío sigue delegando en
   `emptyGenericInferenceOracle`: modelo construido, cero plantillas,
   especializaciones y callables inexistentes.
4. La matriz de límites cubre const genérico mínimo y adyacente, packs de
   cardinalidad 0, 1 y 2, publica los límites configurados, acepta exactamente
   `DefaultMaximumPackExpansionCount` argumentos y rechaza el primero fuera.
5. Determinismo compila dos veces la misma entrada y compara el dump byte a
   byte; después permuta llamadas independientes y compara el conjunto
   ordenado de claves y argumentos normalizados.
6. Escala materializa un fanout de 32 tipos y una cadena de dependencias de 16
   niveles. Exige al menos 48 materializaciones, profundidad observada de 16,
   permanencia dentro de los límites públicos y la solicitud de cuatro
   workers de la variante nocturna.

## Verdad contractual y manifiesto

No se requiere modificar el manifiesto. La descripción existente de
`CF-FRONT-021` —argumentos explícitos/inferidos, receiver, tipo esperado,
lambdas, packs, identidad normalizada y límites— puede probarse mediante las
API públicas actuales de `ProjectGenericModel` y
`ProjectMethodBodyModel`. La corrección no debilita ni reinterpreta ese
contrato.

## Archivos autorizados

- `tests/compiler-suite/host/modules/FrontendGenericsFlowModule.cpp`
- `docs/compiler-testing-reengineering/phase-2/TANDA-VALIDACION-171-INFERENCIA-GENERICA-FRONTEND.md`

## Compuerta de validación

Después del commit causal se construirá únicamente el target
`nsc_component_tests` en Debug y se ejecutarán exactamente los seis IDs
inventariados, con optimización canónica host `NA` para la solicitud `O0`.
La tanda sólo podrá pasar a VALIDADA si:

- el build del target termina correctamente;
- hay 6 seleccionados, 6 resultados, 0 faltantes y 0 extras;
- los seis resultados son PASS;
- se conservan `results.json`, `summary.md`, `events.jsonl` y logs de caso.

Esta validación dirigida no equivale a la corrida integral.
