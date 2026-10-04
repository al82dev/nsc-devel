# Configuración de rutas del bootstrap

## Precedencia

`nscbuilder.py --baseline` resuelve cada ruta en este orden:

1. opción de línea de comandos;
2. variable de entorno;
3. archivo JSON `nscbuilder-paths` versión 1;
4. valor predeterminado compatible con instalaciones anteriores.

## Correspondencia

| JSON | CLI | Entorno |
|---|---|---|
| `prefix` | `--prefix` | `NSC_INSTALL_ROOT`, después `NS_ROOT` |
| `build_dir` | `--build-dir` | `NSC_BUILD_ROOT` |
| `test_build_lld` | `--test-build-lld` | `NSC_TEST_BUILD_LLD` |
| `test_build_nslink` | `--test-build-nslink` | `NSC_TEST_BUILD_NSLINK` |
| `llvm_root` | `--llvm-root` | `NSC_LLVM_ROOT` |
| `nslink` | `--nslink` | `NSLINK_EXE` |
| `history_root` | `--history-root` | `NSC_HISTORY_ROOT` |
| `work_root` | `--work-root` | `NSC_WORK_ROOT` |
| `temp_root` | `--temp-root` | `NSC_TEMP_ROOT` |

`--write-root` es repetible. `NSC_WRITE_ROOTS` usa el separador de rutas del sistema. Cuando existe
al menos una raíz autorizada, el runner valida prefijo, builds, historial, trabajo, temporales y
evidencia antes de escribir. Una ruta fuera de ellas termina con error.

`--run-id` asigna un nombre estable al directorio de evidencia. Sólo admite caracteres seguros y
falla si el directorio ya existe; nunca reutiliza ni mezcla una corrida anterior.

`--build-id` fija los 16 dígitos hexadecimales de la identidad reproducible de `nsc0`. Su valor se
resuelve como CLI → `NSC_BUILD_ID_OVERRIDE` → `build_id_override` del JSON. El runner reconfigura
el árbol del compilador cuando existe este valor, aun si el cache de CMake ya estaba creado.

Los procesos hijos reciben `TEMP`, `TMP` y `TMPDIR` con el `temp_root` resuelto. Las rutas efectivas
quedan incluidas en `metadata.json` mediante la serialización del `RunPlan`.

## Lock físico por corrida

Después de construir e instalar `nsc` y antes de configurar los árboles CTest, el runner puede
crear `<run_dir>/bootstrap/bootstrap.lock` como evidencia descriptiva de la corrida. Esa captura
no es un requisito de aprobación para SH53 y no convierte las versiones o hashes de Python,
CMake, Ninja, clang-cl o LLD en parte del contrato del compilador. SH53 recibe
`NSC_TEST_COMPILER` únicamente para seleccionar de forma explícita la semilla NativeSharp.

La evidencia permite investigar una corrida, pero cambiar herramientas de construcción no invalida
por sí solo los resultados. La certificación compara los artefactos NativeSharp y su comportamiento,
no la identidad física del entorno que los produjo.

## Corrida aislada en A:

```text
python nscbuilder.py --baseline \
  --config selfhost/bootstrap/bootstrap.config.example.json \
  --jobs N
```

El ejemplo puede copiarse fuera del repositorio para una máquina concreta. Los archivos locales con
rutas o secretos no deben incorporarse a Git.
