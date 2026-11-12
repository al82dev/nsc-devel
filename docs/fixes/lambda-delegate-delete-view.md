# Destrucción de un delegado con vista prestada

Base: `65bf5b83e618e386126628fa0b780fb26a4ad547`. La corrección rechaza
`delete` de un delegado local administrado cuando una vista activa puede
depender de su captura, incluso si la destrucción se solicita mediante un
alias local conocido. Permite destruirlo después del ámbito de la vista y
destruir un delegado independiente. No cambia interoperabilidad C/C++, Daikon,
LLVM/Clang externos, instalaciones protegidas ni builds ajenos; no hace merge,
rebase sobre interoperabilidad, push ni deploy. Sólo se usan pruebas dirigidas
y compilación incremental en A.
Se protegen expresamente `A:/nsc-devel`, `C:/native-sharp`, los builds de
interoperabilidad y los originales del informe. Temporales, cachés y evidencia
permanecen en A; `NativeSharp.Text` sigue pendiente de definir su superficie.

El análisis de `delete` sólo protegía objetos de campo, arrays y dueños con
reglas existentes; el delegado ordinario escapaba esas guardas. El cambio
construye un grafo bidireccional con inicializadores y posibles fuentes de
asignación de delegados locales, incluyendo expresiones condicionales, y recorre
la componente del operando buscando préstamos activos. Las asignaciones se
tratan conservadoramente como posibles aliases aunque hayan ocurrido en ramas
distintas. La guarda no afecta funciones externas C ni `delete[]`.

`lambda_delegate_delete_view.py` reproduce las formas directa, alias, alias
inverso, cadena, reasignación y condicional para Rc/Arc y salida ref/out: el
compilador previo aceptó los 24 casos y el híbrido con el cambio los rechaza
con el diagnóstico previsto. Los negativos sólo se analizaron. Tres controles
seguros por combinación comprueban en O0/O2 una destrucción: cierre `raii`,
`delete` después de finalizar la vista y `delete` de un delegado independiente.
Comandos y logs: `A:/nsc-fixes-agent1-delete-regression*.cmd` y
`A:/nsc-fixes-agent1-temp/lambda-double-use/delete-regression*.log`.

La unidad modificada compila con las cabeceras de la rama. La ejecución utilizó
un compilador híbrido de objetos y cabeceras WIP, no un build limpio de esta
revisión; su hash y procedencia están en
`A:/nsc-fixes-agent1-temp/delete-hybrid/delete-inputs.json`. No se han cubierto
campos, parámetros, almacenamiento global o aliases cuyo origen no pueda
identificarse; este cambio no certifica la seguridad completa de delegates.
Integration verification, 2026-11-11: incremental `nsc` build succeeded and
`ctest --test-dir A:\nsc-fixes-evidencia\build -R nsc.compiler.lambda-delegate-delete-view -V`
passed 1/1; full output is in
`A:\nsc-fixes-evidencia\lambda-delete-integrated-20261111.log`. The compiler
also contained other uncommitted work, so this is not a clean-commit build.
