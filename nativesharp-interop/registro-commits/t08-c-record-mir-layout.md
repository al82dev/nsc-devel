# Tanda 08: layout MIR de records C

MIR incorpora el tamaño, alineación y offsets validados de records C completos,
con clasificación Win64 por tamaño y sin destructores C++ inventados. Los tipos
incompletos y los que solo tienen layout para acceso por puntero se omiten.
Pendiente de verificar el ABI directo e indirecto con Clang en la matriz real.
