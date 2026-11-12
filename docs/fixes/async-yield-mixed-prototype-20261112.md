# Experimento de descarte condicional durante cancelación

El diff experimental está conservado en `async-yield-mixed-prototype-20261112.patch`;
no permanece aplicado en la rama. Se limitó a una copia de cleanup cancelado,
sin suspensión, con un condicional cuya rama verdadera era llamada directa a una
fábrica cuyo cuerpo MIR retornaba una construcción heap owned y cuya rama falsa
leía un campo estático. Se emitió la condición una vez, y cada rama evaluó y
descartó su resultado en su propio bloque; solo la rama propietaria destruyó.

No es una solución general: la prueba depende de la forma del cuerpo de la
fábrica, no admite ramas invertidas, anidadas, conversiones u otros productores,
deja sin resolver los casos no coincidentes y no establece semántica ante
suspensión, abandono o excepciones. El código no fue integrado.

`async-yield-mixed-identity-prototype.log` registra 42 en O0/O2 para las dos
ramas sin excepción. La condición con efecto se evaluó una vez; los IDs y la
traza verificaron que se destruyó el objeto temporal ID 2 en la rama propietaria
y no se destruyó el objeto compartido ID 1 en la rama prestada. La rama que
lanza siguió terminando con `0xe04e5301` en O0/O2, en vez de alcanzar el catch.
Los controles normales de nueve formas dieron 42 en 18 ejecuciones y los de
Cancel+Drop dieron 42 en 18; el caso nested independiente continuó con 22.

El NSC anterior tenía SHA-256
`7df4b193b39ac3f176b829b87184eaa38360c35345c1fb7ee1e08318557a73fd`;
el experimental, `f814635e3572f2861bae103ab10c365aa43c42abff0a61252bcedd88215da834`.
Después de invertir solo el diff experimental y reconstruir incrementalmente,
el NSC restaurado tuvo SHA-256
`5cc75d23c719b89d5caad0eb62d3011bbb4fe4d2b7e5e11d44fe89ee5878e0cc`;
el cambio de hash incluye la identidad de build generada. El build de
restauración terminó con salida 0; Cancel+Drop volvió a pasar 18 ejecuciones,
y los casos nested/mixto volvieron a fallar como antes. Logs con prefijo
`async-yield-*prototype*` y `*restored.log` conservan los resultados.

Para una solución general, `completeScheduledCoroutineValueStatementFrame`
conserva el árbol del operando y conoce `frame.memberOrdinal`. Es una frontera
candidata para consultar `expressionTypeBinding(syntax, ordinal)` y transportar
la propiedad del resultado y de cada rama relevante al MIR. En el sondeo las
especializaciones de `Generator<int>` y `Generator<Payload>` obtuvieron sus
bindings concretos por ordinal; eso no demuestra todas las especializaciones.
La expresión mixta tiene ramas con distinta propiedad: `lowerConditional` une
solamente el valor, sin una bandera de propiedad. Toda solución tendrá que
conservar esa distinción sin evaluar de nuevo la condición y sin destruir el
alias; wrappers, otras formas de selección, excepciones y suspensión siguen
sin demostrarse. Esta sección describe una dirección de investigación, no
una modificación implementada.
