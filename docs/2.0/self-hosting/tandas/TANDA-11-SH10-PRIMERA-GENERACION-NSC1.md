# NativeSharp 2.0 self-hosting — tanda 11: primera generación `nsc1`

## 1. Identidad

| Campo | Valor |
|---|---|
| Estado | `LANDED` |
| Tareas | `NS2-SH-127` a `NS2-SH-138` |
| Gate | `SH10` |
| Entrada | `72d181085` |
| Commit | `selfhost(tanda-11): build and certify first nsc1 generation` |
| Fecha | 2026-09-19 |

## 2. Objetivo y resultado

Esta tanda convierte el ejecutable experimental `nsc-next` en una generación nombrada y
reconstruible: `nsc1`, cuya implementación es NativeSharp y cuyo único constructor autorizado es
el `nsc0` certificado. El gate parte dos veces de copias limpias del mismo conjunto fuente, en
rutas absolutas distintas, y exige ejecutables byte a byte idénticos antes de probar funcionalidad.

`nsc1` recorre con su propio lexer, parser, semántica e IR los 20 archivos que forman el compilador;
verifica los cuatro proyectos de biblioteca bootstrap; emite objetos O0, O1, O2 y O3; y ejecuta
cada resultado enlazado por `lld-link` y NSLink. `nsc0` continúa instalado, verde y recuperable.

## 3. Diseño

```text
fuentes congeladas ──copy──> clean-a ──nsc0──> nsc1-a ─┐
        │                                               ├─ SHA-256 exacto
        └──────────────copy──> clean-b ──nsc0──> nsc1-b ┘
                                                        │
                                  PE/imports + CLI + diagnósticos
                                                        │
                     20 fuentes nsc ──lexer/sema/SSA────┤
                     4 proyectos bootstrap ──verify─────┤
                     corpus O0–O3 ──COFF──> 2 linkers───┘
```

La reproducibilidad se consigue en el productor, no editando artefactos después: `LldLinker`
añade `/Brepro` a EXE y DLL. El verificador abre los PE en modo lectura y falla si las dos raíces
producen bytes distintos. No normaliza timestamps, rutas, checksums ni secciones.

## 4. Contrato congelado

`selfhost/contracts/first-generation.contract.json` fija:

- identidad `nsc1`, productor `nsc0` y doce IDs de trabajo;
- cinco raíces de fuentes (`selfhost/nsc` y cuatro bibliotecas bootstrap);
- proyecto del compilador y proyecto de ejecución;
- cuatro perfiles O0–O3 y los dos linkers admitidos;
- imports físicos del PE: Kernel32, KernelBase, Shell32, UCRT y LLVM-C;
- identidad textual completa y código observable 42.

Cada archivo `.ns` y `.nsproj` se registra con ruta canónica, tamaño y SHA-256. El digest global
usa orden ordinal y separadores NUL, de modo que una colisión por concatenación de nombres o
tamaños no pueda certificar otro conjunto.

## 5. Métodos, funciones y responsabilidades

| Elemento | Responsabilidad y condición de fallo |
|---|---|
| `source_files(root, contract)` | enumera sólo `.ns`/`.nsproj` bajo las raíces normativas y falla ante una raíz ausente |
| `manifest(root, contract)` | calcula entradas por archivo y digest agregado estable |
| `digest(path)` | SHA-256 binario, usado para generaciones y productos |
| `pe_contract(path, imports)` | valida MZ, PE32+, AMD64, secciones e imports requeridos sin modificar bytes |
| `run(executable, args)` | captura stdout/stderr, exit y tiempo monotónico de cada prueba |
| `verify_first_generation.main` | compara raíces, generaciones, diagnósticos, corpus, proyectos y productos |
| `VerifyNativeSharpFirstGeneration.cmake` | crea raíces vacías, compila `nsc1` dos veces y materializa la matriz enlazada |
| `LldLinker::linkExecutable` | pasa `/Brepro` para eliminar reloj de pared de EXE |
| `LldLinker::linkDynamicLibrary` | aplica la misma política a DLL |
| `Program::PrintVersion` | publica la frontera `bootstrap.sh10`, todavía stage 1 |

## 6. Archivos y cambios

| Archivo | Acción | Razón |
|---|---|---|
| `src/Linker/LldLinker.cpp` | modificar | reproducibilidad PE en origen |
| `selfhost/contracts/first-generation.contract.json` | crear | entrada normativa SH10 |
| `selfhost/bootstrap/verify_first_generation.py` | crear | auditoría y reporte canónico |
| `cmake/VerifyNativeSharpFirstGeneration.cmake` | crear | gate integral, dos raíces y matriz dual |
| `selfhost/nsc/Driver/Program.ns` | modificar | identidad de la primera generación |
| `selfhost/bootstrap/verify_nsc_next_skeleton.py` | modificar | expectativa acumulativa SH10 |
| `CMakeLists.txt` | modificar | registrar `nsc.selfhost.first-generation` serial |
| `tests/selfhost/test_bootstrap_infrastructure.py` | modificar | proteger contrato y ausencia de reescritura binaria |
| roadmap y READMEs | modificar | operación, trazabilidad y deuda |
| este expediente | crear | diseño, métodos, archivos, pruebas y evidencia de la tanda |

## 7. Matriz de pruebas SH10

| Dimensión | Casos |
|---|---|
| construcción | clean-a y clean-b, rutas absolutas distintas |
| productor | `nsc0` por ruta absoluta; no PATH |
| identidad | `--version`, `--help`, self-test y diagnóstico diferencial |
| frontend/IR | `--verify-ssa` para cada fuente del compilador |
| dependencias | cuatro `.nsproj` del Bootstrap Profile |
| optimización | O0, O1, O2, O3 |
| linker | `lld-link`, NSLink |
| ABI | PE32+ AMD64 e imports físicos exactos |
| observabilidad | tiempos por operación, tamaño y hashes |
| recuperación | baseline completo de `nsc0` posterior al gate |

## 8. Trazabilidad de tareas

| ID | Evidencia |
|---|---|
| 127 | manifiesto exhaustivo de fuentes dentro de `sh10-report.json` |
| 128 | dos árboles limpios contienen sólo fuentes y dependencias declaradas |
| 129 | ambos `nsc1` se construyen invocando la ruta absoluta de `nsc0` |
| 130 | parser PE, imports requeridos y dependencias físicas |
| 131 | CLI, diagnóstico, 20 recorridos lexer/parser/sema/SSA |
| 132 | validación de los cuatro proyectos del perfil |
| 133 | corpus del compilador y proyecto ejecutable escalonado |
| 134 | diagnóstico de fuente ausente idéntico a `nsc0` |
| 135 | ocho ejecuciones: 4 perfiles × 2 linkers |
| 136 | tiempos, tamaño y hashes registrados, sin exigir aún paridad |
| 137 | reporte canónico y gate promovido por SHA-256 |
| 138 | clean-a/clean-b en rutas distintas y comparación byte a byte |

## 9. Frontera honesta

SH10 demuestra la primera generación NativeSharp y que ésta ejecuta autónomamente todos los
subsistemas portados. El backend SH08 todavía emite el programa canónico después de validar la
fuente; por ello esta tanda **no** llama `nsc2` al resultado de esa emisión ni declara completado el
self-hosting. La sustitución de ese lowering canónico por reconstrucción íntegra de la siguiente
generación, sin ejecutar ni cargar el frontend de `nsc0`, es el criterio de SH11/Tanda 12.

Asimismo, “validar proyectos del perfil” no se presenta como creación final de sus LIB/DLL: ese
packaging sigue bajo el oráculo de `nsc0` hasta que la cadena de generación lo produzca por sí sola.

## 10. Baseline y evidencia

Corrida canónica `tanda-11-sh10-final-20260919`: `PASSED` en 784.57 s, sin escrituras en `C:`.

| Dato | Resultado |
|---|---|
| CTest `lld-link` | 447/447 PASS, 330.733 s |
| Fixtures `lld-link` | 148/148 PASS, 50.657 s |
| Triage `lld-link` | 18/18 PASS, 8.568 s |
| CTest NSLink | 447/447 PASS, 301.518 s |
| Fixtures NSLink | 148/148 PASS, 46.742 s |
| Triage NSLink | 18/18 PASS, 8.092 s |
| Fuente baseline | 1421 archivos; `8a1414455d2804f5342ad209049a4ee5f1cd360da55a383608f056081e7e55ed` |
| Fuente congelada `nsc1` | 95 archivos; `df430bcb019aff0957e13ad5f43f7a75492fd049bd17e796bcbbb32519502608` |
| `nsc0` instalado | `314996b65fa1d91cc4996ac059f8f2c779be43f0c7758c25541dceafe575094b` |
| `nsc1` producido con lld | `1f9de137361b10b48e5d415fb7fbf2d2f576b7a8288047760fb75aa9e67dc2d1` |
| `nsc1` producido con NSLink | `d554f8329e310b43159689208b2a3e0bfc84a5e4ef9704409df4e687284b65cd` |
| Reporte baseline JSON | `efd5242900489a608c23cbac830581fbcd8f58615541b0fad3ad1720d6853f4a` |
| Gate G00 | `610435a440d97e01fa3e852a4e69bf894fc01d3063884e040db68e08a4425b29` |
| Reporte SH10 lld | `88e21d763a3c5de00c580ccf75fe62b39489ae40e2ce4770db81dc236921e783` |
| Reporte SH10 NSLink | `d47a8fbfd56ddc57a83540fc5ef7b9b1e1ec19e8d3bcd4a2a435aa4bed8b9b11` |

Los dos `nsc1` producidos en clean-a/clean-b son idénticos dentro de cada linker. No se exige que
un PE enlazado por lld sea idéntico al PE de NSLink: cambian writer, tamaño y timestamp contractual,
pero ambos conservan ABI, imports, comportamiento y determinismo interno.

## 11. Cierre

- [x] diseño, métodos, funciones, archivos y deuda documentados;
- [x] gate SH10 aislado verde en ambos árboles CTest;
- [x] baseline completo verde;
- [x] evidencia SH10 promovida;
- [x] cambios ajenos excluidos;
- [x] commit exacto preparado: `selfhost(tanda-11): build and certify first nsc1 generation`.
