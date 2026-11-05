# Tanda 08: tipos importados en firmas de callbacks

La proyección de un puntero a función utiliza la misma resolución recursiva de
tipos que las firmas exteriores. Esto prepara la representación de enums y
punteros a records importados cuando están disponibles y evita borrar const o
volatile no expresables en la identidad estructural. Los typedef de callbacks
compuestos, la llamada real y el ABI siguen pendientes de validación.
