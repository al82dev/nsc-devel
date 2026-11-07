# T08: comprobación IR de acceso directo e indexado

El verificador exige ahora dos accesos volatile para float y cuatro para los
dos enteros en el IR de NativeSharp, por operación y orden; la sonda Clang
conserva sus mínimos originales. El control actualizado aún no se ha ejecutado.
