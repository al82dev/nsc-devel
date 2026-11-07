# T08-004: validacion adicional de tipos C internos

La construccion incremental y los controles C/NativeSharp pasaron O0–O3 para
char, unsigned char, short, long, unsigned long, bool, enum, atomic int y dos
arreglos fijos de una y dos dimensiones. Se verificaron load y store atomicos
i32 seq_cst en el IR de NSC. Pasaron las regresiones de identidad, TLS y
agregados estaticos. La representacion bool se ejercito con 0 y 1, sin una
comparacion especifica de su IR con Clang.

El fixture se agrega al gate de la tanda, no ejecutado por completo. Este
resultado no cierra T08-004 ni demuestra soporte de todas sus variantes.
