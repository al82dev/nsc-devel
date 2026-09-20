# NativeSharp 2.0 self-hosting — tanda 26: escalares flotantes

## 1. Identidad

| Campo | Valor |
|---|---|
| Estado | `LANDED` |
| Gate | `SH24` |
| Rama | `development` |
| Commit de entrada | `8c1a74736` |
| Commit de cierre | este commit (`selfhost(tanda-26): lower floating scalars`) |
| Fecha | 2026-09-20 |
| Baseline | `tanda-26-sh24-floating-scalars-20260920` |

## 2. Objetivo, alcance y exclusiones

SH24 completa la familia de escalares primitivos del backend derivado de fuente con `float` y
`double`. Incluye firmas, parámetros, locales, literales, negación, suma, resta, multiplicación,
división, resto, comparaciones, asignaciones compuestas, incremento/decremento, casts y conversiones
con enteros.

La tanda no declara autoalojamiento completo. El corpus del compilador todavía requiere strings,
campos y métodos de objeto, colecciones, arrays administrados, ownership, excepciones, llamadas
indirectas y soporte de proyecto completo en el backend fuente. SH24 es una ampliación ejecutable y
acumulativa, no una promoción de `nsc-next` como compilador oficial.

## 3. Diseño, métodos e invariantes

`FloatingScalar` distingue `float`/`double`; `ScalarType` los traduce a los tipos LLVM nativos.
`EmitFloatingOperation` usa FAdd/FSub/FMul/FDiv/FRem y `FloatingComparisonPredicate` conserva
comparaciones ordenadas, salvo `!=`, que usa unordered-not-equal para que NaN sea distinto.

`ConvertScalar` materializa FPExt/FPTrunc, SIToFP/UIToFP y FPToSI/FPToUI según tipo y signedness.
La conversión floating a bool compara con cero mediante FCmp. `CastExpression` consume la
conversión contextual registrada por semántica; no vuelve a inferir el destino.

Los parámetros se copian a allocas del bloque de entrada y se registran en
`FunctionLoweringState`, lo que preserva su mutabilidad y unifica lectura/asignación con locales.
Las actualizaciones `++`/`--` eligen la operación escalar correcta. El módulo define `_fltused`,
símbolo exigido por el ABI MSVC cuando un objeto usa coma flotante y enlaza sin CRT implícito.

## 4. Archivos y ownership

| Archivo | Acción | Responsabilidad |
|---|---|---|
| `selfhost/nsc/CodeGen/LlvmBackend.ns` | modificar | floating, conversiones, casts, parámetros y `_fltused` |
| `lib/NativeSharp.LLVM.C/LLVMApi.ns` | modificar | 13 APIs LLVM floating/conversiones |
| `lib/NativeSharp.LLVM.C/LLVMEnums.ns` | modificar | predicados reales LLVM |
| `selfhost/contracts/llvm-c-api.contract.json` | modificar | hashes e inventario de 110 bindings |
| `selfhost/tests/codegen/source-floating-scalars.ns` | crear | fixture ejecutable float/double |
| contrato/verificador/CMake SH24 | crear | matriz, anatomía, hashes y ejecución |
| CMake, pruebas y documentación | modificar | registro acumulativo y trazabilidad |

## 5. Contrato y cobertura

`nativesharp-selfhost-source-floating-scalars` v1 fija scope
`floating-scalars-arithmetic-comparison-conversion`, O0–O3, LLD/NSLink y exit 42. El fixture usa
funciones float/double, parámetro mutable, división, resto, incremento/decremento, extensión float a
double y cast double a int. Su anatomía debe contener `Average`, `Offset`, `Main` y `_fltused`.

El reporte `nativesharp-selfhost-sh24-report` v1 conserva hashes del backend, objetos, anatomías,
ejecutables y ocho ejecuciones. La prueba unitaria fija las rutas floating y 110 bindings. No cambia
la CLI pública ni el ABI 2.0 de los programas existentes.

## 6. Validación dirigida

- Python selfhost: 44/44 pruebas.
- Emisión exploratoria O0, enlace LLD y ejecución: exit 42.
- SH24 dirigido: PASS O0–O3 con LLD y NSLink.
- Acumulativa SH16–SH24: PASS dentro de los 459 casos de ambos árboles CTest.

## 7. Baseline de cierre

```bat
python nscbuilder.py --baseline --jobs 16 --prefix A:\native-sharp ^
  --build-dir A:\nsc-devel\selfhost\out\nsc0-build ^
  --test-build-lld A:\nsc-devel\selfhost\out\tests-lld ^
  --test-build-nslink A:\nsc-devel\selfhost\out\tests-nslink ^
  --llvm-root C:\clang-llvm --nslink A:\native-sharp\bin\nslink.exe ^
  --history-root A:\nsc-devel\selfhost\evidence\runs ^
  --work-root A:\nsc-devel\selfhost\out\work --temp-root A:\nsc-devel\selfhost\out\temp ^
  --write-root A:\ --run-id tanda-26-sh24-floating-scalars-20260920 ^
  --build-id 2600000000000000
```

Resultado: **PASSED** en 999.96 s. Cada árbol cerró 459/459 pruebas CTest, 148/148 fixtures y
18/18 casos de triage. SH24 produjo cuatro objetos y ocho ejecutables O0–O3; todos devolvieron 42.
El digest del fixture es `0ae8c5eef78e29483d150ff0eac2677df722222049ec17f81170530c9ef20131`
y el backend certificado quedó en
`8de7dc7cc369fe33c350b404c69a23978c9094e7b60d96015afabe97a378d3e7`.
La evidencia versionada vive en `selfhost/evidence/gates/SH24/`.

## 8. Riesgos, deuda y continuidad

Las conversiones floating a entero heredan la semántica LLVM para valores fuera de rango; el gate
sólo certifica valores representables. Fast-math no se habilita en esta rebanada. `_fltused` es una
definición del objeto fuente autónomo; cuando el backend incorpore runtime completo deberá fijarse
su coalescencia con la biblioteca de runtime.

Después de la tanda 26 el compilador C++ continúa siendo válido como `nsc0` y puede seguir
evolucionando. LLVM/Clang siguen siendo dependencias de toolchain: autoalojamiento significa que el
frontend/backend de NativeSharp pueda construirse desde NativeSharp, no reimplementar LLVM.

- [x] Pruebas unitarias verdes (44/44).
- [x] SH24 pasa O0–O3 con LLD y NSLink.
- [x] Baseline completo en ambos árboles.
- [x] Evidencia SH24 versionada.
- [ ] Promoción autoalojada completa; continúa bloqueada por las familias explícitas de la sección 2.
