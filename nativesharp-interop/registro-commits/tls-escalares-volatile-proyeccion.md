# Proyección de escalares TLS volatile

El control C pasó y NSC rechazó los escalares `volatile` adicionales en la
barrera TLS. Se permite que los escalares C existentes alcancen `mapGlobalType`
y el descenso `volatile` compartido; tipos y anchuras no mapeados se siguen
rechazando allí. Pendiente validar estático y DLL agrupados y las regresiones.
