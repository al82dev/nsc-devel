# T08: operandos de signo distinto en TLS C atómico

El control C calculó 2147483647 para `_Atomic(int)` con valor -1 y `/= 2U`,
mientras el ejecutable NativeSharp devolvió el código 10: la conversión en HIR
había perdido el tipo sin signo del operando. La reproducción está en
`A:/nsc-interop-build/tanda08-atomic-signmix-repro.log` y su ejecutable en
`A:/nsc-interop-build/tanda08-tls-atomic-7873-7393/Promotion-O0.exe`.

La proyección conserva ahora el tipo entero del operando para el global C
atómico. Cuando el tipo de cálculo difiere del almacenamiento, el descenso
calcula en ese tipo y actualiza mediante `cmpxchg` secuencialmente consistente.
El resto de casos sigue usando el descenso existente. La regla no se aplica a
variables locales NativeSharp.

La construcción incremental y el control agrupado `atomic/validar.cmd` pasaron
en O0–O3. Se compararon el caso anterior y `_Atomic(long long)` negativo con
divisor `uint`, además de los controles anteriores. Registros:
`A:/nsc-interop-build/tanda08-atomic-signmix-build.log` y
`A:/nsc-interop-build/tanda08-atomic-signmix-final.log`. Otras combinaciones y
la validación integral de T08 siguen pendientes.
