# NativeSharp 2.0 self-hosting — tanda 24: anchos enteros y conversiones

## 1. Identidad

| Campo | Valor |
|---|---|
| Estado | `LANDED` |
| Gate | `SH22` |
| Rama | `development` |
| Commit de entrada | `090f5cb93` |
| Commit de cierre | este commit (`selfhost(tanda-24): lower integer widths`) |
| Fecha | 2026-09-20 |
| Baseline | `tanda-24-sh22-source-integer-widths-20260920` |

## 2. Objetivo, alcance y exclusiones

SH22 elimina el bloqueo de anchos enteros observado en 102 usos del corpus `selfhost/nsc`. Añade
firmas, parámetros, locales, constantes y operaciones para `byte`, `char`, `short`, `ushort`,
`uint`, `long`, `ulong`, `nint` y `nuint`, además de `int`/`bool` ya certificados.

Incluye conversiones semánticas en inicializadores, operandos binarios, argumentos de llamada,
retornos y resultados compuestos. Quedan fuera literales de carácter con escapes, floats/doubles,
pointers, arrays, campos, strings, objetos, ownership y excepciones. `char` comparte el ABI i8 que
define el `TypeTable` 2.0.

## 3. Diseño, funciones e invariantes

`ScalarType(context, type)` es la tabla única: i1, i8, i16, i32, i64 o void. `IntegerBits` describe
el ancho y `UnsignedInteger` la interpretación. `ConvertScalar` compara origen/destino semánticos:
usa `LLVMBuildTrunc` al reducir, `LLVMBuildZExt` para origen unsigned/bool y `LLVMBuildSExt` para
origen signed. Conversión a bool compara contra cero.

`ApplyConversion` recorre `SemanticFact.ConversionAt`, filtra por rol e índice y materializa las
conversiones que el analizador ya decidió. No vuelve a inferir tipos en codegen. Los roles
`contextual`, `operand` y `result` cubren locales/retornos, operaciones/argumentos y narrowing de
asignación compuesta respectivamente.

`EmitIntegerOperation` recibe signedness. `/` y `%` seleccionan SDiv/SRem o UDiv/URem; `>>`
selecciona AShr o LShr. `ComparisonPredicate` hace lo mismo para relaciones. Sumas, restas,
multiplicaciones y bitwise son independientes del signo. Literales con sufijo usan el tipo de su
hecho semántico y `LLVMConstInt` del ancho exacto.

## 4. Archivos y ownership

| Archivo | Acción | Responsabilidad |
|---|---|---|
| `selfhost/nsc/CodeGen/LlvmBackend.ns` | modificar | tipos, conversiones y signedness |
| `lib/NativeSharp.LLVM.C/LLVMApi.ns` | modificar | seis APIs de conversión/unsigned |
| `selfhost/contracts/llvm-c-api.contract.json` | modificar | hash e inventario de 95 bindings |
| `selfhost/tests/codegen/source-integer-widths.ns` | crear | fixture de todos los anchos |
| contrato/verificador/CMake SH22 | crear | matriz, anatomía, hashes y ejecución |
| CMake, prueba Python, roadmap/README | modificar | registro acumulativo y trazabilidad |

Todos los LLVM refs continúan prestados. `ConvertScalar` no conserva handles ni memoria. Los strings
temporales de nombres/tipos son RAII. No hay estado global nuevo ni requisitos de concurrencia. Una
conversión no entera o un tipo fuera del scope falla cerrado.

## 5. Contrato y cobertura

`nativesharp-selfhost-source-integer-widths` v1 fija scope
`integer-widths-signedness-contextual-conversions`, O0–O3, LLD/NSLink y exit 42. El fixture prueba
narrowing de constantes, promociones signed/unsigned, resultados compuestos, UDiv, SDiv, LShr,
AShr, parámetros y retorno de anchos diferentes. Once resultados deben valer 42.

El reporte `nativesharp-selfhost-sh22-report` v1 conserva hashes del backend, objetos, anatomías,
ejecutables y ejecuciones. La prueba unitaria fija las funciones de conversión, APIs requeridas y
95 bindings. No cambia CLI ni ABI pública.

## 6. Validación dirigida

- Python selfhost: 42/42 pruebas.
- Ejecución exploratoria O0 LLD: exit 42.
- SH22 dirigido: PASS O0–O3 con ambos linkers y en ambos árboles CTest.
- Acumulativa SH16–SH22: PASS 8/8 en ambos árboles CTest.

## 7. Baseline de cierre

```bat
python nscbuilder.py --baseline --jobs 16 --prefix A:\native-sharp ^
  --build-dir A:\nsc-devel\selfhost\out\nsc0-build ^
  --test-build-lld A:\nsc-devel\selfhost\out\tests-lld ^
  --test-build-nslink A:\nsc-devel\selfhost\out\tests-nslink ^
  --llvm-root C:\clang-llvm --nslink A:\native-sharp\bin\nslink.exe ^
  --history-root A:\nsc-devel\selfhost\evidence\runs ^
  --work-root A:\nsc-devel\selfhost\out\work --temp-root A:\nsc-devel\selfhost\out\temp ^
  --write-root A:\ --run-id tanda-24-sh22-source-integer-widths-20260920 ^
  --build-id 2400000000000000
```

Resultado: **PASSED** en 976.00 s. Cada árbol cerró 457/457 pruebas CTest, 148/148 fixtures y
18/18 casos de triage. SH22 produjo cuatro objetos y ocho ejecutables O0–O3; todos devolvieron 42.
El digest fuente inicial fue `07174d7ce099e58361663671252efdc6b153613d8063ddb54380aba8d5a4a4e3`
y el backend certificado quedó en
`9241ecf36d45fdd7e45cc37e890a0010bb4d86f9241455a04d9cc033164eca0c`.
La evidencia versionada vive en `selfhost/evidence/gates/SH22/`.

## 8. Riesgos, deuda e higiene

El ABI de `char` sigue el contrato actual de un byte. Las conversiones de igual ancho son no-op;
esto conserva bits al cruzar signedness. Overflow mantiene semántica modular LLVM. El commit excluye
salidas, binarios, temporales y los cambios ajenos de `docs/1.0.1`, `mkdown` y `Claude outputs`.

- [x] Pruebas unitarias verdes (42/42).
- [x] SH22 dirigido pasa O0–O3 con ambos linkers.
- [x] Acumulativa SH16–SH22 verde en ambos árboles.
- [x] Baseline completo y evidencia SH22 versionada.
