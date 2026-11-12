# Regresión dirigida de publicación mediante despacho

`tests/integration/shared_receiver_dispatch_regression.py` comprueba
16 combinaciones de Rc/Arc, sintaxis virtual/interfaz, constructor/destructor
y cuerpo que publica o no publica el receptor. Cada caso debe superar primero
el análisis semántico; los ocho publicadores deben fallar al emitir con el
diagnóstico específico y los ocho controles deben producir ejecutable. Ningún
consumidor se ejecuta. Core se selecciona por `NSC_TEST_CORE_BINARY`; sin esa
configuración o sin temporales adecuados en A se omite con 77, mientras que
una ruta Core configurada pero inválida falla.

El sondeo equivalente, anterior a esta versión con aserciones, observó esos
16 resultados en O0. Comando, hashes y resultados están en
`A:/nsc-fixes-agent1-temp/shared-receiver-dispatch-observe-20261112.log`.
El compilador y Core provenían del build y SDK aislados con otros cambios en
curso; Core no se regeneró para este sondeo.

La regresión incorporada se ejecutó directamente desde `A:/nsc-devel-fixes`
con las cuatro variables temporales en A, `NSC_TEST_CORE_BINARY` apuntando a
`A:/nsc-fixes-evidencia/pointers-sdk-refresh/NativeSharpCore.lib`, el runtime
aislado de A y `NSC_LINKER=nslink.exe`. Pasaron los 16 análisis y las 16
comprobaciones de emisión O0, sin ejecutar consumidores. Comandos, diagnósticos
y hashes antes/después están en
`A:/nsc-fixes-agent1-temp/shared-receiver-dispatch-integrated-20261112.log`.
El compilador SHA-256 fue
`4726a3c676d17e1cf8d4d758230a526bdbcfa52aac61151b2d47f32c65e040d9`;
Core y metadata mantuvieron respectivamente los hashes
`f91d95c248f477531d38a093605f45343f6be9736d15e2097acd452db4bb54e0` y
`774db7b3a80d64cc594c1b05c448a8dcda7b7258abc729d09984df5d8478718c`.
Se comprobó también Core sin configurar (salida 77) y ruta Core inválida
(salida 1), con logs `shared-receiver-dispatch-skip-20261112.log` y
`shared-receiver-dispatch-invalid-core-20261112.log` en la misma carpeta. El
resultado corresponde al árbol de trabajo conjunto; no certifica revisión
limpia ni ejecución mediante CTest.

Solo dos cruces tienen confirmación individual en MIR: el constructor
publicador de Rc con llamada virtual contiene `virtual-instance-call` a
`Payload.Publish`, y el constructor publicador de Rc con interfaz pasa por
`Forward(this)` y contiene `interface-instance-call` dentro de `Forward`.
Las tablas correspondientes figuran en los volcados de
`A:/nsc-fixes-agent1-temp/nsc-receiver-dispatch-mir-yx134_yh/`; comandos y
hashes de los volcados están en `shared-receiver-dispatch-mir-20261112.log`.
Los otros 14 cruces no tienen MIR individual y no se atribuyen a una ruta
interna exacta. No se han probado destinos binarios sin cuerpo, tablas
incompletas, O2 ni ejecución de los controles seguros.

El registro CTest usa `python` por nombre, el compilador de la
configuración, `WORKING_DIRECTORY` del repositorio y `SKIP_RETURN_CODE 77`.
La invocación debe fijar `TEMP`, `TMP`, `APPDATA` y `LOCALAPPDATA` a carpetas
existentes en A desde el proceso inicial. Esta regresión no certifica todas
las rutas de publicación del receptor ni modifica interoperabilidad C/C++.
