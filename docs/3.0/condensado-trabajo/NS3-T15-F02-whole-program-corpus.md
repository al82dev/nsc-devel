# NS3-T15-F02 — Corpus de programas completos / Whole-program corpus

## ES

El corpus canónico contiene seis testigos multiunidad: parser pequeño, allocator arena, job graph, serializer binario, host/plugin DLL y servidor async mínimo. Cada entrada tiene identidad, categoría única, número mínimo de unidades y resultado observable `42`; ninguna depende de un SDK amplio.

Cada programa declara perfiles Debug/Release, clean/incremental, LTO/no-LTO, sanitizers y determinismo. Parser, arena, job graph y serializer incluyen perfil `noheap` parcial. El validador rechaza categorías duplicadas, perfiles faltantes, dependencias SDK y resultados no observables, para que una lista de nombres no sustituya al contrato del corpus.

Prueba dirigida: `nsc_ns3_t15_whole_program_corpus_tests` valida catálogo, categorías, perfiles, noheap, independencia y métricas mínimas.

## EN

The canonical corpus contains six multi-unit witnesses: a small parser, arena allocator, job graph, binary serializer, DLL host/plugin, and minimal async server. Every entry has an identity, unique category, minimum unit count, and observable result `42`; none depends on a broad SDK.

Each program declares Debug/Release, clean/incremental, LTO/no-LTO, sanitizer, and determinism profiles. Parser, arena, job graph, and serializer include partial `noheap` profiles. Validation rejects duplicate categories, missing profiles, SDK dependencies, and unobservable results so a list of names cannot replace the corpus contract.

Focused test: `nsc_ns3_t15_whole_program_corpus_tests` validates catalog shape, categories, profiles, noheap coverage, independence, and minimum metrics.
