# Fallo en constructor anidado de fábrica compartida

`tests/integration/shared_throwing_nested_constructor_vistas.py` prueba las
fábricas públicas `MakeRcThrowing` y `MakeArcThrowing` del Core binario con un
payload de clase o estructura. El constructor inicializa un primer campo
`owned`, y después construye un segundo campo cuyo propio constructor inicializa
otro recurso y lanza. A diferencia del control de fábrica que lanza después de
terminar sus campos, esta reproducción falla dentro de la construcción del
segundo campo.

El programa exige capturar una excepción, destruir los recursos en orden 2, 1
y no ejecutar los destructores de los dos objetos cuyos constructores no
terminaron. Con `vistas-env.cmd`, el comando
`python tests/integration/shared_throwing_nested_constructor_vistas.py
A:/nsc-vistas-evidencia/build/bin/nsc.exe A:/nsc-vistas-evidencia/sdk/NativeSharpCore.lib`
terminó 0. Las ocho combinaciones Rc/Arc, clase/struct y O0/O2 compilaron y
retornaron 42. Log:
`A:/nsc-vistas-evidencia/shared-throwing-nested-constructor.log`; los comandos,
fuentes y hashes antes/después están en
`A:/nsc-vistas-evidencia/temp/shared-nested-constructor-4lqebsqy/results.json`.

Los SHA-256 permanecieron iguales: nsc
`875372ae0fa8e0e6f98f81eccdf6f6a7c197ccefa12bfae213ee13617381ace7`, Core
`565bf2177c85f88d94057ebe8616f2caeb71e01b7e39610bf815c15b685221c6` y
metadata `8213d5eca49219a8322137d2bba05916b71df8a700c9f2bfd04a7089d4ed62d9`.
Se usaron cwd del repositorio, temporales en A y `--no-artifact-cache`.

Esta prueba no observa directamente la liberación física del bloque de la
fábrica, no cubre excepciones dentro de destructores ni todos los layouts y
no resuelve el caso de campos compartidos que la fábrica rechaza, documentado
en `vistas-fabrica-cuatro-campos.md`. El compilador tenía trabajo local
concurrente; los resultados no certifican toda la rama.
