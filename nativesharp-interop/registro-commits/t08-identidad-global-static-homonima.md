# T08-002-09c1: globales C static en encabezados homónimos

Con el NSC de desarrollo de `A:/nsc-interop-build/tanda1-ON`, el control
dirigido `homonimos/validar-global-static.cmd` terminó con código 0. Con
`dirA/same.h` se acepta el acceso a `c_same_static`. Al añadir `dirB/same.h`,
de contenido igual, o `dirC/same.h`, con otro valor, NSC rechaza el acceso
con `ambiguous C internal global origins`. La prueba exige ambos rechazos y
el diagnóstico; no afirma ejecución ni selección de una copia arbitraria.

El control se agregó al validador de identidad para la ejecución agrupada
posterior. No se modificó código del compilador ni se ejecutó la batería del
bloque o la tanda. T08-002 sigue abierto.
