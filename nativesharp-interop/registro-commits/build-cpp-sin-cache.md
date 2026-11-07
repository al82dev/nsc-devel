# Compilación de proyectos C/C++ sin caché de artefactos

El comando `nsc build` calculaba incondicionalmente la identidad del bundle,
que rechaza referencias C/C++ porque todavía no registra los headers
transitivos. Los comandos de emisión ya admiten estas referencias con
`--no-artifact-cache`. El comando de proyecto usa esa ruta para grafos con
referencias C/C++ y omite la identidad del bundle solo en ese caso. Mantiene
intacta la barrera de `calculateIdentity`, rechaza `--dependency-report`
hasta poder informar el cierre de headers y preserva la ruta anterior para
proyectos sin referencias C/C++.

Pendiente construcción y pruebas agrupadas, incluidas modificaciones de un
header transitivo y la matriz TLS de proyecto. Esto no habilita caché C/C++.
