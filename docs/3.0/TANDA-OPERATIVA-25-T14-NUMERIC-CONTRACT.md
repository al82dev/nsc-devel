# Tanda operativa 25 — T14/E003–E007: contrato numérico verificable

Fecha: 2026-09-29

Rama: `development`

Alcance: contrato numérico del compilador y frontera honesta de certificación LinearAlgebra

Estado de T14: `IMPLEMENTING`

Estado de E003–E007: `EXTERNAL-CERTIFICATION-REQUIRED`

## Objetivo

Hacer verificable el contrato numérico que realmente selecciona el driver y eliminar una prueba
tautológica del helper T14. El artefacto debe cambiar cuando cambian FP mode, FMA, rounding o
denormals y debe negarse a presentar E003–E007 como certificadas sin los oráculos externos de la
biblioteca.

Los comandos nuevos son:

```text
nsc --print-numeric-contract
nsc --write-numeric-contract <file.contract>
nsc --verify-numeric-contract <file.contract>
```

Cada build de `nsc` genera además el perfil preciso por omisión:

```text
build-clang/generated/NativeSharpNumeric.contract
```

## Defecto corregido

`constevalMatchesRuntime(...)` devolvía el resultado de comparar `addInteger(...)` consigo mismo.
Por definición siempre era verdadero, incluso si todas las políticas de overflow estaban rotas.

Se reemplazó por `integerSemanticsSelfCheck()`, que recorre independientemente las 65,536 parejas
`int8` y compara checked, wrapping, unchecked y saturating contra resultados de referencia calculados
en un dominio ancho. El contrato sólo publica `integer-self-check=pass` si ese recorrido completo
termina verde.

## Contrato `NSNUMERIC 1.0`

El manifiesto fija y fingerprinta:

- target único Windows x64;
- representación two's complement;
- overflow entero por omisión wrapping y prohibición de flags LLVM no-wrap;
- resultado de la verificación exhaustiva independiente;
- FP mode `precise`, `fast` o `deterministic`;
- política FMA;
- rounding;
- denormal mode;
- libertad de reassociation y preservación de NaN/signed zero;
- estado explícito `linear-algebra-e003-e007=external-certification-required`.

El estado FP no se duplica en una configuración paralela: se captura mediante
`llvm_backend::captureFloatingPointState()` después de que el driver procesa las opciones reales.

## Archivos modificados

- `include/nativesharp/Language/NumericSemantics.h`: reemplaza la API tautológica por self-check.
- `src/Language/NumericSemantics.cpp`: referencia exhaustiva `int8` independiente.
- `src/Driver/Driver.cpp`: comandos CLI y ayuda.
- `tests/unit/NativeSharp3T14NumericTests.cpp`: exige el self-check real.
- `tests/unit/NativeSharp3T15IntegrationTests.cpp`: consume la nueva garantía.
- `CMakeLists.txt`: integra código/test, genera el contrato después de enlazar `nsc` y lo añade al
  payload de una instalación futura.
- `docs/3.0/TANDA-14-SEMANTICA-NUMERICA-Y-CONSTANTES.md`: registra integración y deuda.

## Archivos nuevos y por qué

- `include/nativesharp/Driver/NumericContract.h`: API del contrato sobre el estado FP real.
- `src/Driver/NumericContract.cpp`: manifest, fingerprint SHA-256, escritura y verificación exacta.
- `tests/unit/NativeSharp3T14NumericContractTests.cpp`: nueve comprobaciones de perfiles,
  fingerprint, round-trip y rechazo causal.
- `docs/3.0/TANDA-OPERATIVA-25-T14-NUMERIC-CONTRACT.md`: expediente completo de la tanda.

## Validación dirigida ejecutada

1. `nsc` Debug/O0 construido y generó `NativeSharpNumeric.contract`.
2. T14 histórico: `PASS: 7 checks`.
3. Contrato numérico nuevo: `PASS: 9 checks`.
4. T15 afectado por el cambio de API: `PASS: 11 checks`.
5. La CLI verificó el contrato generado bajo el perfil preciso por omisión.
6. `--print-numeric-contract --fp-mode=fast` publicó FMA fast, reassociation allowed y un
   fingerprint diferente.
7. Verificar el contrato preciso bajo el perfil fast fue rechazado con exit code 1 y fingerprint
   esperado.
8. La carpeta temporal CLI y la carpeta temporal del test fueron eliminadas de forma dirigida.
9. `NSC_BUILD_TESTS` quedó restaurado a `OFF` y `nsc` se reconstruyó.

No se ejecutó baseline, CTest general, O1/O2/O3, LinearAlgebra, libraries, instalación, deploy,
debugger ni remotas.

## Mejora concreta

Una biblioteca numérica puede registrar exactamente qué semántica FP e integer usó el compilador y
rechazar comparaciones hechas con otro perfil. Además, un bug en las cuatro políticas enteras ya no
puede ocultarse detrás de una prueba que siempre devolvía verdadero.

## Deuda explícita

- conectar literal typing, conversiones lossless, strong numeric types y políticas checked/
  saturating a parser, HIR, MIR y LLVM productivos;
- probar equivalencia consteval/runtime con ejecución real, no sólo el helper de referencia;
- certificar E003–E007 con oráculos independientes, matrices condicionadas, tolerancias, NaN/Inf,
  singularidad, corpus hostil y benchmarks;
- verificar publicación `.lib`/`.nsmeta` y consumo externo de LinearAlgebra;
- ejecutar la matriz FP/O0–O3 y ambos linkers únicamente en el gate final autorizado.

Por estas deudas T14 sigue `IMPLEMENTING` y E003–E007 no cambian a verde. La tanda 25 cierra el
contrato verificable del compilador, no la certificación de LinearAlgebra.
