# Tanda operativa 09 — T03 variants y pattern matching

## Objetivo

Convertir T03 de un contrato C++ aislado en una primera vertical NativeSharp 3.0 realmente
ejecutable, sin inventar un segundo backend y sin aceptar ownership/drop que el compilador todavía
no puede garantizar. La entrega debía llegar desde sintaxis `.ns` hasta PE Windows x64, conservar
layout in-place, cruzar genéricos y NSMETA y demostrar que un payload inactivo no queda expuesto
como una lectura ordinaria.

## Situación encontrada

- `SumTypeSemantics` calculaba layout y cobertura en C++, pero parser/AST/frontend/backend no lo
  consumían.
- No existían nodos `VariantDeclaration`, `MatchExpression` ni patterns productivos.
- El pipeline sí tenía rutas maduras para structs, unions anónimas/anidadas, métodos, genéricos,
  layout Win64, LibraryReference, HIR/MIR/LLVM y host headers.
- Una unión raw ya rechazaba storage owning, que era el límite seguro necesario mientras T06 no
  tenga drop flags por alternativa activa.
- Un prototipo escrito con las formas 2.x existentes demostró que una unión anidada no genérica
  puede usar los parámetros genéricos de su variant contenedor y llegar correctamente hasta PE.

## Diseño aplicado

### Representación del variant

`variant Name<T> { Empty, Value(T) }` se materializa en el parser como un struct ordinario con:

1. un tag privado `byte`/`ushort`/`uint` de ancho mínimo;
2. una unión anidada que contiene todos los payloads en offset cero;
3. un único carrier privado para esa unión;
4. una fábrica pública y estática por caso;
5. un test readonly `IsCase()` por caso;
6. un `CaseTag()` readonly;
7. un extractor reservado por caso con payload.

El carrier se llama `__nsc_variant_payload_<VariantName>`. No usa file-id ni source offset, de modo
que mover la declaración no renombra una identidad física publicada en NSMETA/header.

No se añadió un backend de variants. El AST generado usa exclusivamente `StructDeclaration`,
`FieldDeclaration`, `MethodDeclaration`, locals, assignments, returns y la representación de unión
que ya consumen name lookup, generics, layout, HIR, MIR y LLVM.

### Acceso al payload activo

El extractor debe ser público para que un match en un consumidor `LibraryReference` pueda resolver
el método transportado por NSMETA. Eso no lo convierte en API de usuario:

- el método lleva el prefijo reservado `__nsc_variant_extract_`;
- el `CallExpression` creado por `match` lleva `variant-pattern-extract`;
- `ProjectMethodBodyModel::analyzeCall` rechaza toda invocación a ese prefijo sin el marcador.

Así, el usuario puede consultar el tag o `IsCase()`, pero no leer la unión inactiva saltándose el
pattern.

### Lowering de match

El `match` statement:

- evalúa el governing expression una vez en un local oculto const;
- prueba casos en orden mediante `IsCase()`;
- extrae el payload sólo dentro de la rama cuyo tag ya fue comprobado;
- coloca el binding const en el mismo scope que las sentencias del brazo;
- baja a un árbol `if/else` ordinario, sin heap, boxing ni runtime dispatcher;
- exige un wildcard final mientras el catálogo cerrado no viaje de forma uniforme por toda la
  metadata concreta;
- diagnostica duplicados, wildcard repetido y brazos inalcanzables después de wildcard.

Los guards se consumen para recuperación pero se rechazan causalmente; no se finge una
continuación de decision tree que todavía no existe.

## Cambios por archivo

### Modificados

- `include/nativesharp/Parser/Parser.h`
  - declara parser de variant y match;
  - añade contador determinista de temporales de match.
- `src/Parser/Parser.cpp`
  - reconoce `variant` top-level y anidado como contextual de edition 3;
  - valida modifiers/layout incompatibles;
  - parsea casos unit/payload, genéricos y constraints;
  - genera tag, unión, carrier, fábricas, tests y extractores;
  - parsea y baja `match` statement con evaluación única y scopes de binding;
  - emite diagnósticos precisos para límites no implementados.
- `src/Frontend/ProjectMethodBodyModel.cpp`
  - impide que source normal invoque el extractor reservado del payload;
  - conserva la llamada generada y marcada por match, incluida la ruta precompiled.
- `tests/unit/Cc1CorrectnessTests.cpp`
  - registra `c59_ns3_variants` para el baseline final que ejecutará Alberto.
- `docs/3.0/TANDA-03-TIPOS-SUMA-Y-PATTERN-MATCHING.md`
  - reemplaza la afirmación histórica de helper aislado por el estado real, evidencia y deuda.

### Nuevos

- `tests/cc1/c59_ns3_variants/c59_ns3_variants.nsproj`
  - proyecto edition 3 mínimo para la vertical end-to-end.
- `tests/cc1/c59_ns3_variants/Main.ns`
  - cubre unit cases, generic cases, nested variant, tuple payload, alineación 8, nested match,
    wildcard-only y evaluación única; devuelve 42.
- `docs/3.0/TANDA-OPERATIVA-09-T03-VARIANTS-PATTERN-MATCHING.md`
  - evidencia y handoff de este commit.

No se creó código fuente fuera de `A:\nsc-devel`. Los probes y artefactos temporales se eliminaron
antes del commit.

## Mejora concreta

- T03 deja de ser un helper desconectado y compila programas `.ns` reales.
- El layout es compacto y auditable: tag mínimo + máximo payload una sola vez.
- Los variants genéricos reutilizan el modelo de specialization existente.
- Match no reevalúa side effects y no introduce allocation oculta.
- La lectura del payload inactivo queda cerrada semánticamente.
- Un variant concreto cruza LibraryReference y un witness `export extern C` conserva asserts de ABI.
- Los límites peligrosos fallan con diagnóstico en vez de compilar con semántica incompleta.

## Validación dirigida ejecutada

1. Construcción únicamente de `nsc`, configuración Debug/O0, target `nsc`: correcta.
2. `c59_ns3_variants` con `--emit-project-oop-exe -O0`: correcta; proceso termina en 42.
3. Dump dirigido de layouts:
   - `Result<int,int>`: size 8, align 4, tag byte en offset 0, payload en offset 4;
   - `Signal`: size 8, align 4;
   - `Packed`: size 12, align 4;
   - `Wide`: size 16, align 8, payload union en offset 8.
4. Dump MIR dirigido: llamadas de tests de caso y ramas ordinarias visibles; sin dispatcher heap.
5. Producer `VariantApi.lib + VariantApi.nsmeta` y consumidor LibraryReference concreto: correctos;
   NSMETA 1.27 conserva `Zulu=0`, `Alpha=1` aunque el orden alfabético sea el contrario, y el
   consumidor termina en 42. La importación nominal genérica sigue en el frente general de
   genéricos precompilados.
6. Witness C no genérico usado por `EchoStatus(Status)`:
   - host header emite `uint8_t __nsc_variant_tag`;
   - emite una `union` real con ambos payloads en offset 0;
   - `NS_Status` mide 16, align 8 y payload offset 8;
   - metadata read-back válida.
7. Probes negativos, todos con exit distinto de cero:
   - edition 2: `NS3002`;
   - caso duplicado;
   - match sin wildcard seguro;
   - llamada directa al extractor reservado.

No se ejecutó baseline, CTest general, O1/O2/O3, instalación, deploy ni operación remota.

## Límites y dependencias explícitas

- Ownership payload, active drop, unwind y destrucción exactamente una vez: tandas 13–14.
- Patterns literal/range/record/or, binding ref/move, `if let`, `while let`, `let else`, guards y
  closed-domain usefulness sin wildcard: extensión T03 aún pendiente.
- El catálogo concreto ya cruza NSMETA 1.27, fingerprint ABI, API diff y reporte ABI. La
  importación nominal genérica desde `LibraryReference` sigue pendiente en su frente general.
- Visualización del caso activo y payload válido en debugger/LSP: tanda 29.
- Matriz combinatoria, fuzz y comparación de costos: tanda 30.

Por estas dependencias, la tanda operativa 09 queda cerrada como vertical ABI-segura; el gate T03
maestro permanece `IMPLEMENTING` hasta que las tandas dependientes completen ownership y tooling.
