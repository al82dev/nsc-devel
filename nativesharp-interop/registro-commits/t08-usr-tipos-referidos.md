# USR en tipos de record y enum referidos

T08-002: al describir un tipo record o enum, NSC conserva el USR opcional de
su declaración canónica proporcionado por Clang. Esto también alcanza tipos
anidados en punteros, arreglos y firmas ya recorridos por `describeType`. No
reemplaza los controles de compatibilidad, layout ni selección existentes.
Las comprobaciones y la validación se agrupan por separado.
