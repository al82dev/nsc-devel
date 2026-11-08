# T08 H3: control del ámbito foreach

Se agrega un consumidor de una plantilla pública cuya variable de `foreach`
oculta la global C `shadow_value`. El validador comprueba el resultado y que la
metadata del productor no capture la global. Con NSC build `1965527e723a8dbb`,
el diagnóstico agrupado terminó con código 0 en O0–O3. Es una muestra con un
arreglo y no demuestra todas las variantes de foreach ni cierra la tanda.
