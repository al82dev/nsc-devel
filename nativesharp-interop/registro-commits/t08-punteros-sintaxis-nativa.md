# Tanda 08: acceso de campos por puntero

La segunda validacion rechazo `.` sobre punteros. El parser NativeSharp ya
representa `->` como desreferencia seguida de acceso a miembro; los proyectos
de record y union usan esa sintaxis para los punteros C. Los accesos anidados
por valor siguen usando `.`. Falta compilar y ejecutar ambos casos para validar
que la desreferencia y los campos importados funcionan.
