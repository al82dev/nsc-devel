# Operadores con argumentos `in` sobre estructuras

## Alcance

Rama aislada `development-fixes-limitaciones-agente-1`, base
`a56498f051939b8e19410212a5dd1585b1fef117`. Se investiga L-01: los
operadores de usuario que reciben estructuras por `in` devolvían valores
incorrectos en O0/O2, mientras métodos ordinarios y operadores por valor
funcionaban. La evidencia anterior se encuentra en el registro de campaña de
la copia principal. Sus pruebas se copian sin alterar sus expectativas.

Se conserva el comportamiento de métodos ordinarios, operadores por valor,
`ref`, `out`, propiedad, excepciones y nombres de parámetros. El candidato
es `OopLlvmImplementation::lowerStaticCall` en
`src/LLVM/Lowering/OopLlvmCalls.cpp`; sólo se modifica tras confirmar la
cadena causal. La regresión es `tests/integration/operator_in_aggregate` y
su ejecutor es `cmake/VerifyOperatorInAggregate.cmake`. La configuración
observada utiliza Windows x64, O0/O2, runtime y enlazador aislados en A.

No se alteran la copia principal, interoperabilidad C/C++, Daikon, fuentes de
LLVM/Clang, instalaciones en C ni builds ajenos. No se hace push, merge,
rebase ni despliegue. Se emplean únicamente compilación incremental y pruebas
dirigidas. NativeSharp.Text permanece pendiente de definir su superficie.

El parche existente en la copia principal distingue los parámetros `in` de
operadores de usuario al preparar el argumento de llamada. Se comparará una
única unidad recompilada antes y después contra las mismas bibliotecas WIP;
los ejecutables híbridos resultantes no constituyen un build limpio de HEAD.
La validación de la rama y la integración siguen pendientes hasta disponer de
artefactos que los acrediten.

## Cadena causal y resultado dirigido

`lowerStaticCall` aplicaba `wrapIndirectArgument` al resultado ya direccionable
de un operando `in`. Para una estructura que utiliza la ABI indirecta, esto
introducía un portador adicional y la función del operador leía la dirección
como si fueran campos de la estructura. El cambio consulta la firma MIR del
operador de usuario y preserva la dirección en los parámetros `in`; los
restantes argumentos siguen por la misma ruta. El mapa de parámetros se usa
cuando existe y no se introduce un cambio general a llamadas ordinarias.

El ejecutor de comparación `A:/nsc-fixes-agent1-compare.py` copió bibliotecas,
objetos y fuentes auxiliares del build de trabajo, compiló la unidad LLVM sin
y con el parche y enlazó ambas variantes con las mismas entradas. La primera
tentativa de preparación falló por una cabecera no copiada; se conserva en
`A:/nsc-fixes-agent1-temp/l01-build-first.log`. La siguiente produjo ambas
variantes, con comandos y salida en `l01-build-second.log` y hashes de entrada
en `l01-comparison/inputs.json`. Los identificadores SHA-256 de los ejecutables
fueron `1a20ea1c2087fb6c1cd4d56b224dac42a0e732216053a31f2f9bc1f338af983e`
antes y `d748a013569bc0844bb0e36b639713702c78847b70c5bd708dcbbf50548d7380`
después. El hash de build impreso procede de los objetos reutilizados y no
identifica por sí solo estas dos variantes.

`A:/nsc-fixes-agent1-test.py` compiló el mismo fixture sin caché y ejecutó O0
y O2: ambas variantes compilaron; antes devolvió 4 en los dos niveles y
después 0 en los dos. El resultado con comandos y salidas se conserva en
`A:/nsc-fixes-agent1-temp/l01-comparison/results.json`. El ejecutor CMake de la
regresión pasó con la variante posterior mediante
`A:/nsc-fixes-agent1-cmake-test.cmd`. Los controles incluyen estructuras
pequeñas y grandes, paso por valor, métodos `in`, `ref`, `out`, alias, campos,
arreglos, igualdad, desigualdad y multiplicación en ambos órdenes.

La prueba `operator_in_aggregate_binary.py` produjo una biblioteca y un
consumidor por separado: las cuatro combinaciones O0/O2 de productor y
consumidor ejecutaron con salida 42; registros y comandos están en
`A:/nsc-fixes-agent1-temp/l01-binary.log`. El enlace usa runtime copiado en A y
bibliotecas de importación aisladas existentes; no se verificó descarga DLL,
otros targets ni todas las formas posibles de firma de operador.

Toda la ejecución anterior utiliza objetos WIP de la copia principal y una
única unidad recompilada. Demuestra la diferencia concreta en ese contexto,
pero no identifica el ejecutable como build limpio del commit de esta rama.
Falta integrar con los cambios locales existentes y repetir las pruebas con el
compilador resultante. No se ejecutó una batería completa.
