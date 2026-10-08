# Tanda 20E — Deploy explícito de NSC y herramientas

## Objetivo y frontera

Implementar la capacidad de instalar un build de NSC con recibo vigente e
incluir únicamente las herramientas seleccionadas. La capacidad existe, pero
no se ejecuta contra `C:` durante esta tarea.

## Flujo

1. Revalidar ruta y SHA-256 del recibo de NSC.
2. Revalidar cada producto obligatorio de los recibos de herramientas.
3. Rechazar productores duplicados o incompletos.
4. Escribir en A: un manifiesto con prefijo, fuentes, destinos y hashes.
5. Ejecutar `cmake --install` desde el árbol exacto registrado por NSC.
6. Aplicar las copias exactas del manifiesto y comprobar el hash de destino.

El deploy no descubre archivos, no copia árboles, no lee artefactos desde C: y
no puede ejecutarse como consecuencia lateral de Build, Stage o Tests.

## Archivos

- `_nscbuilder2/deploy.py`: `DeployPlan`, generación del manifiesto y aplicador
  privado.
- `_nscbuilder2/__init__.py`: exposición interna para GUI/CLI.
- `tests/harness/tests/test_nscbuilder2.py`: fixture de prefijo temporal en A:.

## Complejidad y seguridad

Para `n` herramientas y `b` bytes totales, la validación y copia cuestan
`O(n + b)` y memoria constante por streaming. El prefijo debe ser absoluto y
no puede ser la raíz de una unidad. No se usa recursión.

## Validación dirigida

```text
python -m py_compile _nscbuilder2\deploy.py _nscbuilder2\__init__.py
python -m unittest tests.harness.tests.test_nscbuilder2
Ran 11 tests in 0.479s
OK
```

La prueba aplicó sólo la fase de copias a un prefijo temporal bajo
`A:\nsc-devel`; no ejecutó `cmake --install` ni escribió en `C:`.
