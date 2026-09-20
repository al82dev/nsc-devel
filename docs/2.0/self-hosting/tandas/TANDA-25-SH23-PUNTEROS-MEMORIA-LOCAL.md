# NativeSharp 2.0 self-hosting — tanda 25: punteros y memoria local

## 1. Identidad

| Campo | Valor |
|---|---|
| Estado | `LANDED` |
| Gate | `SH23` |
| Rama | `development` |
| Commit de entrada | `c8a49c34e` |
| Commit de cierre | este commit (`selfhost(tanda-25): lower pointers and local memory`) |
| Fecha | 2026-09-20 |
| Baseline | `tanda-25-sh23-source-pointers-20260920` |

## 2. Objetivo, alcance y exclusiones

SH23 incorpora punteros al backend derivado de fuente: firmas, parámetros y locales de puntero,
`null`, toma de dirección (`&`), desreferencia (`*`), indexación, asignación a memoria indexada y
`stackalloc`. Es una rebanada de memoria local verificable, no soporte general de objetos.

Quedan fuera campos y miembros, arrays administrados, aritmética de punteros, `new`/`delete`,
ownership, strings, objetos, excepciones y llamadas indirectas. Los punteros LLVM son opacos, como
exige LLVM 23; el tipo del elemento se obtiene del hecho semántico, no del handle del puntero.

## 3. Diseño, métodos e invariantes

`SupportedScalar` y `ScalarType` reconocen cualquier tipo semántico terminado en `*` y lo traducen
a `LLVMPointerTypeInContext`. `ConvertScalar` permite la identidad entre punteros opacos y la
conversión contextual de `null`, sin inventar conversiones pointer/integer.

`EmitLValue` es el camino único para obtener dirección y tipo almacenado. Resuelve locales por
símbolo, desreferencia evaluando el operando e indexación con `LLVMBuildGEP2`. La carga rvalue usa
`LLVMBuildLoad2`; la asignación generalizada usa la misma dirección y `LLVMBuildStore`. Esto evita
que identificadores, `*p` y `p[i]` tengan implementaciones de asignación divergentes.

`StackallocExpression` evalúa el tamaño, obtiene el tipo base semántico y emite
`LLVMBuildArrayAlloca`. `NullLiteralExpression` produce `LLVMConstNull` del puntero opaco. Todas las
fallas de tipo o handle cierran la emisión; ningún puntero LLVM se retiene fuera de su módulo.

## 4. Archivos y ownership

| Archivo | Acción | Responsabilidad |
|---|---|---|
| `selfhost/nsc/CodeGen/LlvmBackend.ns` | modificar | tipos puntero, lvalues, cargas y memoria local |
| `lib/NativeSharp.LLVM.C/LLVMApi.ns` | modificar | `LLVMBuildArrayAlloca` y `LLVMBuildGEP2` |
| `selfhost/contracts/llvm-c-api.contract.json` | modificar | hash e inventario de 97 bindings |
| `selfhost/tests/codegen/source-pointers.ns` | crear | fixture ejecutable de punteros |
| contrato/verificador/CMake SH23 | crear | matriz, anatomía, hashes y ejecución |
| CMake, pruebas y documentación | modificar | registro acumulativo y trazabilidad |

## 5. Contrato y cobertura

`nativesharp-selfhost-source-pointers` v1 fija scope
`pointer-null-address-dereference-index-stackalloc`, O0–O3, LLD/NSLink y exit 42. El fixture toma la
dirección de un local, lo carga por desreferencia, reserva dos enteros en stack, escribe por índice,
comprueba `null` y pasa el puntero a otra función que suma los elementos.

El reporte `nativesharp-selfhost-sh23-report` v1 conserva hashes del backend, objetos, anatomías,
ejecutables y ocho ejecuciones. La prueba unitaria fija `EmitLValue`, las cuatro formas sintácticas
y los dos bindings nuevos. No cambia CLI ni ABI pública.

## 6. Validación dirigida

- Python selfhost: 43/43 pruebas.
- Emisión exploratoria O0: objeto COFF con `Sum` y `Main` derivado de la fuente.
- SH23 dirigido: PASS O0–O3 con ambos linkers.
- Acumulativa SH16–SH23: PASS dentro de los 458 casos de ambos árboles CTest.

## 7. Baseline de cierre

```bat
python nscbuilder.py --baseline --jobs 16 --prefix A:\native-sharp ^
  --build-dir A:\nsc-devel\selfhost\out\nsc0-build ^
  --test-build-lld A:\nsc-devel\selfhost\out\tests-lld ^
  --test-build-nslink A:\nsc-devel\selfhost\out\tests-nslink ^
  --llvm-root C:\clang-llvm --nslink A:\native-sharp\bin\nslink.exe ^
  --history-root A:\nsc-devel\selfhost\evidence\runs ^
  --work-root A:\nsc-devel\selfhost\out\work --temp-root A:\nsc-devel\selfhost\out\temp ^
  --write-root A:\ --run-id tanda-25-sh23-source-pointers-20260920 ^
  --build-id 2500000000000000
```

Resultado: **PASSED** en 974.24 s. Cada árbol cerró 458/458 pruebas CTest, 148/148 fixtures y
18/18 casos de triage. SH23 produjo cuatro objetos y ocho ejecutables O0–O3; todos devolvieron 42.
El digest del fixture es `afc16788c12fa1e07e2c063a39dfd266f9ab7397155c67fc632a3db932750b98`
y el backend certificado quedó en
`43b198b2ab43896a139f7da0f085ee8adc7a33db7d409e3b1a2a7770891bdd0d`.
La evidencia versionada vive en `selfhost/evidence/gates/SH23/`.

## 8. Riesgos, deuda e higiene

El GEP certificado sólo indexa un elemento de puntero y no implementa bounds checks. El
`stackalloc` dinámico vive en el bloque actual y conserva las reglas de vida de LLVM; aún no existe
una política de escape propia del backend autoalojado. El commit excluye salidas, binarios,
temporales y los cambios ajenos de `docs/1.0.1`, `mkdown` y `Claude outputs`.

- [x] Pruebas unitarias verdes (43/43).
- [x] SH23 pasa O0–O3 con LLD y NSLink.
- [x] Baseline completo en ambos árboles.
- [x] Evidencia SH23 versionada.
