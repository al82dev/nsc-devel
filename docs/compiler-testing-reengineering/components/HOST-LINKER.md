# Host.Linker — COFF/PE/PDB, NSLink y productos Windows

## 1. Estado y procedencia

- **Clasificación:** `IMPLEMENTACIÓN VIGENTE`, salvo hallazgos marcados.
- **Repositorio:** `A:\nsc-devel`.
- **Revisión congelada:** `3bb175443a18b2049c20d75443dd6743e2993903`.
- **Universo:** 16 archivos, 4,362 líneas y 160,519 bytes.
- **Inventario:** 132 símbolos físicos, 40 contratos y 320 casos diseñados.
- **Validación realizada:** lectura completa de headers, índice AST de ocho implementaciones, extracción de
  firmas/diagnósticos/constantes y revisión dirigida de verificadores, procesos, builders, imports y runtime.
- **No realizado:** build, ejecución, baseline, cobertura, instalación, despliegue o modificación del producto.

## 2. Frontera del componente

Host.Linker toma objetos COFF ya emitidos por Host.LLVM y produce/verifica archivos Windows. Sus fronteras son:

1. verificar archives COFF, imágenes PE32+ y PDB MSF 7;
2. descubrir NSLink y `llvm-lib`, crear response files y ejecutar procesos;
3. construir ejecutables, DLL, import libraries y static libraries;
4. materializar imports del runtime y de `extern C from`;
5. localizar, verificar y generar `NativeSharp.Runtime.Win32.lib`.

No decide semántica de lenguaje ni genera MIR. Rechaza objetos, librerías y rutas inválidas antes del proceso y
vuelve a verificar cada producto publicado.

## 3. Verificación de archives COFF

`CoffArchiveVerifier` exige magic `!<arch>\n`, headers de miembro de 60 bytes y terminador válido. Interpreta el
primer linker member en big-endian, limita su inspección a 256 MiB y el catálogo a 16,777,216 símbolos. Valida
offsets, tamaños, strings NUL y padding par. Publica número de miembros y nombres; reinicia estado y error en cada
llamada.

## 4. Selección de NSLink y librarian

`LldLinker::discover` busca NSLink junto a `nsc.exe`, mediante `NSC_LINKER` y en `PATH`; no convierte lld-link en
fallback implícito. El path no puede contener comillas. Para archives, `librarianExecutablePath` resuelve
`NSC_LLVM_LIB` o `llvm-lib` compatible; cuando NSLink fue seleccionado y no existe librarian, la creación
estática falla explícitamente.

La identidad del producto cambia los diagnósticos entre NSLink, lld-link y librarian, pero el builder consume
una sola interfaz.

## 5. Response files y proceso hijo

Los argumentos se escriben en response files con quoting controlado y paths UTF-8. `runResponseFile` redirige
stdin desde NUL y stdout/stderr a un log, crea el proceso sin shell, espera, obtiene exit status y adjunta el log
al diagnóstico. En rutas Windows usa handles con cierre determinista; en la ruta portable interpreta el status
de `system`.

`NSC_KEEP_LINK_TEMPS` conserva response files/logs. En modo normal se limpian después de copiar la evidencia
necesaria. Las comillas embebidas en paths se rechazan antes de construir la línea.

## 6. Operaciones del linker

La generación de import library exige DLL y símbolos, valida caracteres, escribe `.def` y comprueba que el
archive exista. Static library exige al menos un objeto COFF existente y usa librarian. DLL agrega objetos,
imports, exports, `/dll`, `/implib` y PDB opcional. EXE selecciona `/subsystem:console` o `/subsystem:windows`,
entry y PDB opcional. Ninguna operación acepta outputs vacíos ni reporta éxito si el producto falta.

## 7. Verificación PDB

`PdbVerifier` analiza MSF 7.00 con límite de 1 GiB. Comprueba block size, superblock, block map, directory,
stream counts y rangos. Del information stream extrae version, signature, age y GUID. Del DBI detecta module
records, public symbols, source files y subsecciones de checksum. Todos los accesos pasan por límites explícitos;
streams ausentes o truncados fallan en su frontera.

## 8. Verificación PE32+

`PeExecutableVerifier` valida MZ, `PE\0\0`, AMD64, optional header PE32+, tablas y secciones. Convierte RVA a
offset sólo cuando cae en datos respaldados por archivo. Distingue EXE de DLL por characteristics y subsistema.

Extrae exports, imports por DLL/símbolo, exception directory, CLR directory y debug directory. Para CodeView
RSDS conserva GUID, age y nombre PDB. Strings, thunks, RVAs y tamaños inválidos se rechazan, no se truncan.

## 9. Construcción de ejecutables

`WindowsExecutableBuilder` puede recibir un módulo LLVM o objetos preconstruidos. La ruta de módulo crea target
x64, runtime, optimiza, emite y verifica programa/runtime. La ruta particionada exige al menos un objeto y puede
omitir reverificación sólo cuando el productor ya los marcó verificados.

Después materializa import libraries, agrega librerías externas y runtime configurado, invoca el linker y
verifica el PE final. El estado observable incluye `PeExecutableInfo`, subsystem, debug, incremental reuse y
configuración de inputs.

## 10. Enlace incremental de ejecutables

La key incremental incorpora hashes/tamaños de objetos y librerías, IR del programa/runtime, imports externos,
optimización, debug, subsystem y entry. El estado se publica mediante archivo temporal y rename. Sólo se reutiliza
si key y productos existentes coinciden; un cache miss continúa por la ruta completa. Un error de identidad o
publicación no se disfraza como miss.

## 11. Construcción de bibliotecas

`WindowsLibraryBuilder` ofrece rutas de módulo y objetos para archives y DLL. Static library incorpora programa
y soporte runtime; con objetos particionados puede restaurar/storear archive y objeto runtime mediante el cache
de Host.LLVM. Dependencias estáticas privadas se aplanan en el archive final.

La ruta DLL emite runtime y anchor, crea imports, agrega librerías extra, enlaza DLL/import library/PDB, verifica
PE y archive y comprueba cada export solicitado. Ante fallo elimina productos parciales.

Los flags de telemetría distinguen intento/hit de cache del archive y del runtime; no equivalen a éxito global.

## 12. Import libraries de Windows

`createWindowsImportLibraries` es la fuente única para tablas runtime y `extern C from`. Los nombres DLL se
normalizan case-insensitive, los símbolos se deduplican y módulos externos que coinciden con runtime se fusionan.
El IR combinado activa imports de soporte condicionales como memoria y SEH. Cada DLL planificada produce una
import library mediante `LldLinker`; el error conserva la DLL causal.

La implementación contiene una estructura local `PlannedLibrary`; su orden canónico evita que el orden de las
declaraciones externas cambie la línea de enlace.

## 13. Runtime Windows estático

El producto canónico es `NativeSharp.Runtime.Win32.lib`, formato 1. La resolución prioriza
`NSC_WINDOWS_RUNTIME_LIBRARY`, después la ruta relativa al ejecutable y finalmente el default configurado.

La verificación abre el archive, prohíbe que posea `__nativesharp_start` y exige todos los símbolos ABI del
runtime. La generación construye un runtime independiente, target x64, optimización, objeto COFF verificado y
archive verificado; finalmente publica por rename. Los temporales tienen ownership RAII.

## 14. Temporales, publicación y limpieza

Los builders crean directorios únicos bajo temp o bajo `NSC_KEEP_LINK_TEMPS`. La limpieza normal es RAII y no
debe borrar el producto. Outputs parciales de DLL/EXE/LIB se eliminan cuando falla la verificación. Archivos de
estado y runtime usan publicación temporal para evitar resultados a medias.

## 15. Dependencias

- `Host.LLVM`: target machine, object emission, optimization y artifact cache.
- `Host.Runtime`: construcción y catálogo ABI del runtime Windows.
- `Host.Basic`: SHA-256 e identidad de archivos.
- NSLink/`lld-link` y `llvm-lib`: procesos externos seleccionados explícitamente.
- Windows API/filesystem: proceso, handles, PE/COFF/PDB y publicación.

## 16. Invariantes observables

1. Un producto se reporta sólo después de existir y pasar su verificador.
2. NSLink es el linker de usuario descubierto; lld-link no aparece como fallback silencioso.
3. Las entradas del proceso se validan antes de crear response file.
4. EXE, DLL, import library, static library y runtime tienen verificadores apropiados.
5. Cache e incremental reuse dependen de identidades completas, no sólo de existencia.
6. Imports runtime/externos se fusionan de forma case-insensitive y determinista.
7. Fallos no dejan productos parciales presentados como válidos.

## 17. Riesgos y hallazgos conservados

1. El nombre `LldLinker` es histórico aunque la política vigente exige NSLink para productos NativeSharp.
2. Existe soporte explícito para lld-link en la interfaz; debe probarse sólo cuando el caller lo selecciona.
3. La ruta portable de proceso usa `system`, mientras Windows usa `CreateProcessW` y handles.
4. Los parsers COFF/PE/PDB son verificadores dirigidos, no implementaciones completas de los formatos.
5. Imports runtime permanecen como catálogo estático grande, aunque ya existe una sola fuente de verdad.
6. Los destructores de temporales ignoran errores de limpieza por diseño vigente.

No se corrigió ninguno de estos puntos en esta fase.

## 18. Estrategia futura de pruebas

Los formatos usarán fixtures binarios construidos por builders de test con mutaciones de campo/rango, no blobs
sin procedencia. Los procesos usarán ejecutables controlados que registren argumentos, cwd, stdin, log y exit.
Los builders se probarán con objetos COFF mínimos y dobles explícitos para cache/runtime/linker; la integración
real con NSLink quedará separada de unitarias deterministas.

Cada contrato recibe nominal mínimo, representativo, prerequisito ausente, estructura inválida, límites,
variantes, determinismo y escala. La última matriz es `NIGHTLY`.

## 19. Estado de cierre

Los 16 archivos y 132 símbolos están trazados a 40 contratos con 320 casos diseñados. `Host.Linker` queda
`AUDITADO` y `RECONSTRUIBLE` para la revisión congelada. Esto no afirma ejecución ni cobertura.
