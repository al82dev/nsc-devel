# Tanda 08: identidad de enum C anónimo con typedef

La captura usa el typedef asociado por Clang para nombrar un enum C anónimo
tanto en su declaración como en las referencias a su tipo. Los enums anónimos
sin typedef conservan su identidad anónima. El cambio no demuestra todavía la
proyección de firmas o el ABI y no se ha compilado ni ejecutado.
