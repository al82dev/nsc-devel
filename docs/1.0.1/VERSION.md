# Contrato de versión NativeSharp 1.0.1

## Identidades vigentes

| Superficie | Valor actual | Significado |
|---|---:|---|
| Línea del producto | `1.0.1` | Carpeta documental, compatibilidad del compilador y siguiente release previsto. |
| Versión visible de `nsc` | `1.0.1-dev+build.<UTC>.<id>` | Build de desarrollo único, todavía no promovido por baseline. |
| Versión del lenguaje | `1.0.1` | Contrato visible en ayuda, diagnósticos y toolchain. |
| Stage interno | `67` | Marcador interno de cobertura del pipeline; no sustituye SemVer. |
| Target | `x86_64-pc-windows-msvc` | Único target contractual actual. |
| LLVM soportado | `23.x` | Línea de backend cargable aceptada por el compilador. |
| NSMETA escrito | `1.13` | Formato actual; conserva política y cuerpo de métodos inline reutilizables. |
| Native ABI | `1.0` | Contrato físico de símbolos y datos públicos. |
| Runtime ABI | `1.0` | Contrato de interoperación con el runtime nativo. |

## Por qué existen tres valores relacionados

`1.0.1` es la familia compatible del producto. `-dev` expresa el canal de desarrollo y evita que un
binario no certificado se presente como release final. `+build.<UTC>.<id>` identifica una
construcción exacta sin cambiar la precedencia SemVer ni la compatibilidad declarada.

Ejemplo:

```text
nsc 1.0.1-dev+build.20260912T153000Z.0123456789abcdef
(NativeSharp 1.0.1, stage 67, build 0123456789abcdef)
```

Dos ejecutables `1.0.1-dev` pueden tener distinta identidad de build. Los reportes, repros y
resultados deben conservar la cadena completa; escribir únicamente “probado con 1.0.1” no basta
durante desarrollo.

## Promoción a release

La versión no pierde `-dev` por fecha, por cantidad de commits ni porque una compilación aislada haya
pasado. La promoción requiere una decisión explícita respaldada por el baseline acordado. Hasta ese
momento:

- la documentación vive en `docs/1.0.1` porque ésa es la línea que describe;
- `nsc --version` conserva `-dev`;
- no se atribuye una certificación 1.0.1 a los ejemplos o libros copiados;
- cada corrección puede seguir evolucionando dentro de la misma línea compatible.

Cuando se certifique, el cambio de canal deberá hacerse en una entrega separada: la versión visible
pasará a `1.0.1+build.<UTC>.<id>` o a la identidad final acordada, la documentación registrará el
build certificado y el historial conservará el estado `-dev` anterior.

## Compatibilidad

El campo `compiler_release=1.0.1` del contrato del toolchain expresa compatibilidad, no
certificación. La política estable acepta requisitos de la misma major cuando el compilador actual
es igual o posterior, y rechaza majors futuras o incompatibles. El canal `-dev` permanece en la
identidad visible del ejecutable, donde no rompe el parser canónico `major.minor.patch` usado por
`--check-compiler-compatibility`.

La versión del formato NSMETA, los ABIs y los formatos de manifiesto tienen numeración independiente.
Cambiar de `nsc 0.9.0` a `1.0.1-dev` no autoriza a modificar esos números si su contrato físico no
cambió.

Para NSMETA, la ventana productora vigente es explícitamente N−1/N: `1.0.1` lee artefactos
producidos por `0.9.0` o `1.0.1`, incluidos sus sufijos `-dev` y `+build`. Versiones anteriores no se
admiten por parecido numérico; deben pasar por una migración deliberada. El formato, fingerprint y
cada versión ABI se verifican además de esta identidad de productor.

## Archivos que gobiernan la identidad

- `CMakeLists.txt`: versión del proyecto y base visible `1.0.1-dev`.
- `include/nativesharp/Basic/Version.h`: lenguaje, release compatible y stage.
- `cmake/GenerateBuildIdentity.cmake`: timestamp UTC e identificador único de cada build.
- `src/Driver/ToolchainContract.cpp`: contrato reproducible instalado.
- `src/Driver/CompatibilityPolicy.cpp`: política de versiones y formatos.
- `include/nativesharp/Interop/LibraryMetadata.h`: NSMETA, Native ABI y Runtime ABI.

---

# NativeSharp 1.0.1 version contract

The compatible product line is `1.0.1`, while development binaries identify themselves as
`1.0.1-dev+build.<UTC>.<id>`. The `-dev` channel remains until an explicit baseline-backed promotion.
Build metadata distinguishes exact binaries without changing SemVer precedence. Language, compiler,
NSMETA, native ABI, and runtime ABI versions are separate contracts and must not be advanced merely
because another one changes.

For NSMETA, the active producer window is explicitly N-1/N: compiler 1.0.1 reads artifacts produced
by 0.9.0 or 1.0.1, including their `-dev` and `+build` suffixes. Older versions are not admitted by
numeric resemblance; they require a deliberate migration. Format, fingerprint, and every ABI
version are verified in addition to this producer identity.
