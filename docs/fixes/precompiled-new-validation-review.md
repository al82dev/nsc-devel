# Revisión independiente de la regresión new() binaria

Registro: `A:\nsc-fixes-agente-2-evidencia\precompiled-new-integrated-20261112\run-20261112T060614984993Z`. Por lectura de `results.json`, el fixture de restricciones devolvió 0, Core sin configurar 77, Core configurado incorrectamente 1 y la regresión de argumento de receptor binario 0; cada uno coincide con la salida esperada. Los comandos parten de `A:\nsc-devel-fixes`.

`constraint.log` registra productor O0 correcto y trece casos: nueve controles aceptados con emisión O0 y cuatro rechazos semánticos esperados. El constructor privado de `PrivateCtor` binario se rechaza con el diagnóstico de `new()`; también se rechazan el privado local y el protegido binario, y el tipo oculto ajeno no se resuelve. Se aceptan construcción directa y fábrica local, y Rc/Arc para constructores públicos explícitos o implícitos del productor actual, además del constructor explícito público local. No se ejecutaron consumidores según el fixture.

SHA-256 antes y después en `constraint.log`: compilador `6ec43b6a3c9a65413bcc30911ccb373f1b3df12e62ca28f286517bc8a9cff33e`; Core `.lib` `f91d95c248f477531d38a093605f45343f6be9736d15e2097acd452db4bb54e0`; Core `.nsmeta` `774db7b3a80d64cc594c1b05c448a8dcda7b7258abc729d09984df5d8478718c`. La revisión de estos registros no repitió pruebas ni verificó causalidad del cambio de forma independiente.

La prueba no demuestra compatibilidad con metadata histórica, ni cubre de forma general especializaciones de plantillas binarias cuya marca `isPrecompiled` puede ser falsa; el rechazo de fábrica Core para tipo interno fuente con constructor implícito sigue pendiente. No se afirma cierre general de `new()`.
