# Tanda 20 — NSC Builder 2 e integración final

## Resultado

La tanda queda **TERMINADA**. Se creó un orquestador PySide6 independiente para
la infraestructura consolidada y se validó el recorrido completo sobre el
compilador construido en `A:`. El builder histórico no fue modificado.

Esta evidencia no es un baseline, una certificación, una promoción ni un
despliegue. La validación ejecutada fue exclusivamente la matriz O0 autorizada.

## Entregables

- `nscbuilder2.py`: entrada GUI y CLI del orquestador nuevo.
- `_nscbuilder2/model.py`: configuración, selección y plan inmutables.
- `_nscbuilder2/pipeline.py`: construcción única, congelamiento SHA-256,
  ejecución secuencial y cancelación del árbol descendiente.
- `_nscbuilder2/storage.py`: configuración e historial propios, persistentes y
  separados de `_nscbuilder`.
- `_nscbuilder2/summary.py`: condensación causal sin falsos positivos para
  contadores exitosos como `failed=0`.
- `_nscbuilder2/.gitignore`: conserva fuera de Git configuración, historial,
  workspaces y temporales locales.
- `tests/harness/tests/test_nscbuilder2.py`: contratos del plan, selección,
  persistencia, resumen causal y cancelación real.
- `tests/harness/nsc_test/suites.py`: cada suite especializada recibe un
  directorio de trabajo privado dentro de su evidencia.
- `tests/harness/tests/test_suite_adapter.py`: prueba del aislamiento del
  directorio de trabajo.

## Contrato del orquestador

1. Configura `NSC_TEST_INFRASTRUCTURE_V2=ON`.
2. Construye una sola vez el target `pruebas`.
3. Congela el SHA-256 de `build-test-v2/bin/nsc.exe`.
4. Permite seleccionar registro unitario, todos los manifiestos, una categoría,
   un caso o las suites especializadas.
5. Ejecuta NativeSharp únicamente con `-O0` y con NSLink explícito de `A:`.
6. Verifica al final que el compilador congelado no cambió.
7. Escribe `run.json`, `output.log`, un log por etapa y `errors.log` de manera
   progresiva bajo `_nscbuilder2/history/<run-id>`.
8. La cancelación termina sólo el proceso actual y sus descendientes mediante
   `taskkill /PID <pid> /T /F`; no enumera ni mata procesos ajenos.
9. CMake, Ninja, CTest y Python se resuelven desde `PATH`; no se fijan rutas de
   instalación para esas herramientas.
10. La GUI muestra salida completa, errores causales e historial persistente.

## Aislamiento de residuos

La primera matriz completa reveló 1,160 archivos `error*.log` en la raíz. El
productor era el adaptador de suites: los propietarios heredados se ejecutaban
con el repositorio como `cwd`, y `nsc.exe` escribe su error log automático en el
directorio de trabajo.

La corrección no oculta ni barre errores del compilador. Cada suite usa ahora:

```text
<evidence-root>/<suite-id>/cwd
```

Los 1,160 residuos no rastreados se eliminaron después de verificar su nombre y
su pertenencia exacta a `A:\nsc-devel`. Tras una prueba dirigida y el pase
completo de 28 suites, la raíz contiene **0** archivos `error*.log`.

## Validación ejecutada

### Infraestructura y GUI

- `python -B -m unittest discover -s tests/harness/tests -p "test_*.py"`
  - resultado: **36/36**.
- `QT_QPA_PLATFORM=offscreen python -B nscbuilder2.py --gui-smoke`
  - resultado: **exit 0**.
- cancelación real:
  - el proceso padre y su hijo fueron terminados;
  - un proceso centinela no relacionado permaneció vivo.

### Selección dirigida

- Caso `nsc.codegen-target`: **1/1**, exit 0.
- Categoría `diagnostics`: **6/6**, exit 0.
- Suite dirigida `nsc.selfhost.bootstrap-infrastructure`: **1/1**, exit 0.
- Después de la suite dirigida: **0** `error*.log` en la raíz.

### Matriz O0 completa mediante Builder 2

Run ID: `20261008T182103.581105Z-21b22a2d`

| Capa | Resultado | Tiempo |
|---|---:|---:|
| Registro unitario consolidado | 289/289 | 107.11 s |
| Manifiestos NativeSharp O0 | 76/76 | 1.16 s |
| Suites especializadas O0 | 28/28 | 1,170.33 s |
| Orquestación total | PASSED | 1,283.30 s |

- Errores causales: `[]`.
- SHA-256 congelado:
  `bb6e0402ae4ce5f23d144d1ac7d64ba9991037fef448c02851d895a48093bbf7`.
- El hash fue idéntico antes y después de las pruebas.

### Revalidación de las suites después del aislamiento

Run ID: `20261008T184451.281379Z-e33cb7ad`

- Suites especializadas O0: **28/28** en 1,138.55 s.
- Orquestación: **PASSED** en 1,142.51 s.
- Errores causales: `[]`.
- SHA-256 congelado:
  `ee70fbe455cc7dabb7b02a1f753177a06fbcfd8024113d3df4730bd133a38b38`.
- Residuos `error*.log` en la raíz: **0**.

## Fronteras verificadas

- `nscbuilder.py` SHA-256:
  `f67632044a0f4da02bbb66799ff1524cc7d436fc36c4080dba34c09883aa5cd6`.
- `git diff -- nscbuilder.py _nscbuilder`: vacío.
- Árbol histórico `_nscbuilder`: 38,700 archivos; no fue usado como almacén por
  Builder 2.
- No se escribió ni desplegó en `C:`.
- No se ejecutó baseline, promoción, certificación ni O1/O2/O3.
- No se hizo push ni otra operación remota.

## Uso

GUI:

```powershell
python nscbuilder2.py
```

Plan sin ejecutar:

```powershell
python nscbuilder2.py --headless-plan
```

Matriz completa O0:

```powershell
python nscbuilder2.py --headless-run
```

Selección puntual:

```powershell
python nscbuilder2.py --headless-run --category diagnostics
python nscbuilder2.py --headless-run --case nsc.codegen-target
python nscbuilder2.py --headless-run --specialized
```
