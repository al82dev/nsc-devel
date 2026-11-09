# T08-D04: DLL C con TLS estático y sin directorio de importación

Fecha: 2026-11-09. Casillas: T08-D04 y la decisión pendiente de T08-002 sobre el valor TLS inicial de una DLL sin
importaciones ([decisión anterior](t08-tls-sin-import-decision.md)).

## Estado de la investigación

Las pruebas previas (`tls-sin-import-host/validar.py`, `tls-dll-distintas/inspeccionar-directorio-importacion.py`)
reproducen el comportamiento sin NativeSharp, con un anfitrión Python (`ctypes`), en el hilo principal y en otro
hilo. Una DLL con directorio TLS y **sin directorio de importación** recibe `_tls_index = 0`. Su propio código lee
entonces el bloque TLS del ejecutable: lee `0x80000000` en lugar de 7 y, si escribe, corrompe el TLS del
ejecutable. La misma imagen con un directorio de importación que solo contiene el descriptor nulo (sin ninguna
dependencia) recibe un índice propio y lee 7. La condición operativa es, por lo tanto, la ausencia del directorio
de importación, no la falta de dependencias.

Esta tanda añadió una observación nueva: `validar-variadicas-agregados.py` usaba dos DLL con TLS y sin
importaciones. Pasaba porque sus funciones no leían el valor inicial, y `Access` escribía y leía mediante el
índice 0, es decir, en el bloque del ejecutable. Con el diagnóstico nuevo, NSC rechaza esa configuración.

No hay un camino del lado del consumidor que no modifique nada. El valor incorrecto lo produce el código de la
DLL con su `_tls_index`, y Windows no ofrece una API pública para pedir al cargador que procese el TLS de un
módulo ya cargado. Emular esa tarea (asignar un índice y ampliar `ThreadLocalStoragePointer` en cada hilo
existente y futuro) supondría manipular estructuras internas de ntdll, lo que es frágil y queda fuera del
contrato. Las opciones reales son las dos siguientes.

## Cambio implementado: diagnóstico al consumir

NSC ya no construye un producto que leería en silencio el TLS de otro módulo:

- `PeExecutableInfo` expone `tlsDirectoryRva` (directorio de datos 9).
- `validateCppImportRuntimes` inspecciona cada `Runtime` de las referencias C/C++, tanto directas como
  transportadas por un productor precompilado. Si la DLL es válida, tiene directorio TLS y no tiene directorio de
  importación, NSC la rechaza:

  ```
  runtime DLL '…/api.dll' has static thread-local storage but no import directory: the Windows loader does not
  initialize the static TLS of such a DLL, so its thread-local variables would use another module's storage;
  rebuild the DLL with an import directory (NSC does not modify the DLL)
  ```

- `validateLibraryReferenceRuntime` aplica la misma comprobación a las DLL NativeSharp referenciadas. Las DLL
  que produce NSC siempre importan el runtime, así que en la práctica no se activa.
- Una DLL ausente o que no es PE conserva sus diagnósticos anteriores.

El diagnóstico es **provisional**: hace visible la limitación, pero no da la capacidad. T08-D04 sigue abierta.

## Propuesta para completar la capacidad: copia distribuida con directorio de importación vacío

La capacidad completa requiere que la DLL distribuida junto al producto tenga directorio de importación. Según la
decisión 4, se presenta el cambio concreto y sus efectos **antes de implementarlo**:

**Activación.** Explícita, por referencia; nunca automática. Por ejemplo:

```xml
<CppLibraryReference Header="api.h" Library="api.lib" Runtime="api.dll" RuntimeTls="ImportDirectory" />
```

**Cambio en la copia.** El original nunca se modifica. NSC escribe una copia en el directorio del producto:

1. Busca 20 bytes a cero dentro de los datos en bruto de una sección inicializada de solo lectura (`.rdata`),
   detrás de su `VirtualSize` y sin superar `SizeOfRawData`. Si no hay espacio, rechaza; no añade secciones ni
   cambia la disposición.
2. Amplía el `VirtualSize` de esa sección en 20 bytes y apunta el directorio de importación (`DataDirectory[1]`)
   a ese descriptor nulo, con tamaño 20.
3. Recalcula `CheckSum` del encabezado opcional.

El prototipo de este cambio es `tls-dll-distintas/inspeccionar-directorio-importacion.py`, que lo aplica sobre
una copia.

**Efectos.**

- **Identidad.** Cambia el SHA-256 de la copia. La identidad de dependencia de NSMeta debe registrar el original y
  la copia derivada, con la regla de derivación. Hoy, si se sustituye la DLL transportada, falla
  `binary dependency identity mismatch`.
- **Firma.** Una firma Authenticode quedaría invalidada. NSC rechazaría la transformación si la DLL está firmada
  (`DataDirectory[4]` distinto de cero).
- **Sin cambios.** El código, las exportaciones, las relocalizaciones, el TLS y el registro RSDS no cambian, así
  que el PDB del proveedor sigue correspondiendo.
- **Carga.** El cargador asigna un índice TLS propio y copia la plantilla. Los valores iniciales y el TLS por hilo
  quedan correctos.
- **Alcance.** Solo afecta al producto que distribuye la copia. Otros consumidores del original siguen igual. La
  copia local de la aplicación tiene prioridad en el orden de búsqueda.
- **Integridad.** Las reglas de lista blanca por hash (WDAC, AppLocker) verán un hash distinto del publicado por
  el proveedor.

**Alternativa sin transformación.** Mantener el diagnóstico como comportamiento definitivo. El proveedor
recompila la DLL con cualquier importación del sistema, como ya hacen las DLL C con CRT.

## Pruebas

**Nueva:** `pruebas-tanda-08-identidad-c/tls-sin-import-host/validar-diagnostico.py`. Aprobada.

- **Directo.** La DLL sin importaciones se rechaza con la causa y sus bytes no cambian. La misma fuente con una
  importación Win32 se construye en O0–O3 y el consumidor lee el valor inicial 7.
- **Precompilado.**
  - Un productor construido contra la DLL sin importaciones se rechaza igual.
  - Construido contra la DLL con importación, se consume sin encabezado y lee 7 (O0 y O2).
  - Si se sustituye la DLL transportada por la versión sin importaciones, la identidad de dependencia la detecta
    antes.

**Actualizada:** `tls-dll-distintas/validar-variadicas-agregados.py`. Cada DLL importa `GetCurrentThreadId`, y el
consumidor comprueba primero el valor TLS inicial de cada DLL (3 y 7) con un método `Initial` nuevo. Antes, ese
valor era incorrecto y la prueba no lo observaba.

## Regresión

`A:\nsc-interop-build\claude\scripts\regresion-d04.cmd`:

- 14 suites Python de TLS y DLL: `tls-sin-import-host` (2), `tls-dll-distintas` (6), `tls-dll-producto`,
  `tls-dll-unicode` (2), `cache-runtime-transitivo`, `globales-c/validar-tls` y `validar-atomicas`;
- los validadores `.cmd` de `tls-c` (DLL, precompilado y lld), `atomic-global-dll`, `record-c/dll-bool` y
  `tls-dll-distintas/validar-empaquetado.cmd`.

Todo correcto tras actualizar `validar-variadicas-agregados.py`.

`tls-dll-distintas/validar-mismo-nombre.cmd` también es correcto si se invoca con su ruta absoluta. Con una ruta
relativa falla al compilar `ThreadOnly.c`: usa `%~dp0` después de `pushd`. Es un fallo previo del arnés, ajeno a
este cambio; ese validador no forma parte de la complementaria. `tls-dll-distintas/validar.cmd` necesita el
argumento `dos` y lo invoca `validar-mismo-nombre.cmd`.
