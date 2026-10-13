# Tanda de validación 256 — aislamiento A-only de la infraestructura de pruebas

## Alcance

Esta tanda corrige exclusivamente infraestructura de NSC Builder 3 y sus gates.
No modifica el compilador, CMake, el toolchain ni los oráculos de producto. La
fuente causal es la corrida que publicó `5,114 PASS`, `468 FAIL`, `17
INFRASTRUCTURE_ERROR` y `187 BLOCKED`.

## Causa confirmada

Los procesos de casos heredaban `TEMP`, `TMP` y `TMPDIR` del proceso que abrió
Builder. Varios helpers host construyen nombres locales reiniciados por proceso,
por ejemplo con sufijo `_0`; corridas concurrentes, reintentos o hijos que aún
conservaban handles podían reutilizar el mismo directorio externo. Los rechazos
`Access denied` de Language, Compilation, Language Service y Driver no eran una
ACL permanente: casos representativos de cada familia pasan al recibir un árbol
privado por variante dentro de la evidencia de la corrida.

Los cinco productores self-host compartían la misma fuga de entorno. Sus `187`
consumidores eran cierre causal del productor y no `187` defectos independientes.
Las `71` filas derivadas de Interop, LLVM, Linker, Runtime y Driver atraviesan la
misma frontera central; esta tanda valida una muestra por cada familia, pero no
presenta esa muestra como ejecución exhaustiva de las `71` filas.

Packaging tenía dos causas distintas:

- `C12B-PKG-003` lanzaba `sys.executable -B package_native_sharp.py` aunque el
  script ya expone `create_archive`. El gate carga ahora esa API en el proceso de
  Builder, conserva su envoltura de salida/error y entrega el entorno A-only a
  `nsc.exe` y `7z.exe`.
- `C12B-PKG-001` prueba literalmente `deploy.bat`. Su shell contractual es
  `cmd.exe`; bajo la regla A-only no se ejecuta ni se sustituye por otro contrato.

## Implementación

- Cada variante, preparación y fixture obtiene directorios privados `temp`,
  `staging` y `evidence` debajo del `EvidenceStore` de la corrida.
- La raíz se deriva del almacenamiento canónico y se rechaza si no es absoluta,
  no pertenece a `A:` o escapa de `A:\nsc-devel\_nscbuilder3`.
- Host, identidad, self-host, gates Python y preparación reciben explícitamente
  el entorno aislado. Reintentos y variantes distintas no comparten ruta.
- El archivador se exige sólo cuando la selección contiene Packaging. Su
  descubrimiento acepta únicamente override A válido o las ubicaciones canónicas
  A; no consulta `PATH`, registro ni `ProgramFiles`.
- `package_native_sharp.py` acepta un entorno explícito y lo propaga a sus hijos.
  El gate de `C12B-PKG-003` ya no crea un intérprete Python hijo.

## Evidencia dirigida

Con ejecutables, entradas, salidas y temporales contenidos en A pasaron:

- `TL-LANG-289`;
- `TF-COMPILATION-097`;
- `TF-LANGUAGE-SERVICE-233`;
- `TF-DRIVER-004`;
- `TF-INTEROP-273`;
- `TF-LLVM-001`;
- `TF-LINKER-290`;
- `TF-RUNTIME-025`;
- compilación y ejecución de `TSH-00001` desde el fixture materializado.

La prueba embebida del gate fija que `C12B-PKG-003` llama la API interna, no
`run_process`, y que conserva el entorno privado. Su ejecución posterior al
refactor queda pendiente en un Builder reiniciado: el proceso GUI que estaba
abierto no expone una frontera para recargar/invocar el módulo modificado. Los
seis IDs `T12B-01069..T12B-01074` deben ejecutarse allí con
`A:\apps\7zip\7z.exe`. Los seis IDs `T12B-00457..T12B-00462` permanecen
bloqueados por la dependencia contractual de shell fuera de A.

No se ejecutó batería completa. Tampoco se declara cerrada la totalidad de los
`17` errores de infraestructura, `71` derivados o `187` bloqueados hasta una
corrida que consuma el cambio.
