# NativeSharp 2.0 self-hosting — tanda 21: control de bucles

## 1. Identidad

| Campo | Valor |
|---|---|
| Estado | `LANDED` |
| Gate | `SH19` |
| Rama | `development` |
| Commit de entrada | `8e698561f` |
| Commit de cierre | este commit (`selfhost(tanda-21): lower structured loop control`) |
| Fecha | 2026-09-20 |
| Baseline | `tanda-21-sh19-source-loop-control-20260920` |

## 2. Objetivo y frontera

SH19 baja desde el AST `for`, `do`/`while`, `break` y `continue`, incluidas salidas y continuaciones
del bucle anidado activo. La rebanada conserva los valores `int`, expresiones y locales de SH16–18.
No añade `switch`, `foreach`, operadores `++`/`--`, asignaciones compuestas, objetos, arrays,
strings, excepciones ni destrucción por scope.

La selección nace de evidencia de las fuentes de `nsc-next`: contienen las cuatro construcciones y
SH18 sólo aceptaba `while`. El fixture suma 26 mediante un `for` con `continue` y `break`; después
un `do` agrega 16 y el resultado observable es 42.

## 3. Diseño, flujo e invariantes

`LoopBinding` retiene dos handles LLVM no propietarios: salida y continuación. La lista `loops` de
`FunctionLoweringState` actúa como pila; `PushLoop` precede al cuerpo y `PopLoop` se ejecuta antes de
salir del lowering. `BreakTarget` y `ContinueTarget` consultan siempre la cima, de modo que un bucle
interno no puede saltar accidentalmente a un bloque externo.

`while` continúa hacia condición; `do` también continúa hacia su condición posterior; `for`
continúa hacia incremento y sólo después vuelve a condición. Una sentencia de transferencia emite
`br`, marca la ruta como terminada y evita un segundo terminador. `for (;;)` construye una constante
LLVM `i1 true`. `EmitEntryAlloca` inserta todos los slots antes de la primera instrucción del bloque
de entrada y restaura el bloque activo; así una declaración posterior a una rama no se convierte en
reserva dinámica. Las construcciones fuera del alcance fallan cerradas con el código de backend ya
establecido; no se sintetiza resultado ni se copia una imagen.

## 4. Archivos, tipos, métodos y contratos

| Archivo | Acción | Responsabilidad |
|---|---|---|
| `selfhost/nsc/CodeGen/LlvmBackend.ns` | modificar | pila de bucles y lowering estructurado |
| `lib/NativeSharp.LLVM.C/LLVMApi.ns` | modificar | exponer posicionamiento ya congelado |
| `selfhost/contracts/llvm-c-api.contract.json` | modificar | fijar el nuevo hash de firmas |
| `selfhost/tests/codegen/source-loop-control.ns` | crear | fixture observable de las cuatro formas |
| `selfhost/contracts/source-loop-control.contract.json` | crear | contrato SH19 v1 |
| `selfhost/bootstrap/verify_source_loop_control.py` | crear | anatomía, hashes y ocho ejecuciones |
| `cmake/VerifyNativeSharpSourceLoopControl.cmake` | crear | matriz de compilación/enlace |
| `CMakeLists.txt` | modificar | registrar `nsc.selfhost.source-loop-control` serial |
| prueba Python, roadmap y README | modificar | regresión estructural y trazabilidad |

`LoopBinding(void*,void*)` expone `BreakTarget()` y `ContinueTarget()`. `PushLoop(void*,void*)`
transfiere un binding a la lista; `PopLoop()` elimina el último. Los getters devuelven `null` fuera
de un bucle, lo que hace fallar un `break`/`continue` inválido aunque la semántica previa debió
rechazarlo. Los handles pertenecen al módulo LLVM y no se liberan desde el estado NativeSharp.

El contrato usa schema `nativesharp-selfhost-source-loop-control` v1; el reporte usa
`nativesharp-selfhost-sh19-report` v1 y scope `for-do-break-continue`. No cambia CLI, ABI, runtime ni
contrato LLVM-C: las APIs de navegación y posicionamiento añadidas al binding ya estaban congeladas
en el inventario `nsc0_import_symbols` de `llvm-c-api.contract.json`, cuyo hash de firmas se
actualiza, y se consumen directamente desde `LLVM-C.dll`.

## 5. Pruebas y divergencias

La prueba unitaria exige el contrato, los cuatro kinds y las operaciones de pila. La prueba CTest
construye `nsc-next` en O0–O3, produce cuatro objetos, inspecciona símbolos/relocations, enlaza ocho
ejecutables mediante `lld-link` y NSLink y exige exit 42. SH16–18 permanecen acumulativos.

La primera ejecución sin el entorno del baseline fue rechazada antes de compilar por falta de
`NS_ROOT`; se repitió con la raíz explícita. Entonces nsc0 detectó que dos expresiones ternarias
mezclaban `null` y `void*`; los getters se reescribieron con retorno condicional explícito, sin
cambiar su contrato. La siguiente corrida produjo un objeto válido pero LLD rechazó `__chkstk`:
un local posterior al `for` se había emitido como `alloca` dinámico. Se corrigió la raíz mediante
`EmitEntryAlloca`, no agregando una dependencia de enlace. No se considera válida una prueba que
use un binario residual ni que omita perfiles.

## 6. Baseline de cierre

Comando previsto, con rutas configurables y escritura limitada a `A:\`:

```bat
python nscbuilder.py --baseline --jobs 16 --prefix A:\native-sharp ^
  --build-dir A:\nsc-devel\selfhost\out\nsc0-build ^
  --test-build-lld A:\nsc-devel\selfhost\out\tests-lld ^
  --test-build-nslink A:\nsc-devel\selfhost\out\tests-nslink ^
  --llvm-root C:\clang-llvm --nslink A:\native-sharp\bin\nslink.exe ^
  --history-root A:\nsc-devel\selfhost\evidence\runs ^
  --work-root A:\nsc-devel\selfhost\out\work --temp-root A:\nsc-devel\selfhost\out\temp ^
  --write-root A:\ --run-id tanda-21-sh19-source-loop-control-20260920 ^
  --build-id 2100000000000000
```

Resultado `PASSED` en 876.37 s: 454/454 CTest, 148/148 fixtures y 18/18 triage por enlazador,
sin problemas. G00 quedó `LANDED` con digest de fuentes
`477ab4b9e4aa7ed31db3ef59db2641ccc7f033175bfff72fe457fa3a92c170db`. SH19 registró ocho
ejecuciones con exit 42 y objetos sin dependencia `__chkstk`; los hashes se fijan en
`selfhost/evidence/gates/SH19/gate.json`.

## 7. Riesgos, deuda e higiene

Los locals siguen viviendo hasta el fin de función aunque sus allocas ya son canónicos en entry.
Una transferencia todavía no ejecuta cleanup porque SH19 no baja ownership ni excepciones. El
commit sólo podrá contener fuentes, contratos, pruebas, documentación y evidencia; se excluyen
`selfhost/out`, binarios, temporales y todos los cambios ajenos ya presentes en el árbol.

- [x] Pruebas unitarias verdes (39/39).
- [x] Fixture SH19 devuelve 42 en O0–O3 con ambos linkers.
- [x] Baseline completo y evidencia SH19 versionada.
