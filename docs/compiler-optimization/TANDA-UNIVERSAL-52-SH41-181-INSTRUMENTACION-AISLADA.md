# Tanda universal 52: instrumentación aislada de SH41/181

## SCOPE LOCK

- Objetivo exacto: determinar mediante dos ejecuciones comparables si los cambios de
  `f0fad42ef` y `19ec3a57e` causan el fallo de `nsc.selfhost.self-hosted-generation`.
- Naturaleza: diagnóstico correctivo, instrumentación temporal y reproducción aislada.
- Universo autorizado: únicamente la prueba CTest 181, también identificada como SH41.
- Configuración autorizada: O0 y NSLink.
- Revisión actual evaluada: `290c55c7c` con el árbol de trabajo vigente.
- Rango candidato: `f0fad42ef^..19ec3a57e`.
- Archivos autorizados: `selfhost/nsc/CodeGen/LlvmBackend.ns`, `run_sh41_181.py` y este documento.
- Archivos protegidos: `nscbuilder.py`, CMake, verificadores, contratos, pruebas, fixtures, runtime,
  bibliotecas, NSLink y todo contenido en `C:`.
- Comportamiento protegido: la instrumentación sólo escribe evidencia; no cambia decisiones de
  tipos, ABI, ownership, generación LLVM, enlace ni validación.
- Acciones prohibidas: ejecutar otra prueba, construir toda la batería, baseline, deploy,
  instalación, modificación de infraestructura, escritura en `C:` o alteración de pruebas.
- Criterio de terminación: dos logs persistidos, diferencial causal demostrado, solución mínima
  aplicada y una tercera ejecución de la 181 aprobada en O0.

## Presupuesto de cambio

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\nsc-devel` |
| Componente | Self-host LLVM backend y ejecutor aislado SH41 |
| Símbolos | `ScalarType`, `AbiValueType`, `ConvertScalar` y helpers temporales etiquetados |
| Prueba | Sólo `nsc.selfhost.self-hosted-generation` |
| Perfil | O0 |
| Linker | NSLink |

## Instrumentación

Todo código temporal está delimitado por `NSC_SH41_181_DIAGNOSTIC_BEGIN/END` y toda salida usa
la etiqueta `NSC_SH41_181_DIAGNOSTIC`. El backend registra únicamente clasificaciones donde un
calificador de ownership o un placeholder genérico cambia la vista física. El script independiente:

1. construye sólo el target `nsc` de `build-clang`;
2. limpia únicamente `build-sh41-181/<variante>` después de validar su ruta;
3. ejecuta directamente el mismo wrapper que define SH41 con `NSC_SELFHOST_PROFILES=O0`;
4. usa el `nsc.exe` recién construido en `A:` y NSLink de `A:`;
5. fija `NS_LIB_ROOT=A:\native-sharp-3.0-baseline\lib` para que el binario de build encuentre
   el paquete precompilado exigido por los proyectos fuente, sin copiarlo ni modificarlo;
6. fija `NSC_LINKER=A:\nslink-devel\bin\release\nslink.exe` para que `nsc0 -> nsc1` use el
   mismo NSLink explícito que el resto de SH41;
7. conserva `sh41-181-<variante>-build.log`, `sh41-181-<variante>.log` y artefactos;
8. expande en el log la salida capturada de `nsc1 -> nsc2` y `nsc2 -> nsc3` cuando existe reporte.

El primer intento de `current` llegó al `nsc0 -> nsc1` y confirmó que el ejecutable recién
construido no encontraba `NativeSharpCore.lib` al ejecutarse fuera de un prefijo instalado. La
corrección del ejecutor usa el contrato existente `NS_LIB_ROOT`; no altera compilador, proyectos,
CMake, verificadores ni archivos de biblioteca.

El segundo intento completó frontend, semántica y los 12 módulos LLVM de `nsc0 -> nsc1`; falló
antes de crear la imagen porque el `nsc.exe` de build no tenía NSLink a su lado ni en `PATH`. El
ejecutor fija el contrato existente `NSC_LINKER`; no cambia la selección del linker dentro de SH41.

## Secuencia de evidencia

1. `--label current`: comportamiento vigente instrumentado.
2. Commit independiente que conserva comentados los bloques sospechosos y activa el comportamiento
   anterior exacto.
3. `--label previous`: comportamiento anterior instrumentado.
4. Cotejo de etapa, salida, clasificación, artefactos y resultado.
5. Corrección mínima y `--label final` hasta aprobar exclusivamente la 181.

## Estado

- Instrumentación actual: implementada, pendiente de build y ejecución.
- Ejecución actual: pendiente.
- Ejecución anterior: pendiente.
- Causa raíz: pendiente de evidencia comparativa.
- Solución final: pendiente.
