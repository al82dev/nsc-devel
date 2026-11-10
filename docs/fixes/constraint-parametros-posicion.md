# Identidad de parámetros en restricciones dependientes

La rama ya contenía `6022fe794`, que permite restricciones dependientes como
`TOp : Reducer<T>`. Sin embargo, la regresión de la laptop
`generic_dependent_constraint.py` aún fallaba en `Alpha`: comparaba como
incomparables `Reducer<T>` y `Reducer<U>` aunque `T` y `U` ocupaban la misma
posición en dos implementaciones. La salida previa está en
`A:/nsc-fixes-evidencia/laptop-dependent-current.log`.

Se incorporó la idea de normalización por posición del commit de laptop
`5a4bf6e56ccdac3e75df25ad5f1a39df6f5aa47a`, adaptada al validador de esta
rama. Solo se normaliza un parámetro directo sin modificadores dentro de la
restricción abierta. La identidad resuelta de la plantilla permanece en la
clave y parámetros en posiciones distintas mantienen identidades distintas.
No se sustituyó la lógica local de seguridad de hilos de Arc.

El build incremental `dependent-constraint-position-build.cmd` terminó con
código 0. La prueba de laptop incorporada al repositorio pasó sus 14 casos,
incluido el caso equivalente y el rechazo por posiciones diferentes, con
análisis y O0/O2 según corresponda. También pasaron
`generic-dependent-regression.cmd`, `generic-constraints-existing.cmd`,
`conditional-unsafe-coherence-after.cmd`, `conditional-unsafe-thread-expanded.cmd`
y `integration-simd-select-test.cmd`. Comandos y logs están en
`A:/nsc-fixes-evidencia`.

Se ensayaron adicionalmente restricciones anidadas `Reducer<Wrap<T>>` con
parámetros renombrados y de posición distinta. Los fixtures conservados en
`dependent-nested-alpha/` y `dependent-nested-position/` no invocan las
sobrecargas: ambos pasaron el análisis, lo que no demuestra cuál se escogería
ni si las posiciones se distinguen al especializar. Esa forma queda pendiente;
no se presenta como cubierta por este cambio. El primer fixture con cierres
`>>` se rechazó por sintaxis y se corrigió separando los cierres.

El build coexistió con cambios locales sin registrar, incluido el prototipo de
Arc, y no certifica un árbol limpio. Se prepararon solo los hunks de esta
corrección en el archivo compartido. No se ejecutó la batería completa ni se
modificaron fuentes de interoperabilidad C/C++.
