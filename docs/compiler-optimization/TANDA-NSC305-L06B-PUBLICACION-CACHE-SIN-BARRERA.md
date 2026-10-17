# NSC 3.0.5 — L06B: publicación de caché sin barrera física síncrona

## SCOPE LOCK

- **Repositorio/rama/revisión base:** `A:\nsc-devel`, `development`,
  `ac9c723d9c1f1ef9fd6341bb7d234366692e3389`.
- **Entrada exacta:** emisión O0 de `tests/performance/p305/generics-32/generics-32.nsproj`
  como EXE, con line tables, caché fría y el mismo `nsc.exe`.
- **Defecto no funcional:** J1 termina `llvm-object-generation` en 12.153 ms, mientras J16 tarda
  79.098 ms. En J16, las tres unidades atribuyen 63.733–64.626 ms a `store-us`; lowering,
  target, optimizer y emisión permanecen en milisegundos.
- **Operación responsable:** `replaceFileAtomically` publica tanto objeto como metadata mediante
  `MoveFileExW(..., MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)`.
- **Hipótesis única:** exigir una barrera de durabilidad física por cada rename obliga a los
  escritores paralelos de una caché regenerable a competir por flushes de volumen. La atomicidad
  del rename es necesaria; su persistencia síncrona frente a corte eléctrico no lo es.
- **Invariante:** el objeto se publica antes de la metadata; un objeto ausente, incompleto o cuyo
  tamaño/SHA-256 no coincide es un miss seguro y nunca se consume como hit.
- **Naturaleza:** optimización interna no funcional del almacenamiento de caché fría.
- **Archivos autorizados:** `src/LLVM/CompilationArtifactCache.cpp`, este expediente y tracking.
- **Archivos protegidos:** lowering, optimizer, target machine, particionador, Driver, Builder,
  GUI, pruebas y consumidores.
- **Contratos congelados:** sintaxis, semántica, diagnósticos, API, ABI, NSMETA, objetos emitidos,
  claves, formato de caché, integridad, concurrencia y recuperación ante publicación incompleta.
- **Pruebas inmutables:** `nsc.compilation-artifact-cache` y
  `nsc.compilation-artifact-cache-driver`.
- **Acciones prohibidas:** baseline, suite completa, deploy, instalación, push, pull, fetch,
  cambios a C: o a cualquier unidad distinta de A:, MSYS y modificación de pruebas.
- **Gate:** build focal de `nsc`; ambas pruebas focales; repetición fría J1/J16 del mismo fixture;
  publicación concurrente íntegra; diferencial auditado y commit local aislado.

## Cadena causal

```text
caché fría + tres unidades LLVM concurrentes
→ cada unidad copia y autentica su objeto
→ cada unidad publica objeto y metadata con MOVEFILE_WRITE_THROUGH
→ los seis renames esperan durabilidad física y compiten en el volumen
→ store-us crece de 4.007 ms en J1 a 63.733–64.626 ms por unidad en J16
→ llvm-object-generation crece de 12.153 ms a 79.098 ms
```

La corrección conserva `MOVEFILE_REPLACE_EXISTING`, el lock por clave, los temporales hermanos, el
orden objeto→metadata y la verificación SHA-256. Sólo retira `MOVEFILE_WRITE_THROUGH`. Si el proceso
o el sistema se interrumpe antes de que ambos renames queden persistidos, la siguiente lectura
observa cold, incomplete-publication o corruption y recompila; no acepta bytes sin autenticar.

## Estado

Implementación terminada y validada. L06 permanece abierto hasta medir y corregir el proyecto
real con el log por unidad.

## Implementación

En Windows, `replaceFileAtomically` conserva `MOVEFILE_REPLACE_EXISTING` y retira únicamente
`MOVEFILE_WRITE_THROUGH`. No cambian las rutas, el lock por clave, los nombres temporales, el
orden de publicación, el formato de metadata, el tamaño registrado ni el digest SHA-256.

## Prueba contrafactual y medición

Se utilizó el mismo fixture, O0, line tables, cinco cachés frías independientes por modo y el
mismo árbol fuente. Cada ejecución alcanzó y terminó `llvm-object-generation`; el fallo posterior
del enlace por una biblioteca runtime externa no forma parte del productor medido.

| Variante | Jobs | Mediana LLVM | Mediana del máximo `store-us` |
|---|---:|---:|---:|
| Control con `WRITE_THROUGH` | 1 | 12.039 ms | 4.246 ms |
| Corrección sin `WRITE_THROUGH` | 1 | 11.425 ms | 3.516 ms |
| Control con `WRITE_THROUGH` | 16 | 15.464 ms | 5.275 ms |
| Corrección sin `WRITE_THROUGH` | 16 | 13.264 ms | 3.787 ms |

Resultado estable:

- J1: `llvm-object-generation` −5,1 %; máximo `store-us` mediano −17,2 %.
- J16: `llvm-object-generation` −14,2 %; máximo `store-us` mediano −28,2 %.
- La muestra inicial J16 de 79.098 ms cayó a 17.655 ms al aplicar el cambio, pero no se usa como
  cifra de cierre: las series mostraron outliers de almacenamiento y el resultado autorizado es
  la comparación de medianas anterior.

## Gates

- `nsc.exe` construido; contrato de runtime estático: `passed`.
- `nsc.compilation-artifact-cache`: PASS.
- `nsc.compilation-artifact-cache-driver`: PASS.
- Publicación concurrente de la prueba existente conservó objeto íntegro y restaurable.
- Una caché publicada por la variante final fue restaurada por un `nsc` con build hash distinto:
  tres unidades, tres hits, cero lowering/target/optimizer/emission/store.
- Compilación real del TU con la configuración del proyecto (`/W4 /WX`): PASS.
- `clang-tidy` 23.1.0 está en el PATH, pero el build vigente no contiene
  `compile_commands.json`. Dos pasadas manuales no resolvieron el header del propio proyecto;
  todos sus diagnósticos se descartan como contaminados. No se reconfiguró el build porque la
  configuración está congelada. **Análisis estático completo no verificado**.
- `git diff --check`: PASS; cero líneas nuevas por encima de 120 caracteres.
- Baseline, suite completa, deploy e instalación: no ejecutados por restricción del alcance.

## Evidencia en A:

- `A:\temp\nsc-l06-generics32-exe-j1-v2.log`.
- `A:\temp\nsc-l06-generics32-exe-j16-v2.log`.
- `A:\temp\nsc-l06b-restore-check.log`.
- Cachés contrafactuales bajo `A:\temp\nsc-l06b-control-f3d8ac27-*`.
- Cachés de la corrección bajo `A:\temp\nsc-l06b-bench-7c6a2921-*`.

## Alcance de la conclusión

L06B demuestra y corrige una penalización de publicación fría reproducible. No demuestra que los
123 s del proyecto real provengan de `store-us`, ni permite cerrar L06. El log real debe mostrar si
esta misma causa escala con sus 15 objetos o si el costo dominante está en lowering, target,
emission, verification u otra unidad rezagada.
