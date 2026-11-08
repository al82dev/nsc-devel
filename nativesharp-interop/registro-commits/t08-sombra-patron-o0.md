# Ámbito de patrón que sombrea una global C

Una plantilla precompilada usa una global C antes y después de un `if let` que
introduce una variable homónima. El consumidor verifica el resultado calculado
con ambas lecturas de la global y la variable del patrón. El validador
`patron-sombra/validar.cmd` compiló y ejecutó la muestra en O0. Otras formas
de patrones no quedan acreditadas por este caso.
