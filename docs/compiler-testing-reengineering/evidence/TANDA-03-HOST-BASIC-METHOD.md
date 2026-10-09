# Método de auditoría — Host.Basic

## Universo

Se seleccionaron por identificador los doce archivos `PF-000084` a `PF-000092` y `PF-000626` a `PF-000628`.
El total coincide con `inventory/production-files.csv`: 12 archivos y 939 líneas.

## Primera pasada

1. Lectura íntegra con líneas numeradas de cada header, plantilla y `.cpp`.
2. Extracción de tipos, funciones públicas, funciones privadas de clase, helpers privados de translation unit,
   aliases, constantes y estados locales con efecto contractual.
3. Reconstrucción del flujo de staging, preflight, commit y rollback sin ejecutar el filesystem.
4. Reconstrucción del padding, schedule, rondas y serialización SHA-256.
5. Búsqueda de consumidores mediante `git grep` y lectura de pruebas históricas relacionadas sólo como evidencia.
6. Diseño de casos válidos, inválidos, de borde y de plataforma sin implementar código de prueba.

## Segunda pasada

Se recorrieron nuevamente las declaraciones y cuerpos contra `inventory/symbols.csv`. La primera redacción había
absorbido cuatro símbolos dentro del algoritmo sin filas propias:

- constructor de copia eliminado;
- asignación de copia eliminada;
- estado local de commit;
- lambda de limpieza de preflight.

Se añadieron como `SYM-000057` a `SYM-000060` antes de ejecutar los gates.

## Gates mecánicos

- huella SHA-256 actual contra manifiesto congelado;
- presencia de al menos un símbolo trazado por archivo;
- unicidad de IDs de símbolo, contrato y caso;
- validez de cada referencia símbolo/contrato y caso/contrato;
- validez de línea de evidencia dentro del archivo fuente;
- continuidad de `TB-BAS-001` a `TB-BAS-088`.

No se compiló, construyó ni ejecutó ningún producto o prueba.
