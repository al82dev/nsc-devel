# Runtime: rutas no ASCII en borrar, copiar, mover y directorios

Ficha NSC-NEW-020 (parcial).

## Defecto corregido

`File.Delete`, `File.Copy`, `File.Move`, `Directory.CreateDirectory`, `Directory.Move` y
`Directory.Delete` pasaban la ruta UTF-8 a las variantes `A` de Win32 (`DeleteFileA`, `CopyFileA`,
`MoveFileA`, `CreateDirectoryA`, `RemoveDirectoryA`). Con una ruta no ASCII (`sub ñ`) la página de
códigos ANSI nombraba otro archivo. Ahora el runtime convierte la ruta a UTF-16 con
`buildUtf16NullTerminatedCopy`, como ya hacía `File.Exists`, y llama `DeleteFileW`, `CopyFileW`,
`MoveFileW`, `CreateDirectoryW` y `RemoveDirectoryW`. Los nombres `W` se añaden a todas las listas
de importación del runtime (`WindowsRuntimeImports.cpp`, `sdk/win64/nativesharp-runtime.nsimports` y
las tres verificaciones de `cmake/`); los `A` se conservan para las bibliotecas de importación
existentes.

## Prueba

`nsc.runtime.unicode-file-paths` copia, mueve y borra un archivo, y crea, mueve y borra un
directorio con nombres no ASCII, en O0 y O2, y comprueba que no queden restos.

## Pendiente

El directorio actual, las variables de entorno y `Process.Run` siguen con variantes `A`: devuelven o
reciben texto además de rutas y necesitan la conversión inversa (UTF-16 a UTF-8).
