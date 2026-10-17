# NSC 3.0.5 — C08: restore pre-LLVM seguro con line tables

## SCOPE LOCK

- **Repositorio/rama/revisión base:** `A:\nsc-devel`, `development`,
  `c7bde5acea67044ed2d1f208f800083220594053`.
- **Evidencia causal:** el juego real compila O0 con `line-tables-only`, genera 15 unidades y tarda
  78.989 ms en `llvm-object-generation`. C07 habilita la caché por defecto, pero el generador sólo
  consulta antes del lowering cuando debug es `None`; con line tables construye y optimiza LLVM
  antes de consultar.
- **Hipótesis única:** una identidad pre-LLVM que incluya MIR, rangos, rutas, contenido completo de
  fuentes y mapas de prefijo permite restaurar objetos `line-tables-only` antes del lowering sin
  cruzar outputs debug distintos.
- **Naturaleza:** optimización incremental correctiva; no cambia el objeto producido en un miss.
- **Archivos autorizados:** identidad de caché MIR/debug, generador OOP de objetos, prueba focal,
  este expediente y tracking.
- **Contratos congelados:** sintaxis, semántica, diagnósticos, API, ABI, NSMETA, particionado,
  lowering, optimización, emisión, verificación COFF y comportamiento de debug `None`/`Full`.
- **Estrategia de seguridad:** la primera versión usa una huella debug conservadora común a todas
  las unidades. Un cambio de cualquier fuente/rango/mapa invalida todas las unidades; se prefiere
  un miss seguro a un hit incorrecto. La invalidación fina no se atribuye a C08.
- **Archivos protegidos:** frontend, parser, HIR, MIR, metadata, linker, runtime y consumidores.
- **Prohibido:** habilitar restore temprano para debug `Full`, ThinLTO o una clave que no cubra
  checksum/ruta/rangos; baseline, suite completa, deploy o instalación.
- **Gate:** prueba determinista de que contenido, ruta, rango y prefix map cambian identidad;
  pruebas de caché; build de `nsc`; dos emisiones hello `line-tables-only` donde la segunda haga
  hits antes del lowering; mutar sólo posición/comentario debe producir misses seguros.

## Presupuesto de cambio

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\nsc-devel` |
| Componentes | LLVM artifact cache identity y object generator |
| Perfiles | O0–O3 con `line-tables-only`; prueba focal O0 |
| Pruebas | target focal de cache y emisiones hello aisladas en `A:\temp` |
| Salidas | mismas unidades COFF en misses; hits verificados mediante `CoffObjectVerifier` |

## Estado

Terminado.

## Implementación

- `projectOopLineTableSourceIdentity` serializa de forma determinista:
  - todos los mapas de prefijo debug normalizados;
  - validez, `FileId`, offset inicial y offset final de cada rango;
  - ruta normalizada, tamaño y SHA-256 del contenido completo de cada archivo alcanzado.
- El colector de rangos del generador es iterativo y cubre tipos, miembros, campos estáticos,
  inicializadores, cuerpos MIR, acciones de cleanup y salidas no locales.
- La identidad de cada unidad en `line-tables-only` enmarca por longitud la identidad MIR y la
  identidad común de fuentes debug.
- La consulta pre-LLVM se habilita para `None` y `LineTablesOnly`. `Full` y ThinLTO conservan la
  ruta tardía existente.
- Un hit sigue verificándose con `CoffObjectVerifier` antes de aceptarse.

## Evidencia focal

- `nsc_compilation_artifact_cache_tests`: `PASS 29/29`.
- Las pruebas unitarias demuestran igualdad estable e invalidación separada por contenido, ruta,
  offsets y mapas de prefijo.
- Build focal de `nsc.exe`: correcto; contrato de host runtime estático aprobado.
- Fixture aislada: `A:\temp\nsc-c08-line-tables-20261017`.
- Primera emisión O0 `line-tables-only`: 3 misses LLVM.
- Segunda emisión idéntica: 3 hits LLVM en la consulta anterior a construir lowerer/target machine.
- Después de añadir sólo un comentario a la copia de fuente: 3 misses seguros.
- Rebuild de `nsc` sin cambios semánticos: build hash cambió de `ceb0fd3ddebb398a` a
  `7c22f9b82b4b3e61`; la emisión idéntica posterior restauró 3/3 objetos.
- Las emisiones alcanzaron el enlace. El enlace focal no concluyó por una biblioteca runtime no
  abrible en la instalación externa seleccionada; ocurrió después del gate store/restore y esa
  instalación no fue modificada.

## Límite demostrado y siguiente acción

C08 acelera recompilaciones idénticas y conserva invalidación debug segura. La huella de fuentes
es deliberadamente común a todas las unidades: cualquier cambio de fuente provoca misses en todas.
No se atribuye incrementalidad fina a esta tanda. L06 queda activo para medir y reducir lowering,
optimización y emisión fríos por unidad; una tanda posterior podrá reducir la huella debug por
partición sólo con equivalencia comprobada.
