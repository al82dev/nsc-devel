# Tanda 17, cambio 5: reutilizar la compilación de un proyecto con bibliotecas C++

## Defecto

En la [auditoría inicial](../TANDA-17-AUDITORIA-INICIAL.md), un proyecto cuyo encabezado incluye
`<vector>`, `<string>`, `<map>` y `<algorithm>` tarda unos 10 s por compilación (`names` 2,9 s, `members`
4,9 s), con la caché de artefactos habilitada o no; sin la STL tarda 0,2 s y `clang -fsyntax-only` del
encabezado, 0,5 s (T17-010).

Causa en el código: `lookupCompilationBundle` deja sin caché de compilación toda sesión que no sea C
(`options.language != C`), y `CompilationInputManifest::calculateIdentity` rechaza cualquier proyecto con
`CppLibraryReference`. Las dos barreras son de cuando la identidad no registraba los encabezados transitivos
(`build-cpp-sin-cache.md`). Hoy cada sesión da sus opciones (`cppSessionConfigurationIdentity`), la huella
de su preprocesado y el contenido de cada archivo que cargó Clang (`sourceFileContentsSnapshot`), en C y en
C++; la sesión no lee encabezados precompilados ni interfaces de módulo (T14-009, T14-010), así que esos
archivos son todas sus entradas.

## Diseño (T17-009, T17-011)

- La caché de compilación se usa con sesiones C y C++ cuando cada referencia C/C++ del grafo tiene su sesión
  en la identidad: opciones, huella del preprocesado, contenido de cada archivo cargado, el binario del
  controlador de Clang, las bibliotecas, los runtimes DLL y su cierre, las fuentes NativeSharp y las opciones
  de NSC (`--crt-startup`, `--link-lib`, nivel de optimización y depuración), como antes en C.
- Si falta una sesión o su captura, la caché sigue deshabilitada (`compilation-bundle-cache: disabled`).
- `calculateIdentity` acepta referencias C/C++ solo si `lookupCompilationBundle` marcó la captura completa
  (`setCppSessionInputsCaptured`); cualquier otro uso conserva el rechazo.
- Una reutilización restaura el producto entero; las sesiones se abren igual (su captura es la que decide),
  así que el ahorro es el del análisis semántico y la generación de código.

## Cambio

- `PrecompiledModuleConfiguration.cpp`: sin la condición de lenguaje; marca la captura completa.
- `CompilationInputManifest.cpp/.h`: `setCppSessionInputsCaptured` y la barrera condicionada.

No se modificó Clang ni NSLink. El catálogo NSMeta no cambia de formato.

## Evidencia

Fixture nuevo `pruebas-tanda-17-cache/validar.py`, con y sin `--dll`. Un proyecto con una biblioteca C++
cuyo encabezado incluye otro desde un `IncludeDirectory`, con `Define` y `Standard`, se compila diez veces
con la caché de artefactos; cada valor coincide con el control C++ de Clang 23.1.0 con las mismas entradas:

| Paso | Cambio | Caché | Resultado |
|---|---|---|---|
| base | — | not-found | 3211 |
| repetida | ninguno | hit | 3211 |
| define | `K=2` | not-found | 3221 |
| define-vuelve | `K=1` | hit | 3211 |
| encabezado-transitivo | `inner.h` | not-found | 3215 |
| standard | `c++17` | not-found | 3115 |
| include-directory | `inc2` | not-found | 3212 |
| biblioteca | `api.lib` (y `api.dll`) | not-found | 4212 |
| fuente | `Main.ns` | not-found | 4219 |
| fuente-repetida | ninguno | hit | 4219 |

Medición (T17-010) con el encabezado de la STL y `--crt-startup`: sin caché 10,14 s, primera compilación con
caché 11,46 s, segunda 1,14 s (`hit`). Evidencia: `A:/temp/nsc-t17-cache-hgm6txsf` (estática),
`A:/temp/nsc-t17-cache-mw01xmqd` (DLL).

Regresión: en la validación agrupada de la tanda (`TANDA-17-VALIDACION-AGRUPADA.md`).

## Límites

- Una compilación sin reutilización sigue costando unos 10 s con la STL: el inventario de `names` y `members`
  procesa todas sus declaraciones. Reducir ese costo (inventario perezoso) queda como pendiente documentado.
- `nsc build` mantiene su ruta sin caché para proyectos C/C++ (`build-cpp-sin-cache.md`).
