# NS304-T00-F07 — Manifest de dependencias por feature

## Estado

`CERRADO` el 2026-10-12 como contrato de planificación. El manifest no crea
dependencias ni autoriza cambios anticipados de runtime o bibliotecas.

## SCOPE LOCK

- Objetivo: publicar, por cada familia funcional 3.0.4, sus productores del
  compilador y la necesidad exacta de runtime/STL.
- Fuente: secciones 5, 6, 22, 25 y 26 del plan canónico 3.0.4.
- Revisión base: `f26e51ff4` en `development`.
- Artefacto canónico:
  `docs/3.0.4/contracts/feature-dependencies.json`.
- Protegido: producto, runtime, bibliotecas, CMake, toolchain, Builder,
  manifests de pruebas y configuración.

## Cobertura

Las 18 familias funcionales del alcance quedaron asignadas a T01–T15:

| Tanda | Familias | Runtime | STL |
|---|---:|---|---|
| T01 | 2 | guard ABI condicional sólo para static init | ninguna |
| T02 | 1 | ninguna | recompilación de consumidores |
| T03 | 1 | ninguna | Span opcional existente |
| T04 | 1 | ninguna | adopción opcional |
| T05 | 1 | protocolo mínimo requerido | Future planeado + ThreadPool existente |
| T06 | 1 | ninguna | adapters de iteración planeados |
| T07 | 1 | protocolo mínimo requerido | AsyncIterator planeado + ThreadPool existente |
| T08 | 2 | ninguna | recompilación de consumidores |
| T09 | 1 | ABI condicional | EventStorage planeado |
| T10 | 2 | ninguna | UTF-8/StringBuilder existentes donde aplica |
| T11 | 1 | ninguna | Span existente requerido |
| T12 | 1 | ninguna | adopción opcional |
| T13 | 1 | allocator ABI requerido | allocator/arena/containers existentes |
| T14 | 1 | ninguna | derives generados opcionales y planeados |
| T15 | 1 | helpers estáticos requeridos | parsing/math existentes |

## Semántica de dependencias

- `none`: la feature es enteramente compiler-side.
- `abi-conditional`: sólo se toca runtime si el diseño cerrado demuestra una
  nueva frontera física.
- `required`: la vertical no cierra sin ese contrato runtime o STL.
- `consumer-rebuild`: no exige nueva API STL, pero sí recompilar consumidores.
- `optional-adoption`: las bibliotecas pueden adoptar la feature después del
  cierre del compilador; no son su oráculo.
- `planned`: la dependencia aún no existe y no puede fingirse mediante un stub.

Las rutas con estado `existing` fueron verificadas físicamente. Los nombres con
estado `planned` no se presentan como archivos actuales ni autorizan crearlos
durante la tanda del compilador.

## Gate F07

```text
feature_families = 18/18
unique_ids = 18
owners_covered = T01..T15
unknown_compiler_components = 0
missing_existing_stl_paths = 0
planned_dependencies_claimed_existing = 0
runtime_modes_unknown = 0
stl_modes_unknown = 0
```

Siguiente acción determinista: `NS304-T00-F08`, harness 3.0.4 sin conteos
fijos, hashes de herramientas ni textos frágiles.
