# Conversión explícita de máscara SIMD

El caso original `cases/89-mask-convert` convierte `mask<float,8>` a
`simd<uint,8>`. Antes del cambio, el compilador incremental aislado rechazó la
conversión en LLVM; comando y diagnóstico en
`A:/nsc-fixes-evidencia/integration-simd-conversion-before.cmd` y `.log`.

Se incorporó el cambio de la laptop `85c1831e94cb02b0e1e520a778dc1b7951832acf`,
que convierte máscaras a vectores con el mismo tipo y cantidad de lanes. El
ejemplo original usa tipos distintos; además se admiten las parejas explícitas
`float → uint` y `double → ulong` con igual número de lanes. Cada lane falso
produce cero y cada verdadero todos sus bits a uno. Para destino flotante del
mismo tipo se reinterpretan esos bits: el patrón verdadero es NaN, no 1.0.
Las demás conversiones cruzadas y cantidades distintas mantienen su rechazo.
Esta decisión aplica la opción recomendada autorizada para el caso y no cambia
el formato de metadata.

`integration-simd-original.cmd` compiló y ejecutó el ejemplo original sin
modificar en O0/O2, con salida 0 en ambos casos. La prueba de la laptop
`integration-simd-conversion-test.cmd` ejecutó 20 programas con diez tipos de
lane en O0/O2 y verificó bits alternados; también rechazó tres combinaciones
incompatibles en ambos niveles. `integration-simd-float-unsigned.cmd` ejecutó
cuatro controles adicionales de bits alternados para las parejas flotante a
entero sin signo en O0/O2, todos con salida 42. Las pruebas dirigidas de la
ruta ASan, la coherencia provisional de Arc y las cinco unitarias de `#run`
volvieron a pasar. Comandos y logs homónimos están en
`A:/nsc-fixes-evidencia`.

La compilación fue incremental en A y coexistió con cambios locales sin
registrar, por lo que no certifica un árbol limpio. Las pruebas cubren esas
formas y plataforma, no todas las arquitecturas o longitudes posibles. No se
ejecutó la batería completa ni se cambió el código de interoperabilidad C/C++.
