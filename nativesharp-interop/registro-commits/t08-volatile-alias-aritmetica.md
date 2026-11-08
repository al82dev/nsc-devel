# T08-010/015: contrato C en aritmética de punteros

La aritmética que devuelve un puntero conserva el marcador de contrato C
volatile del operando puntero, junto con el calificador ya transportado. No se
afirma que el resultado esté alineado: el análisis existente conserva solo la
alineación comprobada de un byte tras un desplazamiento arbitrario.

El marcador aún no se usa para generar accesos. Faltan la política del
consumidor, el tratamiento de alineación, otras operaciones y la validación
agrupada. La protección de la configuración explícita continúa activa.
