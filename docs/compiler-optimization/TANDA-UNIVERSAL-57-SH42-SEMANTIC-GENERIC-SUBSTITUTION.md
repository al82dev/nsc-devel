# Tanda Universal 57: cerrar el tipo genérico en Sema

## SCOPE LOCK

| Dimensión | Alcance autorizado |
|---|---|
| Objetivo | Hacer que `owned T` y `borrowed T` se sustituyan al seleccionar un miembro de un receptor genérico cerrado. |
| Causa demostrada | `SubstituteMemberType` sólo reconocía el texto exacto `T`; dejaba `List<bool>.this` y `List<TokenKind>.this` como `owned T`. |
| Archivos | `SemanticAnalyzer.ns` y retiro de la reconstrucción duplicada en `LlvmBackend.ns`. |
| Protegido | Pruebas, fixtures, infraestructura, clasificadores globales y contratos de ownership de referencias. |
| Terminación | SH41/181 pasa; `KindAt` tiene una carga; `visited[id]` carga `i1`; nsc0/nsc2 coinciden en SH42/182. |

## Diagnóstico causal

La Tanda 56 demostró que el tipo LLVM del resultado no distingue ambos casos: tanto
`List<bool>.get_Item` como `List<TokenKind>.get_Item` cruzan la ABI genérica mediante un
puntero. El fallo ocurre antes, en Sema:

1. el miembro declara `owned T`;
2. `SubstituteMemberType` comparaba ese texto completo contra el parámetro `T`;
3. la comparación fallaba y el `SemanticFact` quedaba abierto;
4. `KindAt` recibía después una conversión contextual en su `return`, pero `visited[id]`
   usado como condición no la recibía;
5. reconstruir el tipo otra vez en el backend introdujo dos cargas en `KindAt`.

## Corrección mínima

La sustitución reconoce un calificador directo `owned` o `borrowed` alrededor del parámetro.
Si el argumento cerrado es un valor nativo —builtin, puntero, struct, union o enum— el
calificador se elimina en esa frontera, conforme al contrato documentado de `List<T>`:
ownership es un no-op para valores. Si el argumento es una referencia, el calificador se
conserva.

El backend vuelve a consumir exclusivamente el tipo ya cerrado por Sema. Se retira la
reconstrucción añadida en las Tandas 55/56; no se revierte ningún commit ni se altera la
conversión genérica compartida.

## Complejidad y seguridad

- Tiempo: `O(p + s)`, donde `p` es el número de parámetros genéricos del propietario y `s`
  la búsqueda nominal ya existente para un argumento Named.
- Memoria temporal: `O(n)` para los textos cortos del tipo sustituido.
- No hay recursión nueva.
- Las referencias conservan `owned`/`borrowed`; sólo los tipos de valor pierden un
  calificador que no tiene representación física.

## Validación dirigida

Después del commit: sólo SH41/181 O0 con NSLink, comparación directa de `--dump-symbols` y
CTest 182. Sin baseline ni batería completa.
