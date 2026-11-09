# T08-D04b: copia adaptada de una DLL C con TLS estático y sin directorio de importación

Fecha: 2026-11-09. Decisión del usuario (punto 2): autorizar que NSC genere una copia adaptada de la DLL, con
activación explícita por referencia. Se mantienen estas condiciones:

- no modificar el original;
- rechazar la transformación de DLL firmadas;
- completar antes las comprobaciones pendientes (firma, `CheckSum`, límites de secciones);
- documentar el cambio de hash y sus efectos.

## Problema

El cargador de Windows inicializa el TLS estático de una DLL (asigna `_tls_index` y copia la plantilla) solo si la
imagen tiene directorio de importación. Basta con la tabla que contiene únicamente el descriptor nulo. Sin
directorio, el código de la DLL usa el índice 0, que es el bloque TLS del ejecutable
([T08-D04](t08-d04-tls-sin-importaciones.md)). T08-D04a rechaza esa DLL con la causa, y ese rechazo se conserva
sin la activación explícita.

## Uso

```xml
<CppLibraryReference Header="api.h" Language="C" Library="api.lib" Runtime="api.dll"
                     RuntimeTls="ImportDirectory" />
```

- `RuntimeTls` solo admite `ImportDirectory` («CppLibraryReference RuntimeTls must be ImportDirectory»).
- Exige `Runtime` («RuntimeTls requires a Runtime»).
- Sin el atributo, el comportamiento es el de T08-D04a. El mensaje de rechazo nombra ahora esta opción.

## Transformación

`src/Compilation/Internal/RuntimeTlsImportDirectory.h` (nuevo), `deriveRuntimeWithEmptyImportDirectory`. Es una
función pura de bytes a bytes.

**Condiciones.** Cada una se rechaza con su causa:

| Condición | Mensaje (resumen) |
|---|---|
| imagen PE, AMD64 PE32+ y DLL | «is not a PE image», «is not an AMD64 PE32+ image», «is not a DLL» |
| `DataDirectory[4]` (seguridad, Authenticode) en cero | «is signed (Authenticode)…» |
| sin directorio de importación | «already has an import directory: RuntimeTls="ImportDirectory" is not needed» |
| con directorio TLS | «has no static TLS directory: RuntimeTls is not needed» |
| una sección `CNT_INITIALIZED_DATA` y `MEM_READ`, sin `MEM_WRITE` ni `MEM_EXECUTE`, con 20 bytes cero tras su contenido | «has no read-only initialized data section with 20 zero bytes…» |

**Límites de la sección elegida.** Se toma la primera que cumple, por orden de sección, para que el resultado sea
determinista. El descriptor se coloca en `VirtualSize` alineado a 4, y tiene que cumplir:

- cabe en `SizeOfRawData`;
- no alcanza la `VirtualAddress` de la siguiente sección ni `SizeOfImage`;
- los 20 bytes son cero en el archivo.

**Campos escritos.** Solo estos tres:

1. `VirtualSize` de la sección, ampliado para cubrir el descriptor;
2. `DataDirectory[1] = {rva, 20}`: una tabla con solo el descriptor nulo;
3. `CheckSum`. Se recalcula con el algoritmo PE si el original tenía uno; si era 0, queda en 0.

Después se comprueba que ningún otro byte cambió. `DataDirectory[12]` (IAT), las importaciones diferidas y las
enlazadas no se tocan. El tamaño del archivo no cambia. Exportaciones, código, datos, TLS y el registro RSDS (PDB)
son los mismos bytes.

**Copia** (`ProjectLoader.cpp`, `adaptRuntimeTlsImportDirectory`).

- Se escribe en `<proyecto>\.nsc\runtime-tls\<FNV-1a 64 del original>\<nombre>`. Primero va a un temporal y luego
  se renombra; si ya existe con los mismos bytes, no se reescribe.
- `CppLibraryReference::runtimePath` pasa a ser la copia y `originalRuntimePath` conserva la declarada.
- Desde ahí todo usa la copia: validación, empaquetado, caché de artefactos e identidad de dependencia binaria en
  NSMeta. El producto la distribuye con el nombre original (`api.dll`).
- **El original nunca se abre para escritura.** Si el producto se publicara en la carpeta del original,
  el empaquetado lo rechaza («runtime package destination already contains a different 'api.dll'») y el original
  queda intacto.

**Verificador PE** (`PeExecutableVerifier.cpp`). Rechazaba toda tabla de importación sin bibliotecas. Ahora la
admite en una DLL, que es la forma que carga Windows. En un ejecutable sigue siendo un error, porque un ejecutable
de NSC siempre importa bibliotecas.

## Efectos sobre la identidad y la integridad

- **Hash distinto.** La copia tiene otro SHA-256 que el original (y otro `CheckSum` si el original lo tenía). Todo
  lo que identifique la DLL por hash ve un archivo distinto del que publicó el proveedor:
  - listas de permitidos por hash de WDAC y AppLocker;
  - inventarios de software y SBOM con hash;
  - verificaciones del proveedor o del instalador.

  Hay que registrar el hash de la copia o pedir al proveedor una DLL con importaciones.
- **Firma.** No se transforman DLL firmadas: la firma Authenticode quedaría inválida. Una regla basada en
  firma o editor no puede admitir la copia, porque la copia no lleva firma.
- **NSMeta y caché.** Registran el hash de la copia. Un consumidor precompilado recibe la copia transportada y
  comprueba su identidad. Si se cambia el original, la copia cambia y la caché falla.
- **Depuración.** El PDB sigue correspondiendo: el registro RSDS no cambia.
- **Alcance.** Solo cambia el producto que declara el atributo. Otros proyectos que usen la misma DLL sin el
  atributo siguen recibiendo el rechazo de T08-D04a.

## Prueba nueva: `tls-sin-import-host/validar-copia-adaptada.py`

1. **Sin atributo:** se conserva el rechazo de T08-D04a.
2. **Directo, O0–O3:**
   - el consumidor lee el valor TLS inicial 7;
   - el original no cambia;
   - la copia difiere solo en los tres campos, y su directorio apunta a 20 bytes cero.
3. **`CheckSum`:** con un original construido con `/release`, el `CheckSum` se recalcula y coincide con el
   algoritmo PE; sin él, queda en 0.
4. **Rechazos con la causa, sin modificar ninguna DLL:** firmada, sin relleno cero, ya con importaciones, sin TLS,
   valor desconocido y `RuntimeTls` sin `Runtime`.
5. **Precompilado:**
   - publicar junto al original se rechaza y el original queda intacto;
   - publicado en otra carpeta, un consumidor sin encabezado usa la copia transportada y lee 7 (O0 y O2).
6. **Caché:** la segunda construcción acierta y se ejecuta. Si se cambia el original (su `TimeDateStamp`), la caché
   falla y el producto lleva la copia del original nuevo.

## Regresión

`claude\scripts\regresion-d04b-d05b.cmd` ejecuta:

- el conjunto de D04 (suites TLS, DLL, Unicode, empaquetado y precompilados);
- D01, D02 y D05;
- la prueba nueva.

Resultado: código 0 en todo el conjunto (`claude\logs\regresion-d04b-d05b.log`).
