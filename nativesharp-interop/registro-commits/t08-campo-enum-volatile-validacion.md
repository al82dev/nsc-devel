# T08-006: validación de campos enum C volatile

Con NSC build `5c1e6c7fa15d8a3e`, compilado incrementalmente en
`A:/nsc-interop-build/tanda1-ON`, `pruebas-tanda-08-volatile-campos-c/validar.cmd`
terminó con código 0. El fixture ejecutó O0–O3 en consumo directo, Shared y
proyecto con referencia de metadata. `Metadata.nsproj` no declara el encabezado,
aunque el archivo permanece en el directorio de trabajo.

El comparador contrastó el IR de Clang con NSC: el enum alineado y el enum de
union usan cargas acquire y escrituras release, `volatile`, `i32`, alineación 4;
el campo enum empaquetado usa accesos `volatile` sin orden atómico, alineación 1.
La comparación cubre directo, Shared y metadata sin optimizar, y directo O1–O3.
El control `ProbeEnum` también compiló y ejecutó en O0.

Esta evidencia no acredita todos los enums, punteros locales ni el cierre de
T08-006 o de la tanda 8.
