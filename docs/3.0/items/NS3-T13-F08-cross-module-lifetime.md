# NS3-T13-F08 — Allocator y destrucción entre módulos / Cross-module allocator and destruction

## Español

NativeSharp 3 define un contrato ABI 1.0 por tipo exportado con tres thunks distintos y
versionados: creación, destrucción y liberación. El contrato conserva módulo productor, identidad
del tipo e identidad del allocator. Un consumidor nunca sustituye el allocator: destruye y libera
mediante los thunks publicados por el módulo que reservó el objeto.

`ModuleLifetimeRegistry` cuenta objetos vivos por productor, rechaza mezclar contratos
incompatibles y bloquea la descarga mientras quede cualquier objeto que necesite código del
productor. Una vez iniciada la descarga tampoco permite nuevas instancias. El modelo de
destrucción publica los tres nombres y la versión ABI en su dump verificable.

La prueba dirigida valida el registro/unregistro y el rechazo de unload, y compila/ejecuta una
ruta real de constructor/destructor a `-O0` con NSLink. No se ejecutó baseline ni suite general.

## English

NativeSharp 3 defines a versioned ABI 1.0 contract per exported type with distinct create,
destroy, and deallocate thunks. The contract retains producer module, type identity, and allocator
identity. A consumer never substitutes its allocator: destruction and deallocation go through the
thunks published by the module that allocated the object.

`ModuleLifetimeRegistry` counts live objects per producer, rejects incompatible contract mixing,
and blocks unload while any object still needs producer code. Once unload begins it also rejects
new instances. The destruction model publishes all three names and the ABI version in its
verifiable dump.

The directed test validates registration/release and unload rejection, then compiles and executes
a real constructor/destructor path at `-O0` through NSLink. No baseline or broad suite was run.
