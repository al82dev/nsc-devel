# Tanda 08: proyección inicial de valores record C

Las funciones de enlace C pueden proyectar records por valor cuando la definición
es única, el tipo C está instalado y su tamaño y alineación coinciden con Clang.
Las definiciones incompletas, múltiples no verificadas y records C++ siguen
rechazados. El backend ya clasifica agregados Win64 por tamaño, pero el recorrido
de llamadas y metadata aún debe construirse y comprobarse contra Clang.
