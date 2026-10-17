# NSC 3.0.5 — C07: caché persistente predeterminada por proyecto

## SCOPE LOCK

- **Repositorio/rama:** `A:\nsc-devel`, `development`.
- **Evidencia causal:** la compilación real del juego O0/release/line-tables tarda 78.989 ms en
  `llvm-object-generation`, 8.521 ms en enlace y reporta `artifact-cache-hits: 0` junto con
  `artifact-cache-misses: 0`. Esa combinación prueba que la GUI no habilitó caché.
- **Hipótesis única:** EXE/LIB/DLL de proyecto deben usar caché persistente sin exigir que cada GUI,
  IDE o script conozca `--artifact-cache`.
- **Raíz:** `<directorio-del-nsproj>/obj/nativesharp/artifact-cache`, siguiendo la convención ya
  usada por el gestor de proyectos para `obj/nativesharp/packages`.
- **Archivos autorizados:** driver/cache, ayuda y pruebas focales, este expediente y tracking.
- **Contratos congelados:** salida compilada, sintaxis, semántica, diagnostics, API, ABI, NSMETA,
  opciones explícitas `--artifact-cache`/`--artifact-cache-max-mb`, verificación y publicación
  atómica de objetos.
- **Escape:** `--no-artifact-cache` deshabilita expresamente el default; es incompatible con root o
  límite explícitos.
- **Capacidad default:** 2 GiB por proyecto, podada por el mecanismo C05; una raíz explícita conserva
  su política existente salvo que el usuario dé `--artifact-cache-max-mb`.
- **Prohibido:** caché global oculta, ruta en C: codificada, depender de la GUI, aceptar entradas sin
  digest/verificador, cambiar NSMETA, baseline, suite completa, deploy o instalación.
- **Gate:** helper de raíz determinista, validación de conflictos CLI, help, prueba focal de driver,
  prueba de cache, build de `nsc` y dos emisiones hello donde la segunda reporte hits sin pasar
  `--artifact-cache`.

## Estado

Terminado.

## Implementación

- Las emisiones de proyecto OOP `EXE`, `LIB` y `DLL` habilitan la caché aunque el frontend no
  proporcione opciones nuevas.
- La raíz predeterminada se deriva exclusivamente del proyecto:
  `<directorio-del-nsproj>/obj/nativesharp/artifact-cache`.
- La raíz predeterminada se limita a 2 GiB mediante la poda verificable existente.
- `--artifact-cache=<directorio-absoluto>` conserva la selección explícita de otra raíz.
- `--artifact-cache-max-mb=N` puede ajustar la capacidad de la raíz predeterminada o explícita.
- `--no-artifact-cache` deshabilita la política predeterminada y rechaza combinaciones
  contradictorias.
- La ayuda en español e inglés documenta default, reemplazo, límite y opt-out.

## Evidencia focal

- `nsc_compilation_artifact_cache_tests`: `PASS 24/24`.
- `nsc_compilation_artifact_cache_driver_tests`: `PASS 5/5`.
- Build focal de `nsc.exe`: correcto; contrato de host runtime estático aprobado.
- Fixture copiada a `A:\temp\nsc-c07-hello-20261017`, sin modificar el proyecto fuente.
- Primera emisión O0/g0, sin `--artifact-cache`: tres misses LLVM y tres publicaciones físicas
  `.obj` con sidecars `.meta` bajo la raíz predeterminada.
- Segunda emisión idéntica, sin `--artifact-cache`: tres hits LLVM de tres unidades.
- Tercera emisión con `--no-artifact-cache`: ninguna poda, consulta, hit, miss o publicación de
  caché; las tres unidades siguieron la ruta normal de compilación.
- Ambas emisiones alcanzaron el enlace. El enlace focal no concluyó porque la instalación externa
  seleccionada contenía una biblioteca runtime que el verificador no pudo abrir; ocurrió después
  de demostrar store/restore y no se modificó esa instalación.

## Límite demostrado y siguiente acción

C07 elimina la dependencia de la GUI y corrige el reporte real `0 hits / 0 misses`. No afirma que
una compilación fría sea más rápida ni evita por sí solo el lowering previo cuando el perfil usa
`line-tables-only`. C08 debe incorporar identidad de debug/source/ranges a la clave pre-LLVM y
habilitar restauración temprana segura para ese perfil. L06 seguirá midiendo y corrigiendo el costo
frío para que la caché no oculte el cuello de botella.
