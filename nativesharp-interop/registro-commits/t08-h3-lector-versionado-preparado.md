# T08 H3: despacho preparado por version de metadata

Se agrega un lector interno que recibe el minor declarado: para el minor futuro 46 reconoce la envoltura preparada de dependencias; para formatos anteriores conserva el lector existente, incluido el caso 1.42 con catalogo heredado solo de enums. La salida permanece intacta si falla la lectura.

Se prepararon controles para aceptar la envoltura con 46, rechazarla con 45 y leer el catalogo 1.42. Los lectores de produccion y la version de NSMETA no cambian. Aun faltan la seleccion y publicacion, la verificacion completa, la huella ABI y la restauracion; los controles se ejecutaran con la validacion agrupada.
