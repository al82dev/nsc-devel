# Segunda auditoría independiente — Host.Target

## Método

La segunda pasada comenzó en los 4 registros `PF-*`. Recalculó el inventario físico, la tabla builtin, cada
operación comprobada, los cuatro modos de agregado, la máquina bit-field, las matrices target/CPU y los lenguajes
de registros/mnemónicos. Después contrastó documentos, símbolos, contratos y casos.

## Resultado cuantitativo

| Control | Valor |
|---|---:|
| archivos | 4 |
| líneas | 781 |
| bytes | 26,937 |
| tipos | 13 |
| funciones | 33 |
| símbolos | 46 |
| contratos | 30 |
| casos diseñados | 240 |
| casos PR | 210 |
| casos NIGHTLY | 30 |
| omitidos o desconocidos | 0 |

## Gates

- 4 archivos con huella, estado, evidencia y sección;
- 46 símbolos con archivo, línea y contrato;
- 30 contratos con fuente y exactamente ocho matrices;
- 240 casos con IDs únicos y referencias válidas;
- paridad de 974 filas CSV/JSON y hashes fuente iguales al congelado;
- cero cambios en producción, pruebas, builders, C: o remotos;
- seis archivos preexistentes conservados byte por byte.

## Hallazgos conservados

1. Windows x64 y ARM64 comparten el modelo LLP64 de esta capa.
2. `maximumNaturalAlignment()` devuelve 8 aunque Int128/UInt128 publiquen 16/16 y puedan elevar un struct.
3. El overload simple y el general coinciden para structs ordinarios, salvo que el general también modela
   opciones y usa el mínimo agregado al resolver el vacío.
4. Flexible array no exige posición final en la implementación vigente.
5. Offset explícito sólo se valida contra alineación cuando el campo pidió mínimo local.
6. Bit-field de union aporta extent pero no alineación natural agregada.
7. CPU selected es el mínimo ordinal entre preferred y available.
8. La tabla de soporte CPU y la tabla de requisito de mnemónico son contratos distintos; esta última puede
   devolver `aes` o `pclmul` aunque la primera no los enumere.
9. Los parsers de registro consumen todos los dígitos y aplican límites arquitectónicos exactos.

Son observaciones estáticas, no defectos corregidos.

## Dictamen

`Host.Target` queda `AUDITADO` y `RECONSTRUIBLE`. No se afirma ejecución, cobertura ni ausencia de defectos de
producto.
