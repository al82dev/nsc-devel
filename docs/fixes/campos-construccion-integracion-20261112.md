# Verificación dirigida: campos y construcción genérica

La revisión probada fue `bc594ada5`, en `development-fixes-limitaciones`.
Se reconstruyeron incrementalmente `nsc` y el ejecutable unitario de
introspección en `A:/nsc-fixes-evidencia/build`; las ejecuciones usaron ese
compilador, runtime y SDK aislados. Se emplearon Core O0 y O2 ya construidos en
la campaña, no la instalación del sistema.

* `static_field_source_fields.py`: éxito; 786 operaciones registradas en
  `A:/nsc-fixes-evidencia/temp/nsc-source-fields-79hobp_w/results.json`.
* `static_field_binary_fields.py`: éxito; 546 operaciones registradas en
  `A:/nsc-fixes-evidencia/temp/nsc-binary-fields-grt_r83o/results.json`.
  Los números corresponden a operaciones, no a defectos ni casos originales.
* La variante de certificados heredados pasó sus controles y dejó evidencia
  en `A:/nsc-fixes-evidencia/temp/nsc-binary-fields-_9edyw16/results.json`.
  Sin embargo, su unitario omitió dos verificaciones que no pertenecían al
  contrato exigido por esa variante; no se cuentan como cubiertas allí.
* Se preparó aparte el productor `MetadataProducer` a partir de la fuente de
  prueba existente y un consumidor que alcanza `MetadataFields.Record`, y se
  reutilizaron los proyectos de certificados y construcción genérica aislados.
  Con las cuatro variables de proyecto configuradas, el unitario pasó **85
  comprobaciones sin omisiones**. Comando y configuración reproducibles en
  `A:/nsc-fixes-evidencia/verificar-campos-completo.cmd`; resultado en
  `A:/nsc-fixes-evidencia/campos-unitario-completo.log`. Una primera prueba con
  un consumidor vacío falló porque la superficie recortada no necesitaba
  `Record`; se sustituyó el consumidor auxiliar por `PrunedOrdinary.ns`, sin
  modificar las fuentes originales ni su expectativa.
* `precompiled_new_constraint_visibility.py` pasó para Core O0 y O2, tanto la
  compilación como la segunda fase que inspecciona importaciones y ejecuta
  los positivos. Evidencia y comandos en `campos-generica-O0.log`,
  `campos-generica-O2.log` y sus correspondientes archivos `-ejecucion.log`
  bajo `A:/nsc-fixes-evidencia`. Los diagnósticos esperados de los controles
  negativos forman parte de la prueba.

No se identificó un defecto nuevo en este subconjunto ni se modificó el
compilador. Estos resultados no certifican todas las rutas de alias, campos,
cachés o jerarquías; las limitaciones documentadas en la campaña siguen
abiertas. No se ejecutó batería completa ni pruebas de interoperabilidad C/C++.
