# Argumentos constantes en llamadas genéricas

La prueba de paquetes fallaba al analizar `Values<Ns...>()`; la de constantes
también interpretaba `Number<Mode.Answer>()` como otra expresión y rechazaba
el identificador `Number`. Ambos fallos se reprodujeron en la revisión base.

El reconocimiento de argumentos de llamada admite ahora un identificador
seguido de expansión de paquete, y un nombre calificado que comienza por un
tipo conocido. Sigue exigiendo el cierre de la lista y un paréntesis de
llamada. La prueba de constantes conserva un control de comparaciones
ordinarias y verifica identidad y rechazo de valores de otro enum.

Con la prueba reenlazada al frontend cambiado, `nsc.ns3.t04.generic-packs`
pasa sus diez comprobaciones. `nsc.ns3.t04.const-generics` supera las
comprobaciones de enum y comparación; su expectativa independiente de matriz
se analiza en `const-matrix-test.md`. Registros `probe-generic-packs.log` y
`probe-const-generics.log` en `A:/nsc-fixes-evidencia`.
