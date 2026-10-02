# Tanda baseline 06E: cierre de contratos self-host

## Objetivo

Cerrar los fallos deterministas del baseline completo sin ocultar los defectos
reales de reproducibilidad SH43 y SH44, que se atienden por separado.

## Cambios

- SH25 incluye `Source/ComptimeProcessor.ns` en el orden canónico del compilador.
- SH26 espera las 27 unidades reales del compilador autoalojado.
- SH48 cubre las 81 fuentes actuales de la biblioteca estándar.
- SH06 espera las categorías semánticas públicas 3.0 `place/value`.
- SH04 y SH10 suprimen explícitamente el banner de `nsc0` al comparar sólo el
  contrato diagnóstico; los errores continúan comparándose byte a byte.
- SH51 distingue el esquema de traza temporal del esquema de log de compilación
  y verifica la medición en la salida real del proceso.
- SH52 elimina correctamente el destino hostil tanto si permanece directorio
  como si el compilador lo reemplaza por archivo. La transacción del compilador
  ahora rechaza previamente directorios y reparse points como destinos de
  archivo y conserva intacto el directorio ocupado.
- SH53 deja de exigir la biblioteca LLVM monolítica, ausente por diseño cuando
  el host usa componentes LLVM estáticos.
- El candado bootstrap declara la línea NativeSharp 3.0 y el ABI runtime 1.1;
  los hashes promovidos permanecen intactos y cada corrida sigue congelando sus
  binarios seleccionados en un candado propio.
- La actualización de una instalación acepta `sdk/win64` como raíz administrada,
  igual que el escritor del manifiesto. Dos instalaciones consecutivas ya no
  rechazan `nativesharp-runtime.nsimports` durante el plan de actualización.
- SH54 reconoce la línea host NativeSharp 3.0 y genera el acta con nombre 3.0.
- SH47 queda registrado únicamente cuando se proporciona un ejecutable NSAsm
  explícito. NSAsm es un componente externo y ya no bloquea el baseline de NSC.

## Archivos modificados

- `CMakeLists.txt`
- `cmake/VerifyNativeSharpDistributionFinal.cmake`
- `selfhost/bootstrap/verify_developer_experience.py`
- `selfhost/bootstrap/verify_final_certification.py`
- `selfhost/bootstrap/verify_first_generation.py`
- `selfhost/bootstrap/verify_nsc_next_skeleton.py`
- `selfhost/bootstrap/verify_robustness_final.py`
- `selfhost/bootstrap/bootstrap.lock`
- `selfhost/contracts/module-set.contract.json`
- `selfhost/contracts/runtime-library.contract.json`
- `selfhost/contracts/semantic.contract.json`
- `selfhost/contracts/source-graph.contract.json`
- `src/Basic/ArtifactTransaction.cpp`
- `src/Driver/PackageLifecycle.cpp`
- `tests/unit/NativeSharp3T34LongPathPublicationTests.cpp`
- `tests/unit/Stage66E9E10ReleaseContractTests.cpp`

## Archivos nuevos

- Este documento de trazabilidad.

## Validación dirigida

- `nsc.selfhost.source-graph`: correcto.
- `nsc.selfhost.module-set`: correcto.
- `nsc.selfhost.nsc-next-skeleton`: correcto.
- `nsc.selfhost.semantics`: correcto.
- `nsc.selfhost.first-generation`: correcto.
- `nsc.selfhost.runtime-library`: correcto.
- `nsc.selfhost.developer-experience`: correcto.
- `nsc.ns3.t34.long-path-publication`: correcto.
- `nsc.selfhost.robustness-final`: correcto.
- `nsc.stage66.e9-e10-release-contract`: correcto, incluida la raíz
  `sdk/win64` en manifiestos de actualización.
- Dos instalaciones consecutivas sobre el paquete temporal SH53: correctas.
- `SH53 distribution`: correcto, 9/9 CTests.
- `nsc.selfhost.final-certification` (SH54): correcto, incluida su matriz final
  de tres pruebas.
- `nsc` se reconstruyó correctamente y pasó el contrato de enlace estático.
- SH47 quedó desregistrado al no haberse proporcionado NSAsm explícitamente.

No se ejecutó el baseline completo.
