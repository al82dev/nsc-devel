# Casos numéricos: alcance y evidencia

## Límites matemáticos con SDK binario

El ejecutor `cmake/VerifyDeterministicMathBoundaries.cmake` acepta de forma
opcional `CORE_BINARY` y `CORE_METADATA`, y genera un proyecto de consumo
sin modificar la fuente del fixture. Su ejecución dirigida terminó con código
0 para los ocho cruces del consumidor O0–O3 y `precise`/`deterministic`, con
denormales preservados. Se usaron el compilador aislado
`A:/nsc-fixes-evidencia/build/bin/nsc.exe` (build hash `5f77cbcea6996489`) y
el Core binario de `A:/nsc-fixes-evidencia/integracion-vistas-sdk`, cuya
procedencia y hashes constan en `integracion-vistas-pruebas-20261112.md`.
El comando reproducible es
`A:/nsc-fixes-evidencia/numeric-binary-boundaries-20261113/verify.cmd`;
su log es silencioso cuando pasa. También pasó la vía anterior con Core fuente
mediante `verify-source.cmd` en la misma carpeta. El Core binario es un productor
previo: las opciones de los consumidores no prueban que ese productor haya sido
compilado en cada modo ni acreditan todos los valores de Exp y Log.

## Alcance operativo

Repositorio `A:/nsc-limitaciones-numericos`, rama `development-fixes-limitaciones-numericos`.
Punto de partida compartido `d00b3a8698d7750c80887d029d64fda481d67211`, no certificado;
inicio de este carril `f19fb10528a17c7c93af30302b00d65b3b3ab8b2`, árbol limpio.
Objetivo: completar las tareas de `TAREAS-CHAT-NUMERICOS.md`, confirmar cada caso con
productores propios, corregir defectos demostrados y conservar evidencia reproducible.
Se permiten decisiones técnicas recomendadas dentro de ese alcance. Compilaciones incrementales
y pruebas dirigidas exclusivamente; no se presenta una selección dirigida como batería completa.

La reproducción original se conserva. Son ajenos y protegidos `A:/nsc-devel`, las otras copias,
la instalación `C:/native-sharp`, fuentes LLVM/Clang, Daikon y la interoperabilidad C/C++.
No hacer push, merge, rebase ni despliegue. Las salidas, cachés y temporales permanecen en A,
separados de los demás chats. `NativeSharp.Text` permanece aplazado. Los archivos compartidos
`src/LLVM/Internal/OopLlvmImplementation.h`, `CMakeLists.txt`, `docs/fixes/campaign-status.md`
y `docs/fixes/decisiones-pendientes.md` están reservados al integrador central.

Este carril es dueño de `src/LLVM/Lowering/OopLlvmOperators.cpp`; se asigna un solo editor
a la vez. Los cambios deben incluir sus pruebas y explicación en commits por causa coherente.
Las pruebas existentes y los ejemplos no se adaptarán para ocultar fallos. Una observación
histórica no demuestra el estado de esta revisión. Cierre: casos asignados clasificados,
defectos reproducidos corregidos y regresiones dirigidas aprobadas, con límites explícitos.

## Inventario inicial

| Caso | Evidencia anterior | Estado en esta copia |
|---|---|---|
| COMP-01 saturación int y casts, ejemplo 48 | -2147483648, -25536 y 34 | Ya no reproducible O0/O2 con Core fuente local |
| COMP-02 decimal, ejemplo 49 | factura 101, igualdad falsa, redondeo 100/100/101 | Ejemplo ya no reproducible O0/O2; otros casos reproducidos |
| COMP-03 checked i128, ejemplo 49 | wrapping observado | Ya no reproducible O0/O2 con Core fuente local |
| COMP-04 división i128, ejemplo 49 | enlace NSL1020: `__divti3` | Ya no reproducible O0/O2 |
| Decimal saturating suma y resta, escalas no reducibles | candidato v2 sin ejecutar | Corregido y verificado mediante pruebas dirigidas; commit `3843895d1` |
| División y residuo i128/u128 | existen fixtures de regresión | Runners dirigidos aprobados O0/O2 |
| SDK-NCSS-02 Exp límites y subnormales | evidencia anterior con Core fuente | Fixture aprobado con productor propio y Core fuente |
| SDK-NCSS-01 Log infinito | evidencia anterior con Core fuente | Fixture aprobado con productor propio y Core fuente |
| L-07 arreglos decimal y L-08 stackalloc decimal | pruebas históricas aprobadas en otro árbol | Fixtures fijo, genérico y stackalloc aprobados O0/O2 |

Fuentes históricas: `A:/native-sharp-reference/REPORTE-PARA-WORKER-COMPILADOR-SDK.md` y
`A:/native-sharp-reference/LIMITACIONES-SDK-Y-COMPILADOR.md`. Los archivos de reproducción
están identificados en esas fuentes. La lista incluye las verificaciones asignadas a este carril;
los demás casos del reporte pertenecen a otros carriles.

## Decisión para saturación decimal

Según la instrucción de la tarea, saturar al límite con el signo correcto sólo si la magnitud
matemática excede el rango. Si se encuentra dentro del rango pero no es exactamente
representable, producir `DecimalOverflow` (145). Conservar reducciones exactas; half-up es
el comportamiento histórico documentado para división, no una política nueva para suma/resta.
El candidato histórico `A:/nsc-fixes-agent1-temp/decimal-saturating-inexact-candidate-v2-20261112.patch`
era una propuesta sin validar. La solución adoptada calcula el límite con escala en 320 bits;
los casos interiores requieren que el ejecutor exija 145, pues su fuente por sí sola puede
devolver 0 con una saturación incorrecta. Se añadieron controles representables y reducibles.

## Hipótesis iniciales, posteriormente reproducidas

En `applyDecimalBinaryOperation` el extractor de coeficiente usa 64 de los 96 bits declarados.
Las rutas de multiplicación y división consumen ese extractor; división desplaza la escala del
operando derecho sin máscara y su resultado no muestra tratamiento del signo en esa ruta.
Estas observaciones fueron hipótesis estáticas iniciales; la línea base y las correcciones
posteriores aparecen más abajo y en los documentos de multiplicación y división.

Al comienzo, `nsc --version` por PATH identificó build hash `4998f7ed435b91d5`, que corresponde
al compilador instalado y no acredita procedencia de esta copia. El fixture de matemática
referencia Core fuente; no certifica consumo binario.

La configuración propia en `A:/nsc-limitaciones-numericos-build` usa Ninja, Release y
Clang/LLVM 23.1.0. El usuario autorizó las rutas absolutas que el build y las pruebas necesitan
para esta copia, sin cambiar PATH ni escribir en C. La configuración inicial asignó a `NSC_TEST_COMPILER`
el nsc instalado en C:; no ejecutar esos tests sin seleccionar el nuevo ejecutable.

Los proyectos originales `PolicyResults`, `DecimalValues` y `CheckedInteger` referencian
Core mediante una ruta absoluta de C:. Para diferenciar procedencia, conservar copias idénticas
de los originales y generar una variante de infraestructura cuyo único cambio sea apuntar
la referencia de proyecto a Core fuente de este repositorio; comparar los hashes de las
fuentes. La variante no se presentará como el proyecto original exacto ni como consumo binario.
`I128DivisionLink` no contiene esa referencia y puede conservarse íntegro.

## Línea base dirigida de esta rama

El target `nsc` terminó con código 0. Ejecutable
`A:/nsc-limitaciones-numericos-build/bin/nsc.exe`, build hash `f2987823e38d264d`, SHA-256
`2349ff10303f45f3ff456193a1530c760ecf6980959c87acc1dc9b1f3f645858`.
Runtime `A:/nsc-limitaciones-numericos-build/generated/NativeSharp.Runtime.Win32.lib`, SHA-256
`3235a612c2dc62bb05c244885bc135ec76bf847f4f9a700badd9017c1f913a53`.
Las cuatro bibliotecas de importación se generaron bajo `build/sdk/win64/import-libraries`.
Compilador y runtime son salidas de este checkout; NSLink es una dependencia externa.

La ejecución `originales-ejecucion-20261112T120850Z-c45b1ee882/results.json` conserva los
comandos, hashes, códigos y salidas. En O0 y O2, las variantes con Core fuente local del 48
producen `2147483647`, `32767`, `255`; la del 49 produce factura 2314, igualdad verdadera
y redondeos 100, 101, 101. El caso checked i128 termina 147 y el proyecto de división firmado
compila, enlaza y termina 0. Estos casos históricos ya no se reproducen en las configuraciones
observadas; las variantes Core no demuestran consumo del SDK binario.

`VerifyDecimalSaturatingInexact.cmake` con `CASE_FILTER=PositiveInner1Add` compiló en O0/O2,
pero ejecutó 31 en ambas configuraciones en lugar del trap 145; el resultado observado queda en
`A:/nsc-limitaciones-numericos-evidencia/baseline-saturating`. Defecto reproducido.

`VerifyDecimalMulDivObserve.cmake` compiló los 12 proyectos en O0/O2 y registró 18
discrepancias contractuales en
`A:/nsc-limitaciones-numericos-evidencia/baseline-mul-div/observations.txt`:
signos de multiplicación y división, coeficientes con bit 64, desbordamiento checked y
saturación del producto. Un divisor decimal no nulo con coeficiente 2^64 produjo una
excepción de división entera entre cero. Half-up 0.5/3 y trap de escala 29 pasaron en ambas
optimizaciones. El divisor decimal cero también produjo la excepción de hardware; se conserva
como observación y no se atribuye un código contractual decimal histórico.

`cmake -DNSC_EXECUTABLE=A:/nsc-limitaciones-numericos-build/bin/nsc.exe
-DPROJECT_FILE=A:/nsc-limitaciones-numericos/tests/integration/deterministic_math_boundaries/DeterministicMathBoundaries.nsproj
-DOUTPUT_DIR=A:/nsc-limitaciones-numericos-evidencia/baseline-deterministic-math
-DRUNTIME_LIBRARY=A:/nsc-limitaciones-numericos-build/generated/NativeSharp.Runtime.Win32.lib
-DNSLINK_EXECUTABLE=nslink.exe -P cmake/VerifyDeterministicMathBoundaries.cmake` terminó 0.
El runner usa O0–O3, precise/deterministic, denormales preservados y Core fuente; no acredita
consumo binario ni precisión para todas las entradas.

La corrección de saturación está en `3843895d1`: 50 casos O0/O2, 18 controles de las
políticas checked/default y cuatro regresiones O0/O2 pasaron tras el build incremental.
El detalle, el análisis estático y sus limitaciones están en `decimal-saturating-inexact.md`.
`CMakeLists.txt` sigue reservado al integrador y el runner se invocó directamente.

La corrección del ancho y signo de multiplicación está en `901a8f992`; siete fixtures
independientes pasaron O0/O2, además del control de literal y factura. Los detalles y límites
están en `decimal-multiply-wide-sign.md`. La línea base de las decisiones de división está
en `baseline-division-design/observations.txt`; la corrección y sus pruebas aparecen abajo.

Con el compilador posterior a `901a8f992`, SHA-256
`a8f04352b005f57b5d913cf65972f126165d9dc7b9d629d63b72850c8b8a74b0`, los runners
`VerifySignedI128Division`, `VerifyI128Divisor`, `VerifyI128Remainder`,
`VerifyUnsignedI128Division` para división y residuo, y `VerifyCheckedI128Add` terminaron
con código 0. También terminaron 0 `VerifyDecimalFixedArray`, `VerifyDecimalGenericArray`
y `VerifyDecimalStackalloc`, en O0/O2. Salidas aisladas: carpetas `i128-*` y `layout-*` en
`A:/nsc-limitaciones-numericos-evidencia`. No prueban otras configuraciones ni ABI general.

Con la implementación de división final, el build incremental produjo el compilador SHA-256
`027ff35778a24ad9e83ed31a6b8beb803f321f6b1bd8dde5c0f4dddd3f3a7053`. Pasaron 56 casos
dirigidos de multiplicación/división y otros dos del exponente máximo, los 50 de saturación,
60 entradas diferenciales O0/O2, checked decimal, los runners i128/u128, los cuatro ejemplos
originales preservados o variantes Core local, y matemática determinista con Core fuente.
Los resultados y limitaciones se detallan en `decimal-division-wide-sign.md`.
Una repetición dirigida de los ocho runners silenciosos conservó sus códigos, comandos,
salidas y hashes en `regresiones-finales-20261112T124000Z-a71cbab4cb/results.json`; todos
terminaron 0. El binario se construyó antes del commit de división, con el mismo contenido
fuente que quedó en ese commit; el hash del binario no es el identificador Git del commit.

El consumo del SDK como binario sigue sin verificarse en este carril: el fixture de matemática
usa `ProjectReference` a Core fuente. El productor y gate de SDK binario pertenecen a otro
carril; registrar esta dependencia no sustituye su resultado. Propuesta de integración del
registro de pruebas y conflictos potenciales: `propuesta-registro-numericos.md`.

## Decisiones para división decimal

F04 documenta half-up y el trap de escala superior a 28; F05 deja la política de overflow
del cociente como residual. No se encontró una regla histórica decimal para dividir entre
cero. Por la autorización de decisiones delegada, se adopta emitir `DivisionByZero` (129
en este runtime) para cualquier divisor decimal de coeficiente cero, antes de calcular la
escala. Se adopta conservar half-up sobre la magnitud, con signo XOR; reducir ceros exactos
del cociente cuando la escala calculada impide representarlo; y, si aún no cabe, trap 145
para default/checked. Para `saturating`, se limita solo cuando el valor matemático exacto
rebasa ±Max; si está dentro del rango pero el resultado redondeado no es representable
exactamente, se emite 145. Son decisiones nuevas, no una afirmación de contrato histórico.
La precedencia de divisor cero frente a escala inválida es deliberada. La evidencia previa
`baseline-division-design/observations.txt` conserva las diferencias antes de implementarlas.
