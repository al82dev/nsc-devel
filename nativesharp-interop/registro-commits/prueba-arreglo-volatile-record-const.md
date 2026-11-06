# Arreglos C: puntero constante y record volatile

Se prepararon controles para un arreglo de punteros constantes a enteros
volatile y otro de punteros a structs volatile. El caso positivo comprueba
escrituras observadas por C y los stores volatile en IR; el negativo intenta
reasignar un slot constante. La batería aún no se ha ejecutado.
