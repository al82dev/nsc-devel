# Tanda correctiva G00 — migración de caché LLVM estática

Fecha: 2026-10-01
Rama: `development`
Estado: corregida; pendiente del baseline integral de Alberto

## Fallo observado

Una configuración nueva de `nscbuilder.py` reutilizó `A:\nsc-devel\build-clang`, creado por la
etapa anterior que enlazaba `LLVM-C.dll`. Aunque el comando actual ya pasaba `LLVM_DIR`, la caché
conservaba:

```text
CMAKE_DISABLE_FIND_PACKAGE_LLVM=TRUE
NSC_LLVM_MONOLITHIC_LIBRARY=C:/clang-llvm/lib/LLVM-C.lib
```

CMake conserva opciones ausentes entre regeneraciones. Quitar los argumentos antiguos del comando
no equivalía a ponerlos en falso ni a retirar las entradas de caché, por lo que `find_package(LLVM)`
quedaba deshabilitado antes de poder usar `LLVM_DIR`.

## Corrección

- `CMakeLists.txt` fuerza en Windows `CMAKE_DISABLE_FIND_PACKAGE_LLVM=FALSE` y elimina de la caché
  `NSC_LLVM_MONOLITHIC_LIBRARY`. El contrato estático prevalece aunque se reutilice un árbol creado
  por NativeSharp 2.0 o por la primera configuración de 3.0.
- `nscbuilder.py` escribe explícitamente `-DCMAKE_DISABLE_FIND_PACKAGE_LLVM=FALSE` y añade
  `-UNSC_LLVM_MONOLITHIC_LIBRARY` a cada configuración.
- La prueba del builder exige ambos argumentos además de `LLVM_DIR`, `clang-cl` para C/C++ y
  `lld-link`.
- Se desactiva la búsqueda de copias externas de zlib, zstd y LibXml2 en Windows. zlib y zstd ya
  son targets vendorizados; los componentes LLVM seleccionados por NSC no requieren LibXml2. Esto
  elimina avisos engañosos e impide que paquetes instalados accidentalmente cambien el enlace.

## Archivos modificados

- `CMakeLists.txt`
- `nscbuilder.py`
- `tests/selfhost/test_bootstrap_infrastructure.py`

## Archivo nuevo

- `docs/compiler-optimization/TANDA-CORRECTIVA-G00-CACHE-LLVM-ESTATICO.md`: causa, migración de
  caché, alcance y evidencia de esta corrección.

## Validación dirigida

- El comando Release exacto reportado por Alberto se repitió sobre el mismo `build-clang` y terminó
  correctamente, sin borrar el árbol.
- LLVM 23.1.0 fue encontrado mediante sus componentes estáticos.
- DIA SDK fue resuelto correctamente.
- No aparecieron avisos de búsqueda externa de zlib, zstd ni LibXml2.
- Prueba unitaria específica del comando de `nscbuilder.py`: 1/1.
- Build incremental exclusivo de `nsc` en Debug/O0: correcto.
- Contrato posbuild sin LLVM-C/MSVC/UCRT dinámicos: correcto.
- No se ejecutó CTest ni baseline.
- `_abi/gates/G00/gate.json`, modificado por la corrida del usuario, se preservó y no forma parte
  de esta tanda.
