# Milestone — baseline previo a optimización de NSC 2.0

## Propósito

Este milestone congela el punto de partida inmediatamente anterior al programa
`NS2-PERF`. No introduce cambios de compilador, lenguaje, ABI, runtime ni linker.
Su única función es dejar una referencia reproducible contra la cual medir las
optimizaciones de `-O0`, `-O1`, `-O2` y `-O3`.

## Revisión y entorno

```text
Branch: development
Base commit: e7298886177d2c35047d01f0aa01f949598fdd79
Run: 20260917-185251-47e135fd
Compiler: nsc 2.0.0+build.20260918T005255Z.bd5e546879e409cd
Compiler SHA-256: d418b43be2988adad1f4c856b9590e9b58d75a1f36f22b1505f7d00343a10ebf
NSLink SHA-256: 3804675135132ef82280ba53435ca80ff44a282779eb224866d05a98d8eea39a
Source digest S0/S1: 8f56f682086852801b326f11ada8c20e3a7ba49be4ebc7f6316f78051e81d3e7
Source files S0/S1: 1323/1323
Installed prefix: C:\native-sharp
LLVM root: C:\clang-llvm
```

## Resultado certificado

```text
Build: passed
Installation verification: passed
lld-link CTest: 435/435 passed
NSLink CTest: 435/435 passed
lld-link fixtures: 148/148 passed (O0/O2)
NSLink fixtures: 148/148 passed (O0/O2)
lld-link bug triage: 18/18 passed (O0/O2)
NSLink bug triage: 18/18 passed (O0/O2)
Failures: 0
Gate 0: LANDED
Elapsed: 601.373 s
```

La evidencia canónica está en `_abi/gates/G00/gate.json` y el reporte detallado
en `_nscbuilder/history/20260917-185251-47e135fd/report.md`.

## Límite conocido del baseline heredado

La matriz histórica de fixtures y bug triage cubre `-O0` y `-O2`. El backend ya
implementa `-O1` y `-O3`, y CTest verifica de forma dirigida el pipeline O3, pero
este milestone no afirma todavía cobertura integral de esos dos niveles. El
programa `NS2-PERF` ampliará la matriz a O0/O1/O2/O3 y medirá por separado
latencia de compilación, memoria, tamaño y rendimiento del código producido.

---

# Milestone — pre-optimization NSC 2.0 baseline

This milestone freezes the certified starting point for `NS2-PERF`. It changes
no compiler, language, ABI, runtime, or linker behavior. The complete inherited
baseline passed with both linkers and Gate 0 is `LANDED`. Its fixture and bug
triage matrix covers O0/O2; the optimization program will expand that evidence
to O0/O1/O2/O3 and will measure compiler latency, memory, artifact size, and
generated-code performance independently.

## Commit message

`Registra baseline previo a optimizacion / Record pre-optimization baseline`
