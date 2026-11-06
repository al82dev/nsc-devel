# Local inferido de enum C

El análisis y la emisión admiten el valor local de un enum C completo con base
y layout validados. La barrera de vida de objetos permanece para tipos C++ y
tipos importados que no cumplan esas condiciones. El caso de enum compartido
entre módulos también necesita resolver las identidades y enumeradores; falta
la validación agrupada.
