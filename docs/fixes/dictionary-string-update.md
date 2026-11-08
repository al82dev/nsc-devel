# Dictionary: actualizar claves string equivalentes

Base: 152654e5a0e2d84d4c2f245ea7590488f0497430. Caso SDK-02 del reporte
A:/native-sharp-reference/REPORTE-PARA-WORKER-COMPILADOR-SDK.md.

## Alcance
Corregir la actualizacion de una clave string equivalente sin alterar la API ni el contrato de
propiedad del diccionario. El trabajo de la campana se limita a la copia A:/nsc-devel-fixes:
no modifica A:/nsc-devel, los ejemplos originales, C:/native-sharp, LLVM/Clang, Daikon ni la
interoperabilidad C/C++; no usa sus builds, ni realiza merge, rebase, push o despliegue.
Los builds, temporales y evidencia se mantienen en A. Los casos que requieran una decision de
lenguaje o API permanecen pendientes de decision. La bateria completa pertinente y el baseline
final siguen pendientes.

## Causa y correccion
Dos llamadas con String.Copy("red") compilaron, pero el proceso termino con 0xc0000374. El MIR
y LLVM generados para Dictionary<string,int>.AddOrUpdate muestran que la rama de reemplazo
liberaba el parametro owned key explicitamente y luego el cleanup automatico de retorno liberaba
la misma direccion. Se retiro la liberacion explicita: el cleanup conserva la responsabilidad
de liberar la clave redundante exactamente una vez. La insercion, el valor reemplazado, la API y
la propiedad declarada permanecen iguales. No se modifico el compilador.

## Verificacion
Con el compilador aislado 3.0.0 build 4d05f2e394da7813, runtime generado y Core de esta copia,
la reproduccion historica compilo y fallo antes con codigo 3221226356; despues compilo y termino
con codigo 0. La fuente historica no se cambio; una copia del proyecto apunta al Core aislado.
La prueba nsc.sdk.dictionary-string-update ejecuta un programa real y comprueba sustitucion,
conteo y otra insercion. Antes: fallo 0xc0000374. Despues: 1/1 aprobo. La prueba no demuestra
por si sola todos los tipos de clave y valor ni la ausencia de otras regresiones.
Evidencia: A:/nsc-fixes-evidencia/cases/sdk-02/ y baseline-dictionary-regression-2.log,
fixed-dictionary-regression.log. La bateria completa, analisis estatico integral y los demas
casos siguen pendientes.
