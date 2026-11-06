# Visibilidad de enum C reconciliado en un campo

El consumidor con header y biblioteca precompilada rechazó `MoodData.value`:
su tipo ya estaba reconciliado como `type:CEnumCallbackProducer::Mood`, pero
la comprobación del campo exigía que el tipo perteneciera al mismo proyecto
que el record. Se consulta el mapa existente de reconciliación y se permite
el enum C canónico solo cuando existe una identidad local reconciliada para
el proyecto del campo. La excepción se limita a campos de instancia; las
demás clases de tipo y consumidores conservan su regla anterior.

La corrección está pendiente de construcción incremental y de validación de
los consumidores con y sin header y de la tanda completa.
