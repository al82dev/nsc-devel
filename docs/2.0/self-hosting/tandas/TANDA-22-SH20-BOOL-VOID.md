# NativeSharp 2.0 self-hosting — tanda 22: escalares bool y void

## 1. Identidad

| Campo | Valor |
|---|---|
| Estado | `LANDED` |
| Gate | `SH20` |
| Rama | `development` |
| Commit de entrada | `af0f4d45e` |
| Commit de cierre | este commit (`selfhost(tanda-22): lower bool and void scalars`) |
| Fecha | 2026-09-20 |
| Baseline | `tanda-22-sh20-source-bool-void-20260920` |

## 2. Objetivo, alcance y exclusiones

SH20 amplía la rebanada fuente de `int` a `bool` y `void`: declara firmas heterogéneas de hasta
cuatro parámetros `int`/`bool`, baja parámetros y locales booleanos, literales `true`/`false`, `!`,
comparaciones como valores, retornos `void` y llamadas a funciones sin resultado. El fixture usa
una función booleana, una observadora `void` y `Main` para producir 42.

La selección responde al corpus: después de `int`, las fuentes de `nsc-next` contienen 247 usos de
`void` y 211 de `bool`. Quedan fuera short-circuit `&&`/`||`, otros anchos enteros, floats, chars,
pointers, strings, overloads, métodos de instancia, objetos, ownership y excepciones.

## 3. Diseño e invariantes

`ScalarType` es la única tabla bootstrap de `int -> i32`, `bool -> i1` y `void -> void`.
`ScalarFunctionType` recorre parámetros sintácticos en orden y crea una firma exacta; rechaza tipos
fuera del scope. `DeclareFunctions` conserva la fase de prototipos antes de emitir cuerpos.

Cada `LocalBinding` conserva storage y tipo LLVM no propietarios. `EmitScalarExpression` usa ese
tipo en `LLVMBuildLoad2`, materializa booleanos i1, emite `LLVMBuildNot` y convierte comparaciones
en i1. Las llamadas obtienen la firma declarada mediante `LLVMGlobalGetValueType`, evitando
reconstruir incorrectamente todas como `(i32...) -> i32`. Un `return;` sólo es válido en función
`void` y produce `LLVMBuildRetVoid`. `EmitCondition` devuelve directamente un i1 semántico; sólo
compara contra cero cuando el valor es entero.

## 4. Archivos, APIs y ownership

| Archivo | Acción | Responsabilidad |
|---|---|---|
| `selfhost/nsc/CodeGen/LlvmBackend.ns` | modificar | tipos escalares, bool y void |
| `lib/NativeSharp.LLVM.C/LLVMApi.ns` | modificar | exponer tipo de global y not |
| `selfhost/contracts/llvm-c-api.contract.json` | modificar | fijar hash de 82 firmas |
| `selfhost/tests/codegen/source-bool-void.ns` | crear | fixture observable SH20 |
| contrato/verificador/CMake SH20 | crear | matriz, anatomía, hashes y ejecución |
| CMake, prueba Python, roadmap/README | modificar | gate acumulativo y trazabilidad |

`LocalBinding(int,void*,void*)` no adquiere handles LLVM. `FindLocalType` recorre de atrás hacia
delante igual que `FindLocal`; la lista continúa siendo propiedad de `FunctionLoweringState`.
`ScalarFunctionType` devuelve un tipo poseído por el contexto LLVM. Ninguna API nueva sale del
inventario ya auditado de nsc0; se actualiza el hash del archivo de firmas.

## 5. Contrato, pruebas y divergencias

El contrato `nativesharp-selfhost-source-bool-void` v1 y el reporte
`nativesharp-selfhost-sh20-report` v1 fijan scope `bool-void-functions-locals-calls`. La CLI y ABI
externa no cambian. El verificador exige los tres símbolos fuente, cuatro objetos, ocho ejecutables,
exit 42 y presencia de las APIs SH20 en el backend.

La primera corrida unitaria detectó que la aserción histórica SH16 aún buscaba el nombre
`EmitI32Expression`; se actualizó a `EmitScalarExpression`, conservando la misma exigencia de que el
backend dependa del AST. La prueba unitaria fija además el inventario del binding en 82 y verifica
las decisiones estructurales. La acumulativa reveló la misma referencia en el verificador SH16;
se actualizó el marcador, tras comprobar que SH17–SH20 ya pasaban y sin relajar símbolos, hashes ni
resultados ejecutables de SH16. La repetición acumulativa cerró 6/6 en ambos árboles.
CTest construye nsc-next en O0–O3 y enlaza con LLD/NSLink. Toda divergencia dirigida se registrará
antes del baseline; no se aceptan objetos residuales ni perfiles omitidos.

## 6. Baseline de cierre

```bat
python nscbuilder.py --baseline --jobs 16 --prefix A:\native-sharp ^
  --build-dir A:\nsc-devel\selfhost\out\nsc0-build ^
  --test-build-lld A:\nsc-devel\selfhost\out\tests-lld ^
  --test-build-nslink A:\nsc-devel\selfhost\out\tests-nslink ^
  --llvm-root C:\clang-llvm --nslink A:\native-sharp\bin\nslink.exe ^
  --history-root A:\nsc-devel\selfhost\evidence\runs ^
  --work-root A:\nsc-devel\selfhost\out\work --temp-root A:\nsc-devel\selfhost\out\temp ^
  --write-root A:\ --run-id tanda-22-sh20-source-bool-void-20260920 ^
  --build-id 2200000000000000
```

Resultado: **PASSED** en 901.66 s. Cada árbol cerró 455/455 pruebas CTest, 148/148 fixtures y
18/18 casos de triage. SH20 produjo los cuatro objetos O0-O3 y ocho ejecutables LLD/NSLink; todos
devolvieron 42. El digest fuente inicial del baseline fue
`d7cbaca552370664bda153cedc4ff87361424af35e8e8fa755537e038f5e7ab6` y el backend certificado
quedó en `4efb0c68d0723ba840da72057c4ff715d51693ef38f720293badb6f6933e3c08`.
La evidencia versionada vive en `selfhost/evidence/gates/SH20/`.

## 7. Riesgos, deuda e higiene

La tabla de tipos es deliberadamente cerrada y aún no maneja conversiones ni representaciones de
ABI más amplias. `!` sólo se emite para valores validados por semántica. El commit excluye salidas,
binarios, temporales y los cambios ajenos de `docs/1.0.1`, `mkdown` y `Claude outputs`.

- [x] Pruebas unitarias verdes (40/40).
- [x] SH20 dirigido pasa O0–O3 con ambos linkers.
- [x] Baseline completo y evidencia SH20 versionada.
