# Tanda 20C — Construcción separada y recibo de NSC

## Objetivo

Separar la construcción de NSC de la configuración y ejecución de pruebas. Un
test no puede autorizarse únicamente porque exista un `nsc.exe`; necesita un
recibo vigente emitido después de una construcción exitosa.

## SCOPE LOCK

- Sólo cambia la infraestructura privada `_nscbuilder2` y sus pruebas.
- No cambia CMake, el compilador, NSLink ni el builder anterior.
- No se ejecuta ningún build de producto ni se escribe en `C:`.
- Release/Debug, limpio/incremental, CMake/Ninja, STATIC/DYNAMIC y O0-O3 del
  host deben implementarse aunque esta tanda sólo valide los planes.

## Implementación

`_nscbuilder2/compiler.py` añade:

- `CompilerBuildOptions`, que contiene todas las dimensiones que cambian el
  producto;
- `CompilerBuildPlan`, que configura las pruebas v2 pero construye únicamente
  el target `nsc`;
- limpieza segura mediante `cmake --fresh`, sin eliminar árboles mediante
  recursión;
- runner seleccionable `cmake --build` o Ninja directo;
- `CompilerReceipt`, con opciones completas, ruta, tamaño, `mtime_ns` y
  SHA-256;
- validación que rechaza un ejecutable ausente o alterado.

`_nscbuilder2/storage.py` conserva atómicamente un solo recibo activo. Una
construcción posterior sustituirá explícitamente ese recibo; la mera presencia
de un binario nunca habilita pruebas.

## Flujo y complejidad

La generación de planes es `O(1)`. La validación del recibo es `O(bytes)` por
el SHA-256 y usa un buffer fijo de 1 MiB. No existe recursión ni descubrimiento
por directorios.

## Validación dirigida

```text
python -m py_compile _nscbuilder2\compiler.py _nscbuilder2\storage.py
python -m unittest tests.harness.tests.test_nscbuilder2
Ran 10 tests in 0.427s
OK
```

El fixture comprobó Debug, STATIC, host O0, Ninja directo, `--fresh`, ausencia
de CTest/batería en el plan de build y reapertura válida del recibo. La
ejecución de esas configuraciones sobre NSC permanece pendiente por la
restricción vigente.
