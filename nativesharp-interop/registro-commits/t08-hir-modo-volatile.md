# T08-015: modo volatile en HIR

Los accesos a campos transportan el modo capturado en el símbolo. La dirección
de un índice fijo lo conserva desde otra dirección de campo o elemento, sin
heredarlo de un valor puntero cargado. El modo se serializa ya en HIR/MIR;
esto no genera orden atómico todavía y falta validar el transporte.
