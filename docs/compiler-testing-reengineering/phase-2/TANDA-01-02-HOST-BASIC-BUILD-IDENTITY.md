# Tanda 01.02 — Identidad de construcción de Host.Basic

## SCOPE LOCK local

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\nsc-devel` |
| Componente | `Host.Basic` |
| Contrato | `CB-BAS-006` |
| Casos | `TB-BAS-020` a `TB-BAS-023` |
| Binarios observados | Rutas explícitas de dos `nsc.exe` ya construidos bajo `A:` |
| Código de producto | Sólo lectura |
| Acciones prohibidas | Build de NSC, C:, deploy, remoto, baseline y batería histórica |

## Implementación

El controlador incorpora una autoridad de proceso específica para el contrato público de identidad. No busca
binarios ni usa instalaciones implícitas: recibe un NSC primario, un segundo NSC y el `BuildId.h` generado que
pertenece al primario. Cada invocación y sus flujos stdout/stderr quedan dentro del workspace del caso.

Los cuatro casos comprueban:

- constantes de release, lenguaje, etapa y release previo mediante protocolos públicos estructurados;
- igualdad entre los exports del binario y el header generado;
- identidades distintas entre dos construcciones reales;
- validez SemVer y correspondencia del core con la release.

La ausencia de una herramienta requerida es `INFRASTRUCTURE_ERROR`. Una salida exitosa que viola el protocolo o
el oráculo es `FAIL`; no se mezclan ambas causas. El router selecciona por la autoridad declarada en el manifiesto
y nunca usa otro ejecutor como fallback.

## Alineación del esquema

La asignación canónica de Fase 1 marca estos casos como `build-contract`, con optimización `N/A` y ejecutor
`nsc_suite_controller+nsc`. El schema ahora admite literalmente esa combinación. Esto no relaja los casos host:
`nsc_component_tests` continúa restringido a `unit|component`, `in-process-reset` y optimización `NA`.

## Exclusiones

- No se cambió `Version.h`, `Version.cpp`, Driver ni CMake de producto.
- No se congelan hashes o IDs concretos: se verifican formato, coherencia y desigualdad entre builds.
- No se ejecutó compilación de código NativeSharp ni se usó NSLink.

## Evidencia

- Manifiesto contra asignación canónica: 23/23 casos acumulados conservan ID, ejecutor y capa; esta tanda añade
  exactamente `TB-BAS-020..023`.
- Pruebas Python del controlador: 51/51 aprobadas.
- Gate real Debug: 5/5 pruebas CTest aprobadas; `CB-BAS-006` produjo 4/4 `PASS`.
- Gate real Release: 5/5 pruebas CTest aprobadas; `CB-BAS-006` produjo 4/4 `PASS`.
- Se observaron dos `nsc.exe` distintos ya construidos bajo `A:`; no se construyó ni modificó el compilador.
