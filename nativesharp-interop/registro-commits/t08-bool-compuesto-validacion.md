# T08-015: validación dirigida de `_Bool` C compuesto

La construcción incremental `cmake --build A:/nsc-interop-build/tanda1-ON
--target nsc --parallel 4` terminó con código 0. El grupo
`pruebas-tanda-08-record-c/validar.cmd` terminó con código 0: `|=`, `&=` y
`^=` sobre el arreglo C global `volatile _Bool` conservaron los valores
esperados en O0, O1, O2 y O3; el control negativo de bool nativo permaneció.
Registro: `A:/nsc-interop-build/t08-bool-compuesto-record.log`.

El IR O0 `A:/nsc-interop-build/tanda08-record-c/volatile-bool-compound.ll`
contiene cuatro cargas `atomic volatile i8 ... acquire, align 1` y cuatro
escrituras `atomic volatile i8 ... release, align 1` correspondientes a las
operaciones. El verificador incorporado al fixture comprueba específicamente
esas instrucciones. Se ejecutó el verificador sobre el IR ya producido; no se
repitió el grupo tras añadirlo.

La evidencia cubre únicamente esos operadores entre bool sobre ese origen C en
modo MS. Las operaciones aritméticas, otros orígenes, T08-015 y la validación
integral posterior a estos cambios siguen pendientes. NSLink no se modificó.
