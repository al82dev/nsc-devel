# NS3-T06-F06 — Pinning y direcciones estables / Pinning and stable addresses

## Resultado

NativeSharp 3.0 tiene una vertical productiva completa de pinning: owners `pin raii`, proyecciones
`pin ref`, construcción placement autorreferencial, destrucción object-only y transporte explícito
de la garantía hasta MIR/caché. No se añadió runtime ni ABI.

## Cambios principales

- El parser reconoce `pin ref T name = ref place` sin convertir `pin` en keyword global.
- El frontend exige un place estático y conserva borrow exclusivity/lifetime.
- `pin raii` admite placement como inicializador owner válido.
- El cleanup de un objeto placement usa la entrada object-only del descriptor y nunca libera los
  bytes prestados.
- HIR, MIR, dumps e identidad incremental transportan `pinned=true`.
- La frontera de containers falla cerrada para índices/proyecciones dinámicos.
- Closures no pueden capturar una referencia pinned; T09/T10 podrán consumir el bit ya publicado.

## Archivos nuevos

- `tests/unit/NativeSharp3T06PinningTests.cpp`: prueba dirigida O0/NSLink de ejecución, dumps y
  diagnósticos.
- `docs/3.0/language/pinning-and-stable-addresses.md`: contrato normativo bilingüe.
- Este documento: mapa y evidencia del commit.

## Archivos modificados

- Parser/frontend: `src/Parser/Parser.cpp`, `ProjectMethodBodyModel.cpp` y modelos/dumpers HIR/MIR.
- Backend/caché: `src/LLVM/ProjectOopLlvmLowerer.cpp` y
  `src/LLVM/ProjectOopMirCacheIdentity.cpp`.
- Contratos públicos HIR/MIR bajo `include/nativesharp/Frontend`.
- Registro dirigido: `CMakeLists.txt`.
- Roadmap, tanda 06 y tracking maestro.

## Mejora observable

Un objeto puede construirse sobre storage del caller, referirse a su propia dirección y terminar
su lifetime sin liberar el buffer. Una proyección pinned mantiene dirección comprobable y no puede
disfrazar un elemento relocatable de contenedor como estable.

## Validación dirigida

- `nsc.ns3.t06.pinning`: 8/8 checks en O0 con NSLink.
- Ejecución de placement autorreferencial con resultado 42 y destructor exactamente una vez.
- Method model y MIR conservan owner/proyección pinned y ownership `owned/borrowed` del placement.
- Rechazos de edición 2, índice dinámico, move, replacement y captura closure.
- Baseline, suites y matrices: no ejecutados por prohibición expresa del proyecto.
