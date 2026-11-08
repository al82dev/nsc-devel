# Identidades restauradas de anónimos anidados

La sonda nueva `nsc_cpp_c_anonymous_identity_tests` usa la ruta real del driver
para cargar la biblioteca de la muestra. Extrae del log de Clang los USR de los
dos campos y dos enumeradores, y comprueba que cada nombre corresponda al mismo
USR en el modelo de miembros o de constantes restaurado por NSC.

La compilación incremental de la sonda y el control dirigido pasaron. Los cuatro
USR coincidieron; el fixture también compiló y ejecutó el acceso directo y el
consumidor precompilado en O0. La sonda está incluida en la construcción y el
validador de la tanda. No se ejecutó la batería completa. La evidencia corresponde
a esta muestra, no a todas las formas posibles de agregados anónimos.
