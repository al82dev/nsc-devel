# Referencias repetidas a una captura `move`

La base de esta rama es `65bf5b83e618e386126628fa0b780fb26a4ad547`. El
alcance es corregir el falso diagnóstico cuando una misma lambda usa varias
veces un local o parámetro que capturó una sola vez mediante `move`. Se
conservan el rechazo de uso después de mover, la propiedad de la captura y los
contratos previos. No se modifican C/C++, Daikon, LLVM/Clang externos ni las
copias o instalaciones protegidas; sólo se usan builds incrementales y pruebas
dirigidas en A. No se hace merge, rebase sobre interoperabilidad, push ni deploy.
Se protegen explícitamente `A:/nsc-devel`, `C:/native-sharp`, los builds de
interoperabilidad y los originales del informe. Temporales, cachés y evidencia
permanecen en A; `NativeSharp.Text` sigue pendiente de definir su superficie.

## Causa y cambio

`collectLambdaCaptures` recorría cada uso de un nombre y llamaba la validación
de captura antes de que `addLambdaCapture` deduplicara la captura. El primer
uso marcaba movido al dueño exterior y el siguiente denunciaba un segundo
movimiento, aunque era el mismo cierre. El cambio recuerda las capturas move
directas ya validadas en esa lambda y registra cada uso posterior contra el
mismo ordinal. Una captura añadida transitivamente por una lambda hija no se
considera validada como uso directo hasta recorrerlo en su propia lambda.

## Evidencia y límites

En el WIP congelado SHA-256
`dd62ce77ae873c501c9fc7197786464e36742b79142190969d9e11d79e995556`,
la regresión portable falló nueve análisis válidos: dos lecturas de un local,
dos lecturas de un parámetro owned y dos lecturas en la lambda anidada, para
clase simple, `Rc` y `Arc`. No se ejecutaron negativos. Con un compilador
híbrido SHA-256
`34093d8e931268319ae7f9376b34cf97962d4f5383d9035ea198143e2cefabe3`
terminó sin fallas: nueve variantes anteriores aceptadas, controles negativos
de uso posterior, dos cierres y move dentro del cuerpo rechazados, y controles
válidos O0/O2 con una destrucción observada. Los casos anidados de la prueba
portable sólo se analizan: su destrucción requiere cobertura independiente.
Comandos y salidas: `A:/nsc-fixes-agent1-lambda-regression*.cmd` y
`A:/nsc-fixes-agent1-temp/lambda-double-use/move-regression*.log`.

La unidad modificada compiló con las cabeceras de esta rama; el ejecutable
híbrido usa bibliotecas, objetos y cabeceras WIP copiados y no certifica un
build limpio del commit. Las entradas del híbrido están en
`A:/nsc-fixes-agent1-temp/lambda-hybrid/inputs.json`. La destrucción observada
requiere `raii` en el delegado o un `delete` explícito: un delegado ordinario
sin liberación explícita no acredita el fin de vida de su captura. No se
certifican todas las formas de anidamiento, alias desconocidos ni excepciones.
Integration verification, 2026-11-11: incremental `nsc` build succeeded and
`ctest --test-dir A:\nsc-fixes-evidencia\build -R nsc.compiler.lambda-move-capture-reuse -V`
passed 1/1; full output is in
`A:\nsc-fixes-evidencia\lambda-move-integrated-20261111.log`. The compiler
also contained other uncommitted work, so this is not a clean-commit build.
