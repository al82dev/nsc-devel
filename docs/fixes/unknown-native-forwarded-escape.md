# Escape desconocido reenviado por código fuente

Después de `295f19e94`, una llamada directa a una biblioteca NativeSharp con
contrato de escape `Unknown` rechaza una dirección local. Sin embargo, si un
método fuente recibe el puntero y lo reenvía a esa biblioteca, el llamador
podía pasarle una dirección local (`A:/nsc-fixes-evidencia/unknown-native-forwarded-before.log`).
El consumidor solo se analizó; no se ejecutó el acceso potencialmente inválido.

La emisión de una biblioteca intermediaria confirmó que su resumen sí queda
`Unknown` (`unknown-native-forwarded-summary2.log`); el problema estaba en la
validación, que limitaba el rechazo al destino inmediato precompilado. Una
primera inspección con `ProjectCompilation` no cargó la biblioteca referenciada
(`unknown-native-forwarded-summary.log`) y no se usa como evidencia del resumen.

El análisis conserva una marca interna por parámetro, iniciada en contratos
desconocidos de bibliotecas NativeSharp y propagada por las dependencias de
escape existentes. Si la marca alcanza un intermediario cuyo resumen es
`Unknown`, pasarle almacenamiento del marco se rechaza. La marca no se publica
ni modifica el formato de metadata; no cambia la regla de imports C directos.
Una biblioteca intermediaria precompilada sigue tratándose por su contrato
publicado. El resultado es conservador: un intermediario legítimo sin garantía
conocida puede requerir un contrato acreditado antes de recibir esa dirección.

La prueba `nsc.ns3.t06.escape-summaries` pasó 20 comprobaciones después del
cambio (`unknown-native-forwarded-expanded.log`): llamada reenviada a un método,
dos intermediarios hacia una función, resumen publicado `Unknown`, almacenamiento
global y controles anteriores. También pasaron la selección de punteros
(`unknown-native-forwarded-pointers.log`), las lecturas con Core binario
(`unknown-native-forwarded-binary.log`) y dos controles de duración y escape
(`unknown-native-forwarded-controls.log`). Todos los registros están bajo
`A:/nsc-fixes-evidencia`.

La compilación fue incremental; el cambio de cabecera compartida provocó 208
acciones, incluida la recompilación de `CHeaderGenerator.cpp` como dependencia
del compilador aislado. No se modificaron fuentes ni carpetas de build de
interoperabilidad. El compilador y Core usados contienen otros cambios locales,
por lo que la evidencia no certifica un árbol limpio. La corrección no acredita
contratos antiguos incorrectos `NoEscape` ni firmas binarias con vistas
compuestas; esos casos siguen abiertos.
