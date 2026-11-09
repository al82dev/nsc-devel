# T08-D04b y T08-D05b: soluciones preparadas, pendientes de autorización

Fecha: 2026-11-09. Instrucción del usuario: preparar y revisar las soluciones concretas, sus pruebas y sus efectos,
sin modificar ni distribuir copias alteradas de DLL sin su autorización.

En esta tanda no se implementó ninguna de las dos. Este documento deja el cambio listo para implementarse cuando
se decida. El comportamiento vigente es el de [D04](t08-d04-tls-sin-importaciones.md) (diagnóstico) y el de
[D05](t08-d05-dll-homonimas-transitivas.md) (rechazo con la causa).

## T08-D04b: DLL con TLS estático y sin directorio de importación

### Problema y estado

Windows no inicializa el TLS estático de una DLL sin directorio de importación: recibe `_tls_index = 0` y usa el
bloque TLS del ejecutable. NSC rechaza hoy esa DLL al consumirla (D04a).

### Solución propuesta

Una copia distribuida con un directorio de importación que solo contiene el descriptor nulo. Se activa
explícitamente por referencia:

```xml
<CppLibraryReference Header="api.h" Library="api.lib" Runtime="api.dll" RuntimeTls="ImportDirectory" />
```

### Revisión del prototipo (`tls-dll-distintas/inspeccionar-directorio-importacion.py`)

El prototipo demuestra el efecto: la copia lee 7 y la original lee `0x80000000`. Pero le faltan condiciones que la
implementación sí necesita:

| Falta en el prototipo | Necesario en la implementación |
|---|---|
| exige una sección llamada `.rdata` | cualquier sección con `CNT_INITIALIZED_DATA` y `MEM_READ`, sin `MEM_WRITE` ni `MEM_EXECUTE`; se elige la primera por orden de sección, para que el resultado sea determinista |
| solo comprueba `offset + 20 <= SizeOfRawData` | además, que `VirtualAddress + offset + 20` no alcance la siguiente sección y que `offset` esté alineado a 4 |
| no recalcula `CheckSum` | recalcularlo (algoritmo PE estándar), o dejarlo en 0 si el original es 0 |
| no mira la firma | rechazar si `DataDirectory[4]` (seguridad) no es cero |
| solo PE32+ | igual: rechazar todo lo que no sea x64 PE32+ |
| no compara antes y después | comprobar que solo cambian `VirtualSize` de esa sección, `DataDirectory[1]` y `CheckSum` |

`DataDirectory[12]` (IAT), las importaciones diferidas y las enlazadas siguen en cero o sin cambios.

### Implementación prevista

| Pieza | Archivo | Cambio |
|---|---|---|
| Propiedad | `ProjectLoader.cpp`, descriptor de `CppLibraryReference` | `RuntimeTls="ImportDirectory"`; cualquier otro valor se rechaza |
| Transformación | nuevo `src/Linker/PeEmptyImportDirectory.{h,cpp}` | función pura de bytes a bytes con las condiciones de arriba; devuelve el error exacto |
| Diagnóstico | `validateCppImportRuntimes` | con la propiedad, en lugar de rechazar se marca la DLL como derivable; sin ella, el diagnóstico de D04a no cambia |
| Empaquetado | `RuntimeClosurePackaging.cpp` | la copia derivada se escribe en el directorio del producto; la original nunca se toca |
| Caché | `CompilationInputManifest` | la clave incluye el hash del original y la regla de derivación (`empty-import-directory/1`); la salida registra el hash derivado |
| NSMeta | identidad de dependencia binaria | registrar hash original, regla y hash derivado. El consumidor de una plantilla precompilada repite la derivación y compara. Es contenido del contrato vigente, no una versión nueva |

### Pruebas previstas (`tls-sin-import-host/validar-copia-derivada.py`)

1. Con la propiedad, en O0–O3: el consumidor lee 7, el producto lleva la copia y el original no cambia (hash antes
   y después).
2. La copia difiere del original solo en los tres campos; las exportaciones y el registro RSDS son iguales.
3. Sin la propiedad: el diagnóstico de D04a no cambia.
4. Rechazos: DLL firmada, sin espacio cero, PE32, y DLL que ya tiene directorio de importación (la propiedad sobra:
   aviso o error, por decidir).
5. Precompilado: el productor registra la derivación; el consumidor sin encabezado obtiene la misma copia. Si se
   sustituye el original, falla por identidad.
6. Caché: un acierto restaura la copia; cambiar el original produce un fallo de caché.

### Efectos

Ya presentados en D04:

- cambia el hash de la copia;
- no se admiten DLL firmadas;
- las listas blancas por hash (WDAC, AppLocker) verán un hash distinto;
- el PDB sigue correspondiendo;
- solo afecta al producto que lleva la copia.

### Decisión pendiente del usuario

Autorizar que NSC escriba esa copia alterada. Si no se autoriza, el diagnóstico de D04a queda como comportamiento
definitivo y se pide al proveedor que recompile con alguna importación.

## T08-D05b: alias TLS importado directamente que necesita aislar sus dependencias

### Problema y estado

Una función exclusiva de una de las `api.dll` homónimas se importa por la tabla de importación desde el alias
`__nsc_tls_<hash>.dll`. El cargador resuelve esa tabla antes de que el código pueda activar un contexto, así que la
`dep.dll` homónima del alias no se puede aislar. El empaquetado lo rechaza con «… is imported directly, so its
dependencies cannot be isolated» (`RuntimeClosurePackaging.cpp`).

### Solución A: enlace en ejecución (no modifica ninguna DLL)

**Cambio en HIR.** En `HirBuildCoordinator.cpp` (`buildNamespaceFunction`), hoy se activa
`importRequiresRuntimeBinding` solo si el mismo nombre existe en dos DLL homónimas distintas (`distinctDlls`). La
condición se amplía a toda función C cuya DLL tenga alias TLS (`requiresTlsAliases` evaluado sobre la identidad de
su DLL, sin exigir `distinctDlls`). La llamada pasa por `__nsc_tls_import_*`, que resuelve con el cargador de alias
(`__nsc_cpp_dll_tls_export`), y este ya activa el contexto si existe `<alias>.nsc-isolation.manifest`.

**Cambio en el driver.** `appendCppTlsAliasedFunctionImports` deja de añadir importaciones directas para esas
funciones. El rechazo de `RuntimeClosurePackaging.cpp` se conserva para alias que sigan en la tabla de importación
por otra vía.

**Alcance.**

- *Amplio:* todas las funciones de DLL con alias. Es simple y uniforme, pero cambia los productos de D05a que
  hoy funcionan sin aislamiento.
- *Estricto:* solo cuando hace falta aislamiento. HIR tendría que conocer los conflictos de dependencias que hoy
  calcula el driver después de enlazar; exige adelantar ese análisis de cierres al driver antes de HIR.

Recomendación: el alcance estricto, para no cambiar los productos de D05a sin necesidad.

**Efectos.**

- La DLL alias se carga al primer uso, no al iniciar el proceso. Su `DllMain` y la inicialización TLS ocurren más
  tarde, en el hilo que llama primero.
- Una DLL ausente falla en la primera llamada (trampa de enlace), no al cargar el ejecutable.
- Cada llamada pasa por un puntero resuelto una vez (una indirección).
- La tabla de importación del producto deja de nombrar el alias.

**Pruebas previstas** (se amplía `validar-transitivas-homonimas.py`):

1. La función exclusiva de `api.dll` (1) da 101 y la de (2) da 102, en O0–O3, en ejecutable y en producto DLL.
2. La tabla de importación del producto no contiene el alias.
3. Sin el manifiesto, la primera llamada falla con la trampa; no se usa otra `dep.dll`.
4. Caché: un acierto restaura alias, ensamblados y manifiestos.
5. Con alcance estricto: un producto D05a sin conflicto conserva la importación directa y el mismo binario.

### Solución B: manifiesto embebido en el alias

Un manifiesto de recurso ID 2 (`ISOLATIONAWARE_MANIFEST_RESOURCE_ID`) dentro de la DLL alias haría que el cargador
resolviera sus importaciones en su propio contexto. Modifica la DLL, igual que D04b, y requiere la misma
autorización. Las firmas y los hashes cambian. No se recomienda frente a la A.

### Decisión pendiente del usuario

1. ¿Se aplica la solución A, aunque no altera DLL?
2. Si se aplica, ¿con qué alcance: estricto (recomendado) o amplio?

Mientras no se decida, el rechazo con la causa queda como está.
