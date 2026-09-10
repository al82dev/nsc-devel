# 1.0.1 — Clausura transitiva de plantillas reutilizables en NSMETA

## Resultado

NSC ya puede materializar desde `LibraryReference` una plantilla pública cuyo
cuerpo depende transitivamente de plantillas no públicas. El consumidor no
necesita los fuentes ni un `ProjectReference`, y las dependencias privadas no se
convierten en API nombrable.

NSMETA sube de 1.10 a 1.11. Cada plantilla conserva sus dependencias canónicas y
el productor serializa el punto fijo alcanzable desde las raíces públicas. El
verificador rechaza una clausura incompleta antes de publicar el artefacto.

## Archivos y funciones

- `include/nativesharp/Frontend/ProjectGenericModel.h`
  - accesibilidad y aristas de dependencias del template;
- `src/Frontend/ProjectGenericModel.cpp`
  - descubrimiento de tipos y llamadas genéricas;
  - visibilidad separada entre raíz pública y dependencia de materialización;
- `include/nativesharp/Interop/LibraryMetadata.h`
  - contrato NSMETA 1.11;
- `src/Interop/LibraryMetadata.cpp`
  - cálculo de clausura, codec, fingerprint y verificación transitiva;
- `src/Driver/Driver.cpp`
  - reconstrucción de templates no públicos y propietarios anidados;
- `src/Parser/Parser.cpp`, `include/nativesharp/Parser/Parser.h`,
  `src/Frontend/ProjectNameLookup.cpp`
  - partes `partial` sintéticas para propietarios anidados;
- `tests/unit/Stage44GenericsMonomorphizationTests.cpp`;
- `tests/unit/Stage46NativeLibrariesTests.cpp`;
- `tests/unit/Stage48_2LibraryMetadataTests.cpp`;
- golden `tests/fixtures/nsmeta/historical-1.11.nsmeta`.

## Decisiones de contrato

1. Sólo una raíz pública es visible y nombrable desde el consumidor.
2. Una dependencia privada alcanzable se hidrata dentro del proyecto sintético
   del productor y conserva su accesibilidad original.
3. Una dependencia privada no alcanzable no se serializa.
4. Las aristas usan identidad canónica de módulo, namespace, propietario,
   nombre, aridad y firma; no sólo el nombre simple.
5. Una arista hacia otra NSMETA se valida en el módulo propietario, no se exige
   erróneamente dentro del archivo de la fachada.
6. Los escritores históricos hasta 1.10 conservan su semántica y no emiten
   entradas privadas sin las aristas introducidas en 1.11.
7. Cero candidatos y ambigüedad real producen diagnósticos distintos.

## Evidencia dirigida

- Stage 44 generics: 77/77;
- Stage 46 native libraries: 433/433 con `lld-link`;
- Stage 48.2 metadata: 176/176;
- Stage 50.9 metadata/host: 56/56;
- Stage 52 generic collections: 147/147;
- Stage 62 toolchain contract: 44/44;
- Stage 66 release contract: 77/77.

El corpus real de Foundation se copió únicamente a una carpeta temporal dentro
del worktree y se regeneró en O0 con el compilador local. Su DLL, import library
y NSMETA verificaron 1,463 símbolos. Un consumidor basado exclusivamente en
`LibraryReference` compiló HIR, enlazó y terminó con 42 usando:

- `List<int>.Sort()`;
- `List<Vector3>`;
- `List<TestComponent>` con el tipo declarado sólo por el consumidor;
- `SortedDictionary<float, Vector3>`.

También existe una regresión interna O0/O2 para
`biblioteca A -> template público de B -> helper privado de B`, cerrada con un
tipo declarado únicamente por el consumidor.

No se ejecutó el baseline global por instrucción del usuario: se reserva para
la integración en `development`. No se instaló nada en `C:\native-sharp`.

## Commits

- `bf9a7f22` — conserva dependencias genéricas privadas;
- `be41fe5c` — serializa la clausura transitiva y NSMETA 1.11;
- `b930f705` — materializa propietarios `static` anidados;
- `9c5b54fa` — distingue ausencia y ambigüedad genérica;
- `55b60251` — certifica clausura genérica entre bibliotecas;
- `d00c8dbe` — corrige la expectativa residual de NSMETA 1.11.

## English summary

NSMETA 1.11 persists the canonical fixed-point dependency graph of reusable
public templates. Reachable private templates remain private but are available
inside the producer's synthetic materialization project. Missing same-module
edges fail producer/artifact verification with the public root, required
template, owner, and dependency chain. Directed compiler suites and a read-only
Foundation corpus validate source-free O0 consumption; the global baseline is
deferred to the `development` integration pass.
