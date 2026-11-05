# Tanda 08: layout de enum C importado

El cálculo de layout de tipos nombrados acepta un enum importado únicamente si
se conoce su base y layout y no hay conflicto. Los records importados siguen
sujetos a su política de representación por valor. Falta ejecutar las pruebas
agrupadas y comprobar su uso dentro de structs NativeSharp.
