# Origen de funciones C++ en MIR y caché

La bajada y serialización de funciones conserva las marcas de origen C++ e
importación DLL. Las identidades de caché HIR y de objetos incluyen esas marcas
para distinguir compilaciones con información externa diferente.

El cambio registra código acumulado; la validación corresponde al conjunto.
