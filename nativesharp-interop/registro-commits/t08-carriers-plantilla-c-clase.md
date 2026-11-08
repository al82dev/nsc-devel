# Contenedor de la plantilla del fixture combinado

El primer intento falló al publicar metadata porque la plantilla estaba en un
`struct` y el productor exige un contenedor de clase materializable. El fixture
usa ahora una clase con los mismos métodos estáticos, como la prueba de plantillas
existente. La ejecución posterior aún está pendiente.
