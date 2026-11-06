# Desplazamiento, máscara y unario de enum C

El proyecto rechazó desplazamiento, máscara y negación sobre enum C. Se prepara
resolverlos mediante su tipo subyacente y promover el operando en HIR; los
enums NativeSharp mantienen su comportamiento anterior. Falta validar con
enum de 8 bits y comprobar resultados y regresiones agrupadas.
