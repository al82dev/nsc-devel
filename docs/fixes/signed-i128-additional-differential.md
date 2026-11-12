# División i128 firmada: comparación dirigida adicional

El caso 49 del informe solicita división `i128` con signo. La reproducción
original de `i128_division_link/Program.ns` permanece intacta. La corrección y
las pruebas originales están documentadas en `signed-i128-division.md`.

Se compararon 111 divisiones firmadas con un cálculo Python independiente que
trunca el cociente hacia cero. La muestra determinista utiliza semilla 493107,
incluye extremos, signos y divisores de distintos tamaños hasta 125 bits. En O0
y O2 el proyecto generado compiló y el ejecutable terminó con código 0, lo
que indica que todas las comparaciones de esa ejecución coincidieron. Código
generador, fuente, operandos, comandos y resultados están en
`A:/nsc-fixes-agente-2-evidencia/i128-differential.py` y
`A:/nsc-fixes-agente-2-evidencia/signed-i128-extra-differential/`.

El compilador utilizado fue `generic-indexer-closure-checked/nsc.exe`, SHA-256
`546846c0bb72f2c8827f248da4cb3fe53389d2677011bb00ebdec207c70e5296`. Es un
ejecutable híbrido enlazado con bibliotecas copiadas del build de campaña, no
un build limpio de la rama. Esta muestra no prueba todos los operandos, el
resto ni las operaciones sin signo y no sustituye la validación integrada.
