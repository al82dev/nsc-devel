# Tanda 00.02 — Selector común y cierre de dependencias

## Resultado

`Selection` y `select_cases` ofrecen una semántica única para el futuro ejecutable host y el controlador. Los
filtros por caso, contrato, componente, capa, perfil, build mode, optimización y tags se intersectan; `--all` es
una elección explícita y no puede mezclarse con filtros.

Los perfiles son acumulativos: `PR` selecciona PR, `NIGHTLY` incluye PR y NIGHTLY, y `WEEKLY` incluye los tres
buckets. La selección incorpora iterativamente el cierre transitivo de productores, salvo que una herramienta de
diagnóstico solicite explícitamente observar sólo el conjunto filtrado.

## Rechazos

- selector vacío;
- `all_cases` combinado con filtros;
- perfil, build mode u optimización desconocidos;
- selección sin coincidencias;
- colección con IDs duplicados;
- dependencia ausente en la colección cargada.

## Evidencia enfocada

`tests/compiler-suite/controller/tests/test_selector.py` valida orden determinista, intersección de filtros,
perfiles acumulativos, cierre de dependencias y errores de configuración. No ejecuta NSC ni prueba contratos de
producto.
