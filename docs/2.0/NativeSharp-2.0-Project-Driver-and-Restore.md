# NativeSharp 2.0 — Driver de proyectos, restore y protocolo GUI

**Estado:** implementación integrada y certificada en `development`.

## Objetivo

NativeSharp 2.0 incorpora un flujo de proyecto comparable al frente de trabajo de otros SDK sin
reemplazar ni ocultar las opciones de compilación de bajo nivel. El mismo `nsc.exe` crea proyectos,
materializa bindings, compila, ejecuta y limpia. La GUI sólo es un cliente del driver.

## Superficie pública

```cmd
nsc new console -n Juego -o A:\codigo\Juego
nsc add package NativeSharp.Glfw --version 2.0.0 --source A:\packages\NativeSharp.Glfw\2.0.0 Juego.nsproj
nsc restore Juego.nsproj
nsc restore Juego.nsproj --locked-mode
nsc restore Juego.nsproj --verify
nsc build Juego.nsproj -c Release -O3 -gline-tables-only --linker NSLink
nsc run Juego.nsproj -- argumento1 argumento2
nsc clean Juego.nsproj
nsc list package Juego.nsproj
```

Todos los comandos aceptan `--output-format=json`. El documento resultante tiene
`schemaVersion`, `command`, `success`, `message` y los datos propios de la operación. Los errores
también son JSON y conservan la clase de salida de NSC.

## Contratos de archivos

- El `.nsproj` conserva el esquema estricto de NSC 2.0.
- `<proyecto>.nspm.json` guarda preferencias y solicitudes de paquetes.
- `NativeSharp.lock.json` fija identidad, versión, ruta materializada, tamaño y SHA-256 de cada
  artefacto.
- `obj/nativesharp/packages/<id>/<version>` contiene la materialización reproducible.
- `bin/<Debug|Release>` contiene el producto final.
- Si existe `<proyecto>/Content`, `build` publica el árbol completo en
  `bin/<Debug|Release>/Content`, conservando sus subdirectorios.

El package puede declarar `NativeSharp.package.json`, `package.json` o `.nspackage.json`:

```json
{
  "schemaVersion": 1,
  "packageId": "NativeSharp.Glfw",
  "version": "2.0.0",
  "libraries": [
    {
      "library": "NativeSharp.Glfw.lib",
      "metadata": "NativeSharp.Glfw.nsmeta",
      "runtime": "NativeSharp.Glfw.dll"
    }
  ],
  "files": []
}
```

Sin manifiesto se permite el convenio mínimo de exactamente un `.lib`, un `.nsmeta` y, de manera
opcional, un `.dll` en el directorio raíz del paquete.

## Diseño y métodos

`include/nativesharp/Driver/ProjectCommands.h` separa el frente de proyecto del parser histórico del
driver. `Driver::run()` reconoce el comando alto nivel antes de abrir el contexto de compilación y
le entrega un callback que vuelve a entrar únicamente con una acción de emisión existente.

`src/Driver/ProjectCommands.cpp` implementa:

- parser JSON acotado a 4 MiB, con claves duplicadas rechazadas;
- `loadSidecar()` y `renderSidecar()` como contrato canónico de la GUI;
- `resolvePackage()`, validación de rutas relativas y SHA-256;
- `makeRestorePlan()`, `materialize()` y `verifyPlan()`;
- publicación transaccional mediante staging, intercambio de directorios y rollback;
- `makeGeneratedProject()`, que inyecta `LibraryReference` sólo en un `.nsproj` temporal junto al
  proyecto propietario para conservar el significado de globs y referencias relativas;
- `executeBuild()`, que reutiliza los emisores OOP actuales y selecciona NSLink o lld-link;
- `publishContentDirectory()`, que prepara recursivamente `Content` en staging y reemplaza la
  publicación anterior sólo después de completar la copia;
- lanzamiento Win32 con quoting compatible con `CreateProcessW`;
- `clean` restringido a `bin`, `obj` y nombres transitorios conocidos.

## Invariantes de seguridad

1. Ningún artefacto de paquete puede ser absoluto ni contener `.` o `..`.
2. El archivo real se canonicaliza y debe permanecer dentro de la raíz del paquete.
3. `--locked-mode` compara la resolución exacta con el lock existente antes de escribir.
4. `--verify` no modifica disco.
5. Build con `--no-restore` consume el lock y verifica los hashes materializados; no necesita que el
   origen siga conectado.
6. `Main.ns` nunca se reemplaza al crear un proyecto.
7. El `.nsproj` temporal se elimina por RAII también ante fallo del compilador.
8. `clean` nunca recibe globs ni elimina fuera de la raíz propietaria.
9. `Content` no admite enlaces simbólicos y nunca se publica parcialmente.

## Compatibilidad

Las opciones históricas (`--emit-project-oop-*`, checks, dumps y verificadores) no cambian. Build
traduce configuración, optimización O0–O3, perfil de depuración y linker a esa superficie; por ello
no existe un segundo backend ni una semántica diferente para la GUI.

## Matriz de cierre

La implementación quedó certificada con la corrida canónica
`project-driver-restore-final-r4-20260923`, ejecutada con identidad reproducible
`5700000000000001`, instalación aislada en `A:\native-sharp` y ocho trabajos paralelos:

| Superficie | Resultado |
|---|---:|
| CTest con lld-link | 490/490 |
| Fixtures con lld-link | 148/148 |
| Triage con lld-link | 18/18 |
| CTest con NSLink | 490/490 |
| Fixtures con NSLink | 148/148 |
| Triage con NSLink | 18/18 |
| Gate 0 | `LANDED` |
| Fallos | 0 |
| Duración total | 8334.269 s |

La prueba `nsc.project-commands` pasó dentro de ambas matrices. Antes del baseline completo también
pasaron las pruebas Python del Project Manager (14/14), el smoke offscreen y el flujo real
`new -> restore -> build -> run -> clean`.

Artefactos de trazabilidad:

- `nsc.exe`: SHA-256
  `5373c22ce1abcc452a67b516aae883d8698562523112b415c084b62fbe260789`;
- reporte Markdown: SHA-256
  `23eea02e809b4578c7d0f2127c052bf4011d30870489bbd1db5fef9bec948fbb`;
- reporte JSON: SHA-256
  `83d042658feb6a65966c568d5a07dffe6e1f8055c9ac94001953f0f8df73174b`;
- metadata: SHA-256
  `f80a352c30fe9d2423133973356d3c2dca38a7e7f11e086addadc25204dff786`;
- contrato Gate 0: SHA-256
  `790d4b5e4a67a099e2ef438d5dcd15dfff40668eb3df1c7e810b3a598116d653`.

La corrida previa `project-driver-restore-final-r3-20260923` se descartó: una invocación del
compilador autocontenido terminó de forma intermitente con `0xC0000374` durante SH47/O3. La misma
puerta pasó 50/50 repeticiones aisladas y la corrida canónica r4 completa pasó sin reintentos
internos; r3 se conserva únicamente como evidencia diagnóstica y no cuenta como certificación.
