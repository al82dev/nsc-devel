# Tanda 08: proyección inicial de arreglos C en campos

Los campos de records C con arreglos fijos de escalares o punteros escalares
representables se proyectan a la identidad de arreglo fijo de NSC, preservando
el orden de dimensiones. Los arreglos incompletos, calificadores no expresables
y elementos compuestos siguen sin proyectarse. No se ha validado el acceso
generado ni la coincidencia ABI.
