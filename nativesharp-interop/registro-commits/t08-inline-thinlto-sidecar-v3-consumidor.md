# T08-003: preparar restauración de objetos nativos

El consumidor conserva los bytes nativos de un sidecar aceptado y los restaura
en el directorio temporal del bundle, junto a los objetos del backend de la
biblioteca correspondiente. Si no puede escribirlos falla la compilación.
Cuando una biblioteca aceptada contiene esos objetos, el backend conserva todas
las definiciones porque aún no se extraen sus referencias como raíces.

La publicación de sidecars v3 sigue deshabilitada mientras se completan las
pruebas de productor, consumidor y rutas de corrupción. No se cierra la tanda.
