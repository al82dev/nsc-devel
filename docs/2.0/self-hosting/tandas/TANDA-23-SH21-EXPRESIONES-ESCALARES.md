# NativeSharp 2.0 self-hosting — tanda 23: expresiones escalares completas

## 1. Identidad

| Campo | Valor |
|---|---|
| Estado | `LANDED` |
| Gate | `SH21` |
| Rama | `development` |
| Commit de entrada | `4ab798878` |
| Commit de cierre | este commit (`selfhost(tanda-23): lower scalar expressions`) |
| Fecha | 2026-09-20 |
| Baseline | `tanda-23-sh21-source-expressions-20260920` |

## 2. Objetivo, alcance y exclusiones

SH21 completa las expresiones que operan sobre los escalares bootstrap `int` y `bool`: `%`, `&`,
`|`, `^`, `<<`, `>>`, sus asignaciones compuestas, `+=`, `-=`, `*=`, `/=`, incremento/decremento
prefijo y postfijo, `&&`, `||` y `?:`. La meta no es sólo aceptar sintaxis: cada expresión debe
producir LLVM válido, preservar el valor observable y respetar evaluación condicional.

Quedan fuera conversiones entre anchos, unsigned shifts, floats, pointers, indexadores, campos,
propiedades, objetos, strings, ownership y excepciones. Los lvalues actualizables de esta tanda son
exclusivamente locales `int` identificados por símbolo semántico.

## 3. Diseño, métodos e invariantes

`EmitIntegerOperation(builder, operation, left, right, name)` centraliza la correspondencia de
operadores simples y compuestos con LLVM-C. La asignación compuesta carga una sola vez el slot,
calcula, almacena y devuelve el resultado. Los nodos `UnaryExpression`/`PostfixExpression` de
incremento o decremento cargan y almacenan exactamente una vez; el prefijo devuelve el resultado y
el postfijo el valor previo.

`EmitScalarExpression` reconoce `&&`/`||` antes del camino binario ávido. Evalúa el lado izquierdo,
crea `logic.rhs` y `logic.end`, ramifica según el operador y fusiona el constante cortocircuitado con
el valor derecho mediante `LLVMBuildPhi`/`LLVMAddIncoming`. Los bloques entrantes se toman después
de bajar cada subexpresión, por lo que expresiones lógicas anidadas conservan el predecesor real.

El condicional crea ramas `select.then`/`select.else`, baja sólo la seleccionada en ejecución y
fusiona valores `i1` o `i32` en `select.end`. Todo bloque queda terminado antes de cambiar el punto
de inserción. Las APIs añadidas pertenecen al inventario LLVM-C ya auditado de nsc0.

## 4. Archivos y responsabilidades

| Archivo | Acción | Responsabilidad |
|---|---|---|
| `selfhost/nsc/CodeGen/LlvmBackend.ns` | modificar | lowering de operaciones, updates, short-circuit y `?:` |
| `lib/NativeSharp.LLVM.C/LLVMApi.ns` | modificar | exponer siete APIs LLVM-C auditadas |
| `selfhost/contracts/llvm-c-api.contract.json` | modificar | fijar hash e inventario de 89 firmas |
| `selfhost/tests/codegen/source-expressions.ns` | crear | fixture observable con efectos laterales evitados |
| contrato/verificador/CMake SH21 | crear | matriz, símbolos, hashes y ejecución |
| CMake, prueba Python, roadmap/README | modificar | registrar gate y regresión estructural |

Los handles LLVM y bloques son préstamos no propietarios. Los arrays de dos entradas usados por
`LLVMAddIncoming` viven en el stack durante la llamada. No se introduce memoria compartida ni
concurrencia. Un operador o lvalue fuera del alcance falla cerrado al devolver `null`, y el driver
mantiene su código de error de backend.

## 5. Contrato y pruebas

`nativesharp-selfhost-source-expressions` v1 fija el scope
`compound-increment-bitwise-short-circuit-conditional`, O0–O3, LLD/NSLink y exit 42. El fixture
aplica todos los operadores compuestos, verifica valores prefijo/postfijo y coloca asignaciones a
90/91 en lados que deben omitirse. Si `&&` o `||` fueran lowered con evaluación ávida, el resultado
dejaría de ser 42.

El reporte `nativesharp-selfhost-sh21-report` v1 conserva hashes del backend, cuatro objetos,
anatomía COFF, ocho ejecutables y cada ejecución. La prueba unitaria fija las decisiones de diseño y
el inventario de 89 bindings. La CLI y ABI externas no cambian.

## 6. Validación dirigida

- Python selfhost: 41/41 pruebas.
- SH21 dirigido: PASS O0–O3 y LLD/NSLink en ambos árboles CTest.
- Acumulativa SH16–SH21: PASS 7/7 en ambos árboles CTest.

## 7. Baseline de cierre

```bat
python nscbuilder.py --baseline --jobs 16 --prefix A:\native-sharp ^
  --build-dir A:\nsc-devel\selfhost\out\nsc0-build ^
  --test-build-lld A:\nsc-devel\selfhost\out\tests-lld ^
  --test-build-nslink A:\nsc-devel\selfhost\out\tests-nslink ^
  --llvm-root C:\clang-llvm --nslink A:\native-sharp\bin\nslink.exe ^
  --history-root A:\nsc-devel\selfhost\evidence\runs ^
  --work-root A:\nsc-devel\selfhost\out\work --temp-root A:\nsc-devel\selfhost\out\temp ^
  --write-root A:\ --run-id tanda-23-sh21-source-expressions-20260920 ^
  --build-id 2300000000000000
```

Resultado: **PASSED** en 927.20 s. Cada árbol cerró 456/456 pruebas CTest, 148/148 fixtures y
18/18 casos de triage. SH21 produjo cuatro objetos y ocho ejecutables O0–O3; todos devolvieron 42.
El digest fuente inicial fue `6fccb703fa8db406a1e68bd242e09e4823b26980cf0003f4dd12eabac3c04019`
y el backend certificado quedó en
`03d7a702ab06ff4fa18d936c7fe46507e6f2b1e340474c76e75d8b57b57e1b07`.
La evidencia versionada vive en `selfhost/evidence/gates/SH21/`.

## 8. Riesgos, deuda e higiene

El desplazamiento derecho es aritmético porque el único entero cubierto es `int`; los tipos unsigned
se certificarán por separado. Los increments sólo aceptan storage local i32. El commit excluye
salidas, binarios, temporales y los cambios ajenos de `docs/1.0.1`, `mkdown` y `Claude outputs`.

- [x] Pruebas unitarias verdes (41/41).
- [x] SH21 dirigido pasa O0–O3 con ambos linkers.
- [x] Acumulativa SH16–SH21 verde en ambos árboles.
- [x] Baseline completo y evidencia SH21 versionada.
