# Tanda 08: prueba de diagnostico de tipo incompleto

La validacion rechazo correctamente sizeof y alignof de un record incompleto,
pero la prueba esperaba solo el texto anterior. Se incluye el diagnostico
especifico nuevo, manteniendo la exigencia de fallo. Falta repetir la suite.
