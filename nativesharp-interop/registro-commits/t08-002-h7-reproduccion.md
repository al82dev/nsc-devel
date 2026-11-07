# T08-002: reproducción de H7 (constantes de enum C en bibliotecas)

Se añade el fixture autocontenido `pruebas-tanda-08-identidad-c/constantes-enum`:

- un enum con nombre y uno anónimo;
- cada uno usado desde un método ordinario y desde una plantilla pública;
- un caso con el enum en la superficie;
- dos controles sin constantes.

Se añade el modo `--inspect-c-constants`, que comprueba en el NSMETA publicado que
no hay estados sin espacio de nombres ni con el nombre de la constante, y que el
catálogo conserva el enumerador requerido. En `validar.cmd` se añade la sección
`constantes`, que también se puede ejecutar sola con `validar.cmd constantes`.

Resultado: los cinco productores que usan una constante fallan hoy en el
productor y los dos controles ejecutan.

En `T08-002-H7-CONSTANTES.md` se clasifica qué casos necesitan información
adicional en el consumidor. No se modifica el compilador.
