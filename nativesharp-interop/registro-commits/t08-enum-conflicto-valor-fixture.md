# Control de enum C incompatible

Se agrega un consumidor que incluye un `NMood` con el mismo nombre y un valor
de enumerador distinto al de la biblioteca precompilada. El control debe
rechazarlo, aunque los nombres coincidan. La ejecución está pendiente; un
rechazo por otra causa no demuestra todavía el contrato completo.
