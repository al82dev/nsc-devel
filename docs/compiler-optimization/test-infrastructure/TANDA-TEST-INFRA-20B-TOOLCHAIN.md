# Tanda 20B — Integración de toolchain en NSC Builder 2

## SCOPE LOCK de la subtanda

El cambio autorizado amplía exclusivamente Builder 2. Las fuentes de NSC,
NSLink, NSASM, NSDBG, el builder anterior y toda instalación en `C:` quedan
protegidos. La implementación puede generar comandos y operaciones de deploy,
pero durante esta subtanda no los ejecutará contra `C:`.

## Inventario de productores

| Productor | Fuente | Configuraciones | Productos consumibles |
|---|---|---|---|
| NSC | `A:\nsc-devel` | Release, Debug | `bin\nsc.exe` |
| NSLink | `A:\nslink-devel` | Release, Debug | `bin\<config>\nslink.exe` |
| NSASM | `A:\nsasm-devel` | Release, Debug | `bin\<config>\nsasm.exe`, `nsdump.exe`, `nsdis.exe` |
| NSDBG | `A:\nsc-debugger` | Release, Debug | `bin\<config>\nsdbg.exe`, `nsdbg.pdb` cuando exista |

La auditoría se obtuvo de los builders y CMakeLists vigentes en cada
repositorio. Builder 2 no duplicará su lógica de producto: conservará las
variables `*_PRODUCT_OUTPUT_DIR`, los targets oficiales y la resolución de
CMake/Ninja mediante el entorno.

## Flujo implementable

1. Construir NSC y emitir un recibo con configuración, ruta y SHA-256.
2. Construir cada herramienta seleccionada de forma independiente y emitir su
   propio recibo.
3. Verificar todos los productos declarados antes de considerar aprobado al
   productor.
4. Copiar herramientas seleccionadas al `bin` del build de NSC en A: mediante
   una operación de staging explícita.
5. Emitir un manifiesto que relacione productor, producto original, destino y
   SHA-256.
6. Al pulsar Deploy, validar nuevamente recibos y hashes; opcionalmente
   autogenerar primero las herramientas seleccionadas.
7. Instalar NSC en el prefijo elegido y copiar únicamente los productos
   seleccionados al `bin` resultante.

## Invariantes

- `Build compiler`, `Build tools`, `Stage tools`, `Configure tests`, `Run tests`
  y `Deploy` son operaciones distintas.
- Ninguna prueba reconstruye NSC ni una herramienta silenciosamente.
- Ningún deploy toma archivos de `C:`; `C:` es únicamente destino explícito.
- Un recibo deja de ser válido si cambia la ruta, configuración, tamaño, fecha
  o SHA-256 de cualquiera de sus productos.
- NSASM es una unidad de tres ejecutables; no se admite un recibo parcial.
- La ausencia del PDB opcional de NSDBG no invalida `nsdbg.exe`, pero el
  manifiesto registra si fue incluido.
- No se usa recursión para limpiar, descubrir, verificar ni copiar árboles.

## Complejidad

La verificación y el staging recorren una lista fija de productos: tiempo
`O(n + bytes)` por el hash y copia de cada archivo, memoria `O(1)` además del
buffer de I/O. No se recorre el repositorio completo ni se copian directorios
por conveniencia.

## Validación autorizada

- Pruebas unitarias de generación de planes, invalidación de recibos y
  manifiestos dentro de `A:\nsc-devel`.
- Comprobación sintáctica de Python y smoke de GUI.
- No ejecutar builds de productos, baseline, O1/O2/O3 ni deploy en `C:`.

## Estado

`NÚCLEO IMPLEMENTADO; GUI, AUTOGENERACIÓN Y DEPLOY EN CURSO`.

## Implementación del núcleo

### Archivos

- `_nscbuilder2/toolchain.py`: catálogo, opciones, planes, evidencia,
  recibos, validación y staging.
- `_nscbuilder2/storage.py`: persistencia atómica de recibos por herramienta
  y configuración.
- `_nscbuilder2/__init__.py`: superficie interna consumible por la GUI y CLI.
- `tests/harness/tests/test_nscbuilder2.py`: contratos dirigidos sin construir
  productos reales.

### Contratos cerrados

- Release y Debug se conservan en el plan de cada productor.
- CMake, Ninja, `clang-cl` y `lld-link` se nombran mediante el entorno, sin
  imponer sus rutas de instalación.
- El recibo contiene ruta, tamaño, `mtime_ns` y SHA-256 de cada producto.
- Un producto ausente o modificado invalida el recibo antes del staging.
- NSASM sólo genera recibo cuando existen sus tres ejecutables.
- El staging copia una lista exacta; no recorre ni copia directorios.
- Cada copia queda trazada en JSON con productor, configuración, origen,
  destino y SHA-256.

### Validación ejecutada

```text
python -m py_compile _nscbuilder2\toolchain.py _nscbuilder2\storage.py
python -m unittest tests.harness.tests.test_nscbuilder2
Ran 9 tests in 0.416s
OK
```

No se compiló NSLink, NSASM, NSDBG ni NSC. No se escribió en `C:`. La
validación de los comandos reales queda pendiente por restricción de ejecución,
no por falta de implementación.
