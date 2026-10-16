# Tanda correctiva: instalación limpia de NSC

## SCOPE LOCK

- Objetivo exacto autorizado: impedir que la instalación de NSC copie documentación fuente, Markdown, expedientes
  internos o especificaciones destinadas a publicación separada.
- Naturaleza: corrección de empaquetado e instalación; no cambia sintaxis, semántica, ABI, NSMETA ni compilación.
- Repositorio y rama: `A:\nsc-devel`, `development`.
- Entrada causal: `cmake --install`, invocado directamente o por NSC Builder 3.
- Operación responsable: las reglas `install(DIRECTORY ...)` de CMake copian íntegramente `docs/`, `spec/` y los
  archivos auxiliares de las bibliotecas distribuidas.
- Resultado esperado: una instalación nueva contiene sólo ejecutables, runtime, bibliotecas fuente NativeSharp,
  proyectos, SDK y contratos mecánicos requeridos por la autoverificación del producto.
- Documentación separada: `docs/` y `spec/` permanecen en el repositorio como fuentes de la futura publicación HTML;
  el usuario puede colocar una publicación documental junto al producto sin que el verificador la considere parte
  autenticada del compilador.
- Archivos de producto autorizados: `CMakeLists.txt`, `src/Driver/InstallationVerifier.cpp`,
  `src/Driver/InstallationManifest.cpp` y los dos mensajes de ayuda afectados en `src/Driver/Driver.cpp`.
- Pruebas autorizadas: `tests/unit/Stage60LanguageContractTests.cpp`,
  `tests/unit/Stage63InstallationVerifierTests.cpp` y `tests/unit/Stage64InstallationManifestTests.cpp`.
- Builder: auditado; `nsc-builder/nsc_builder/deploy.py` y `baseline.py` no copian documentación directamente, sino
  que delegan en `cmake --install`, por lo que no requieren modificación.
- Archivos y comportamientos protegidos: compilador, parser, frontend, HIR, MIR, LLVM, NSMETA, runtime, NSLink,
  NSDBG, bindings, juegos, contenido documental fuente y contratos mecánicos instalados.
- Acciones prohibidas: tocar `C:`, instalar, desplegar, ejecutar baseline o batería completa, mover o borrar la
  documentación fuente, regenerar HTML, modificar bindings, usar MSYS, push, pull o fetch.
- Validación autorizada: auditoría estática, configuración CMake en `A:` si es necesaria y pruebas focales de los
  tres contratos de empaquetado; sin instalación real.
- Criterio de terminación: CMake no enumera ni instala `docs/` o `spec/`, las bibliotecas sólo instalan `.ns` y
  `.nsproj`, el verificador no exige documentación, la ayuda no anuncia rutas documentales instaladas y las pruebas
  focales reflejan el paquete limpio.

## Presupuesto de cambio

| Dimensión | Autorizado |
|---|---|
| Repositorios | `A:\nsc-devel` |
| Componentes | Instalación, manifiesto, verificador y ayuda de NSC |
| Comportamientos | Contenido del paquete y validación de su layout |
| Formatos conservados | Contratos mecánicos, manifiesto criptográfico, SDK, `.ns`, `.nsproj` |
| Formatos excluidos | Árboles `docs/`, `spec/` y Markdown de bibliotecas |
| Destinos | Ninguno; no se ejecuta instalación |
| Perfiles | Configuración/pruebas focales solamente |

## Estado de validación

- Configuración CMake del build existente en `A:`: correcta.
- Compilación dirigida de `nsc_stage60_language_contract_tests`,
  `nsc_stage63_installation_verifier_tests` y `nsc_stage64_installation_manifest_tests`: correcta.
- `nsc.stage60.language-contract`: `PASSED`.
- `nsc.stage63.installation-verifier`: `PASSED`.
- `nsc.stage64.installation-manifest`: `PASSED`.
- Resultado focal: 3/3 pruebas superadas.
- No se ejecutaron instalación, deploy, baseline, batería completa, bindings ni juegos.
