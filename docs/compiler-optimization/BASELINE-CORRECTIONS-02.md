# Correcciones del baseline 02

## Contexto

El baseline paralelo reportó dos salidas distintas para `cc1_c26_driver_minimo`: código 1 en O2
y código 6 en O1. El fixture usa rutas relativas derivadas del nombre del ejecutable para crear
su directorio de fuentes y archivos intermedios. Aunque `nscbuilder` separaba los ejecutables por
enlazador y optimización, los lanzaba a todos con la raíz del repositorio como directorio de
trabajo.

Como el nombre base del ejecutable es idéntico en O1 y O2, ambas variantes operaban sobre
`cc1_c26_driver_minimo.c26.src`. Una podía borrar el directorio mientras otra lo utilizaba, o leer
un archivo después de que la otra variante lo hubiera creado o truncado.

## Cambio

Los ejecutables de fixtures ahora se lanzan con su propio directorio de artefactos como directorio
de trabajo. La compilación continúa ejecutándose desde la raíz del repositorio porque sus rutas y
scripts conservan ese contrato.

## Archivos modificados

- `nscbuilder.py`: `_run_one_case` entrega `artifact_dir` como `cwd` al ejecutar el programa ya
  construido.

## Archivo nuevo

- `docs/compiler-optimization/BASELINE-CORRECTIONS-02.md`: documenta la causa, el alcance y la
  verificación dirigida de la corrección.

## Mejora y estabilidad

Cada combinación de enlazador, optimización y caso dispone ahora de un espacio de ejecución
independiente. Esto elimina la condición de carrera de C26 y protege cualquier otro fixture que
cree archivos mediante rutas relativas. No cambia la generación de código, el optimizador ni los
artefactos producidos por NSC.

## Verificación dirigida

- Se comprobó la sintaxis de `nscbuilder.py` de forma aislada.
- Se ejecutaron concurrentemente únicamente los binarios C26 O1 y O2 ya construidos, cada uno
  desde su propio directorio de artefactos; ambos devolvieron el código esperado 42.
- No se ejecutó ni se monitorizó el baseline.
