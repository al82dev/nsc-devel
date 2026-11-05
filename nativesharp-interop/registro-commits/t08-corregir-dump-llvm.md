# Tanda 08: correccion del volcado LLVM

La primera validacion rechazo `--no-artifact-cache` con el comando de volcado,
que no admite esa opcion. Se elimina solo de esa invocacion; la emision del
ejecutable conserva la configuracion previa. Falta repetir la validacion.
