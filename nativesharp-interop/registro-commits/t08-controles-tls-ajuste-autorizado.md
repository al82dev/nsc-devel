# T08: ajuste autorizado de controles TLS

El usuario autorizó ajustar los controles TLS, conservar los originales como
diagnósticos separados, mantener NSLink intacto y validar la tanda completa.

El control C original que enlaza directamente una variable TLS de otra DLL
falla con Clang oficial antes de ejecutar NSC. El gate utiliza el control PE C
existente y el consumidor NativeSharp directo O0–O3, incluido aislamiento por
hilos. El script original `directo/validar.cmd` permanece disponible sin cambios.

Los controles originales `validar-nsc-escalares.cmd`,
`validar-nsc-escalares-con-entry.cmd` y
`validar-escalar-carga-dinamica.cmd` permanecen sin cambios como diagnósticos.
Sus DLL sin importaciones exhibieron inicialización TLS incorrecta también en
el control C. Para el gate se usa la variante existente con importación, una
variante adicional con `DllMain` e importación, y una variante adicional de
carga dinámica con importación. Estas variantes siguen comprobando los mismos
valores de TLS, la carga y la ejecución de NSC cuando corresponde.

Esto cambia qué configuraciones son requisito del gate: no demuestra soporte
de DLL sin importaciones ni acceso C directo entre DLL. Esas limitaciones
permanecen abiertas y los originales pueden ejecutarse para diagnosticarlas.
La validación integral ajustada `validar-tanda-08.cmd` terminó con código 0 el
2026-11-07. Se construyeron incrementalmente los objetivos configurados y se
ejecutaron todos los casos listados en el gate, incluidos los controles TLS
ajustados y el productor y consumidor NSMETA O0–O3. Registro completo:
`A:/nsc-interop-build/tanda08-integral-ajustada-20261107.log`.

La aprobación del gate acredita únicamente sus casos y configuraciones, no
resuelve por sí misma los ítems todavía abiertos de T08 ni las limitaciones
indicadas arriba. No se modificó NSLink.
