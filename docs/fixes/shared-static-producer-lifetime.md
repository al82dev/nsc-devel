# Shared pointers with a static producer

The directed control builds a NativeSharp static library defining `Payload`, then
builds separate consumers using `Rc` and `Arc` against that binary and an
independently supplied Core library. Both producer and consumer are tested at
O0 and O2. Each consumer checks the live value and counts, the single destructor
invocation after the strong owner leaves scope, and expiration of its still-live
weak reference. It never accesses the payload after destruction.

The CTest `nsc.compiler.shared-static-producer-lifetime` requires
`NSC_TEST_CORE_BINARY` to identify a Core library with its adjacent `.nsmeta`.
It uses an existing directory on A: from `NSC_TEST_TEMP_ROOT` or `TEMP`, and
reports an explicit skip (77) if either prerequisite is absent. The compiler
path comes from the test configuration; all subprocesses start in the source
repository. The configured compiler and Core must be selected for the tree
under test; this test does not itself establish their provenance.
The test records SHA-256 for compiler, Core and metadata before and after, and
fails if an input changes during the run. Its directed configured and skip
logs are `A:/nsc-fixes-agente-2-evidencia/shared_static_producer_lifetime-hashed-configured-20261112.log`
and `A:/nsc-fixes-agente-2-evidencia/shared_static_producer_lifetime-hashed-missing-core-20261112.log`.

The original exploratory control passed two producer builds and eight consumer
builds and runs using a work-in-progress compiler and preexisting isolated Core;
its detailed commands, hashes and outputs are in
`A:\nsc-fixes-agente-2-evidencia\shared-static-producer-lifetime-20261112\run-20261112T040303876974Z\results.json`.
Those results do not establish behavior of a clean revision. A static library
becomes part of the executable: this control neither exercises module unloading
nor establishes that a dynamically loaded module is pinned.
