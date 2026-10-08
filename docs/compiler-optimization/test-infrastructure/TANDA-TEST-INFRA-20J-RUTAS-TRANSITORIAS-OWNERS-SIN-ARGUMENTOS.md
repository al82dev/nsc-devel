# Tanda 20J — Rutas transitorias para owners sin argumentos

## Objetivo

Restaurar una ruta individual y ejecutable para 158 identificadores históricos
cuyo owner C++ conserva una entrada `int main()` o `int main(void)`, sin crear
158 ejecutables separados y sin declarar terminada su migración a fixtures.

## Inventario exacto

`tests/unit/transitional_legacy_units.tsv` contiene exactamente una fila por
identificador con estos cuatro datos:

1. identificador CTest histórico;
2. componente del registro consolidado;
3. owner C++ original;
4. símbolo de entrada generado.

El registro no acepta IDs, owners ni símbolos duplicados. Cada owner se compila
en una unidad de traducción independiente generada por
`generate_transitional_unit_adapters.py`; únicamente se renombra su `main`.
Así se conserva el cuerpo histórico completo, incluidas sus aserciones,
diagnósticos, comandos y códigos de salida.

## Integración

- CMake genera los 158 adaptadores dentro del árbol de build.
- `TransitionalLegacyUnitTests.cpp` registra cada ID por separado.
- `nsc_unit_tests.exe --case <id>` selecciona exactamente una fila.
- El target consolidado conserva las dependencias y definiciones que los
  owners históricos necesitan, incluidas las rutas de Clang para los casos de
  interoperabilidad.
- El entorno de ejecución sigue perteneciendo a CTest: `NSC_LINKER`,
  `NS_LIB_ROOT`, `NSC_WINDOWS_RUNTIME_LIBRARY`, `PATH` aislado y directorio de
  trabajo del subdirectorio de pruebas.

## Validación dirigida

1. `nsc_unit_tests` se construyó correctamente como único ejecutable
   consolidado.
2. El auditor enumeró 476 casos unitarios: los 318 anteriores más los 158 de
   esta tanda.
3. Los 158 IDs del TSV se invocaron individualmente mediante `--case`, bajo el
   entorno exacto materializado por CTest.
4. La primera pasada válida produjo 157 aprobados y un fallo transitorio en
   `nsc.stage54-2.atomics`.
5. Ese caso aprobó aislado con 78/78 checks y aprobó nuevamente después de
   ejecutar su predecesor `nsc.stage54-1.threads`.
6. Una segunda pasada completa produjo 158 aprobados y cero fallos.

El primer intento realizado sin el entorno CTest produjo 87 fallos; se conserva
como diagnóstico de infraestructura, no como resultado del lenguaje. Demostró
que una ejecución directa no puede omitir el contrato de entorno y, por tanto,
no se utilizó para cerrar ningún caso.

## Estado uno a uno

La lista nominal de esta tanda es el TSV de 158 filas. Cada una de esas filas
tiene una ruta única en el catálogo y fue observada en la segunda pasada con
código de salida cero. El reporte general
`test-execution-parity.{json,csv,md}` contiene una fila para cada uno de los 684
IDs históricos y queda así:

| Estado | Cantidad |
|---|---:|
| Con ruta ejecutable | 577 |
| En destino final correcto | 161 |
| Transitorio, capa final pendiente | 416 |
| Bloqueado por ruta especializada pendiente | 41 |
| Sin mapear | 65 |
| Ambiguo | 1 |

## Límite de la afirmación

Esta tanda demuestra ejecución individual de sus 158 IDs, no paridad final de
los 684. Los 158 casos están deliberadamente marcados
`TRANSITIONAL_WRONG_LAYER`: la matriz exige que su estímulo NativeSharp termine
como fixture externo. Ningún owner histórico fue retirado y ninguna expectativa
fue cambiada.

## Archivos

- `tests/unit/transitional_legacy_units.tsv`: inventario nominal.
- `tests/unit/modules/TransitionalLegacyUnitTests.cpp`: registro transitorio.
- `tests/unit/CMakeLists.txt`: generación, compilación y contrato de entorno.
- `tests/unit/consolidated/main.cpp`: incorporación del módulo.
- `tests/unit/include/nsc_test/Modules.h`: declaración del registro.
- `docs/compiler-optimization/test-infrastructure/test-execution-parity.*`:
  auditoría general de 684 filas.

## Trabajo pendiente explícito

- Crear adaptadores explícitos para cinco owners con argumentos.
- Separar los 18 selectores todavía contenidos en `Cc1CorrectnessTests.cpp`.
- Mapear los owners directos `.ns`, `.nsproj` y `.exe` restantes.
- Implementar las 41 rutas especializadas bloqueadas sin ejecutar O1, O2, O3,
  baseline, certificación ni promoción.
- Resolver el único ID ambiguo.
- Migrar los 416 casos transitorios a su capa final y demostrar paridad antes
  de retirar cualquier representación antigua.

No se modificó el compilador, `nscbuilder.py`, `_nscbuilder`, C: ni repositorios
de herramientas.
