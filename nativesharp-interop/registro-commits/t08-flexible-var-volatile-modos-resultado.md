# T08-010/015: resultado del alias local en modos volatile

Con NSC build `5c1e6c7fa15d8a3e` y Clang 23.1.0 de `A:/al82dev-clang`,
`diagnosticar-var-volatile.cmd` terminó con código 1, por diferencias del IR:

| Modo | Acceso | Clang | NSC |
| --- | --- | --- | --- |
| Microsoft | lectura | atomic volatile acquire, align 4 | volatile, align 4 |
| Microsoft | escritura | atomic volatile release, align 4 | volatile, align 4 |
| ISO | lectura | volatile, align 4 | volatile, align 4 |
| ISO | escritura | volatile, align 4 | volatile, align 4 |

El control usó C11 y el triple `x86_64-pc-windows-msvc`, sin optimización.
Los IR están en `A:/nsc-interop-build/tanda08-flexible-var-volatile/`. El
proyecto NS obtiene el modo de su `CppLibraryReference`; este contraste no
decide cuál debe prevalecer cuando un consumidor usa referencias divergentes.
No acredita concurrencia, optimizaciones, reasignación o flujo entre ramas.
El defecto y el diseño del modo de la unidad consumidora siguen pendientes.
