# Diagnóstico dirigido de tipo C en biblioteca

Con NSC hash 3cfae0e8fc412141, el fixture completo falla al resolver
`get_c_record`. El productor aislado llega a metadatos y falla porque la firma
referencia `CPublishedProducer::CRecord`, ausente del catálogo local. Ambos
terminaron con código 1; el consumidor y la lectura no se ejecutaron. No es
la validación completa de la tanda ni una solución de persistencia.
