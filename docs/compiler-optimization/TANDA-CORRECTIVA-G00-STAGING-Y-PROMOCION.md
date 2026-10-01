# Tanda correctiva G00 — staging y promoción del baseline

Fecha: 2026-10-01
Componente: `nscbuilder.py` y runners locales del compilador
Estado: terminada, pendiente del baseline completo ejecutado por Alberto

## Problema

El baseline usaba `C:\native-sharp` como prefijo operativo. Construía el compilador en A:, lo
instalaba en C:, copiaba `NativeSharp.Core` y `NativeSharp.Core.Collections.Generic` desde C: de
regreso a `A:\nsc-devel\lib` y después ejecutaba las pruebas. Eso mezclaba el producto estable con
el candidato y convertía una certificación en un deploy anticipado.

## Contrato nuevo

1. `A:\nsc-devel\build-clang` contiene el build incremental del compilador.
2. `A:\native-sharp-3.0-baseline` es el único prefijo del candidato de Gate 0.
3. El staging se elimina y crea de nuevo antes de instalar el candidato.
4. CTest, fixtures, bootstrap lock y mediciones ABI usan el candidato instalado en A:.
5. Las bibliotecas de las fuentes se validan en `A:\nsc-devel\lib`; nunca se copian desde C:.
6. Sólo un Gate 0 certificado, sin fallos, promueve A: hacia `C:\native-sharp`.
7. La promoción superpone los archivos certificados y conserva componentes externos ya instalados,
   como NSLink, bindings, debugger y Saru.
8. `C:\native-sharp` nunca es origen para modificar el árbol de fuentes.

## Archivos modificados

- `nscbuilder.py`: staging, destino de promoción, limpieza segura, promoción condicionada al Gate,
  rutas predeterminadas y eliminación de la sincronización C: hacia A:.
- `tests/selfhost/test_bootstrap_infrastructure.py`: contratos unitarios del plan, limpieza,
  promoción y bibliotecas locales.
- `tools/prepare_libraries.cmd`: deja de borrar y copiar bibliotecas; sólo valida las versionadas.
- `deploy.py`, `package_native_sharp.py`, `tools/nsc_performance_baseline.py`, `nslink_pipeline.py`,
  `run_cases.py` y runners `.bat`/`.cmd`: instalación predeterminada unificada en
  `C:\native-sharp`, sin el sufijo `-3.0`.

No se creó código fuente en C: ni se modificó `deploy.bat`.

## Mejora

El ejecutable y las bibliotecas probados pertenecen a una sola instalación candidata. Una corrida
fallida deja intacta la instalación consumida por el resto del ecosistema. La dirección normal de
publicación queda explícita: build A: → staging A: → instalación C:.

## Verificación específica

- Comprobación sintáctica de los scripts Python modificados.
- Cinco pruebas unitarias de infraestructura: 5/5 correctas.
- `git diff --check` sin errores del parche.
- No se ejecutó baseline ni CTest.

## Pendiente externo

El baseline completo debe repetirse porque la corrida que estaba activa tomó `S0` antes de esta
modificación. El nuevo Gate deberá comprobar `S0 == S1`, ejecutar toda la matriz desde el staging y
promover a C: únicamente si termina `LANDED`.
