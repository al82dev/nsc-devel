# Colecciones con elementos prestados de solo lectura

Antes de la corrección, `[owner.Get()]` podía ejecutar una conversión implícita
que escribía la carga. También podía almacenar directamente la referencia de
clase y permitir una escritura por el arreglo resultante. Las variantes con
`[..existing, owner.Get()]` y `[..owner.Get().Data]`, donde `Data` contiene
objetos, presentaron el mismo problema. Las cinco reproducciones ejecutaron la
mutación para Rc y Arc en O0 y O2; los registros anteriores al cambio se
conservan como `shared-implicit-collection-before.log`,
`shared-implicit-spread-before.log`, `shared-readonly-collection-alias-before.log`,
`shared-readonly-collection-spread-alias-before.log` y
`shared-readonly-spread-source-before.log` en `A:/nsc-fixes-evidencia/`.

El análisis semántico rechaza ahora elementos prestados de solo lectura en
arreglos construidos por una expresión de colección y argumentos de builders
sin contrato de solo lectura. Para un spread de un arreglo prestado, permite
copiar elementos numéricos o booleanos y rechaza conservadoramente los demás:
una estructura que contenga solo valores también puede quedar rechazada hasta
que exista una comprobación transitiva de que no conserva referencias. La regla
evita entregar al arreglo nuevo alias mutables sin seguimiento del dueño.

Con el compilador aislado build `93cd3e9595be3ac6`, la selección
`shared-readonly-collection-check.cmd` pasó para Rc y Arc. Rechazó las cinco
reproducciones y el caso semántico del builder; los controles de expansión de
enteros prestados y conversión desde un objeto mutable ejecutaron 42 en O0/O2.
El consumidor de `generic-owner-sdk43/NativeSharpCore.lib` rechazó los casos
dirigidos de elemento y expansión de objetos, para ambas familias. Registros
`shared-readonly-collection-check.log` y `shared-binary-sdk43-check.log`.

El caso de builder había superado análisis antes del cambio, pero su emisión
fallaba en el verificador LLVM por incompatibilidad del argumento; por tanto,
no se observó la mutación en ejecución para esa variante. El endpoint de rango
ensayado se rechazaba antes del cambio como no integral. La prueba no acredita
todas las formas de colección, alias ni Send/Sync.

Durante la compilación existían cambios simultáneos sin registrar en CMake,
MIR de arreglo decimal y fuentes de matemática, ajenos a esta corrección. Los
tests usaron el compilador incremental identificado arriba; esta evidencia no
acredita un build aislado del commit sin esos cambios concurrentes. No se
incluyen esos archivos en el commit ni se alteran sus pruebas.
