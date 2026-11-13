# Enumeraciones C++ con bases adicionales en Win64

Se amplía la proyección de enumeraciones C++ y la validación de campos en registros transportados por la ruta SysV ya soportada, para `long`, `unsigned long`, `char8_t`, `char16_t` y `char32_t`, conforme al tipo, tamaño, alineación y signo observados por Clang en `x86_64-pc-windows-msvc`. Se conserva la identidad de la base en el catálogo NSMeta y se comprueba su correspondencia con el campo. El cambio no pretende establecer tamaños para otros destinos.

## Evidencia dirigida

Clang oficial 23.1.0 y NSC ejecutaron correctamente los cinco casos individuales. Cada fixture `pruebas-tanda-10-enum-cpp-{long,ulong,char8,char16,char32}` prueba valores extremos, copia y campo de registro en O0–O3. Los fixtures `pruebas-tanda-09-sysv-cpp-copia-enum-{long,ulong,char8,char16,char32}` comparan llamadas con bibliotecas estáticas y DLL en O0–O3; cada caso pasó ocho comparaciones en compilación directa y dieciséis consumidores precompilados, con y sin encabezado.

Las salidas directas se conservaron respectivamente en `A:/temp/nsc-t09-sysv-cpp-copia-relleno-dqfsx0du`, `7o4_vmqq`, `ysh2k1ly`, `fen8ytix` y `cwfz3r_2` (los cuatro últimos con el mismo prefijo). Los consumidores precompilados ejecutados desde los fixtures del repositorio registraron `A:/temp/nsc-t09-sysv-cpp-copia-relleno-precompilado-ejcfdcfl`, `ad6w_4r3`, `hp972yys`, `cejkwnap` y `v9r4jmev` (mismo prefijo). El catálogo e identidad, transporte y plan ABI pasaron sus pruebas dirigidas; también pasó la regresión C y el rechazo conservado de bases aún no admitidas. La compilación incremental terminó correctamente después de liberar el ejecutable que las pruebas mantenían abierto.

## Límites

La evidencia de ejecución cubre las formas, valores y destino presentes en los fixtures, no todas las declaraciones o ABI de C++. La prueba sintética del catálogo cubre además la conservación de las variantes con ámbito; no implica ejecución general de esas variantes. Se mantienen abiertos T09-003, T10-002 y los demás ítems que aún requieren casos y validación.
