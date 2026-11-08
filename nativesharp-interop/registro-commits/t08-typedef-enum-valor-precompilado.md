# Valor de enum C entre consumidor y biblioteca

El productor recibe y devuelve por valor un enum C mediante su typedef. El
consumidor obtiene un enumerador del encabezado equivalente, lo pasa al método
precompilado y comprueba el resultado. `typedefs/precompiled-enum/validar.cmd`
compiló y ejecutó el caso en O0, con y sin el encabezado original del productor;
los controles de enum incompatible y fuente modificada también pasaron.
No acredita otras optimizaciones ni todos los tipos subyacentes de enum.
