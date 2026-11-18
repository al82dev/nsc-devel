# Construcción fallida, excepciones anidadas, noheap, colocación y stackalloc en bucle

Fichas LKF-020, LKF-022, LKF-023, LKF-024, LKF-107, LKF-108, LKF-109, LKF-110 y LKF-111.

## Defectos corregidos

- **LKF-020 / LKF-023.** `delete x` de un objeto construido con `new (storage) T()` sobre
  almacenamiento crudo (por ejemplo `stackalloc`) entregaba memoria de pila al heap (`0xC0000374`),
  para clases y structs. Se rechaza ahora: "'x' was constructed in place with new (storage) and was
  not allocated with new; end its lifetime with destroy". `destroy x` sigue siendo la forma correcta,
  y la colocación con allocator (`new (pool) T()`) conserva su liberación por `Deallocate`.
- **LKF-108.** La advertencia "stackalloc inside a loop reserves again on every iteration" solo
  aparecía en las acciones de comprobación: una compilación correcta (`--emit-project-oop-exe`, `-lib`,
  `-dll`) solo mostraba diagnósticos si había errores. Ahora también muestra las advertencias. Una
  compilación restaurada desde la caché de bundles no las repite.

## Verificado sin cambio

- **LKF-107.** Si un constructor lanza después de iniciar un campo `owned`, el campo se destruye y la
  excepción se libera. Si lanza la base, el campo de la base se destruye y el cuerpo derivado no corre.
  Si lanza el cuerpo derivado tras completar la base, se destruyen ambos campos y corre el destructor
  de la base. Con allocator, el bloque vuelve por `Deallocate` (una reserva, una devolución).
- **LKF-109.** Un inicializador de campo que lanza se rechaza por C-13 aunque el constructor declare
  `throws`, así que no existe un inicializador posterior a la colocación que pueda lanzar; los
  inicializadores de objeto (`new C() { G = … }`) no existen para clases.
- **LKF-110 / LKF-024.** Un método `noheap` admite una clase `raii` promovida y un local ligado a
  `new` que no escapa; si el objeto escapa por el retorno se rechaza con NS3030.
- **LKF-111.** Lanzar dentro de un `catch`, relanzar con `throw;` y los `finally` anidados (de dentro
  hacia fuera) liberan cada excepción una vez. Las excepciones entre módulos C quedan en
  interoperabilidad.

## Contrato del nivel crudo (sin cambio)

- **LKF-020.** `delete` de un puntero devuelto por un allocator o arena (`ArenaAllocator.Allocate`)
  corrompe el heap: su procedencia llega por una llamada y no es demostrable en compilación; es nivel
  crudo, igual que el doble `destroy` de un objeto colocado (el destructor corre dos veces) y un
  `new` crudo sin `delete`.
- **LKF-022.** El presupuesto acumulado de `stackalloc` en un bucle no se comprueba: cada iteración
  reserva de nuevo hasta el retorno. La advertencia de LKF-108 lo señala en toda compilación.

## Prueba

`nsc.compiler.construction-exception-placement-boundaries`: 17 casos, O0/O2, con Core construida
desde el repositorio.
