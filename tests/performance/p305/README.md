# NSC305-P03 corpus reproducible de rendimiento

Corpus primario de la campaña 3.0.5. Arranca en `Hola mundo` (`hello`) para medir el costo fijo
del pipeline y escala con microfixtures de cardinalidad controlada (`generics-32`, `multi-source`),
todos propiedad de NSC y autocontenidos (sin dependencias externas ni timestamps en las claves).

## Fixtures

| Nombre | Proyecto | Propósito | Salida esperada |
|---|---|---|---|
| `hello` | `hello/hello.nsproj` | Costo fijo mínimo: 1 namespace, 1 callable, 1 retorno. | 42 |
| `generics-32` | `generics-32/generics-32.nsproj` | 32 structs genéricos instanciados una vez cada uno. | 42 |
| `multi-source` | `multi-source/multi-source.nsproj` | 4 unidades de traducción compartiendo un namespace. | 42 |

## Escenarios obligatorios

```text
COLD          caché vacía controlada; sin cambios.
WARM-0        segundo build con la misma caché; sin cambios (todos hits).
WARM-ROOT     caché caliente; un cuerpo privado de la unidad raíz cambia.
WARM-DEP      caché caliente; un cuerpo privado de una dependencia cambia.
ABI           caché caliente; una firma/layout pública cambia.
FAIL          caché caliente; un error sintáctico/semántico deliberado.
J1/JN         misma entrada limpia; sólo cambia el presupuesto `-j`.
```

Por ejecución se conservan: comando normalizado, configuración efectiva, identidad de compilador y
target, tiempos de pared y por fase/subfase (`--time`), `code-unit ... miss-reason` por unidad
(`--artifact-cache`), hashes de salida y el diagnóstico completo si falla. Al menos 5 repeticiones
tras un warm-up no contabilizado; se reporta mediana y p95.

## Ejecución

```bat
cd /d A:\nsc-devel
python tools\nsc_performance_baseline.py --corpus-file tests\performance\p305\corpus.json --levels O0 --debug -g0 --artifacts exe --linkers lld-link --iterations 5 --warmups 1
```

Todo el trabajo vive en `A:`; no se usan timestamps ni rutas temporales en las claves de medición.
