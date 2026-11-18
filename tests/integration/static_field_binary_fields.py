"""Exercise binary field selection with preserved artifacts on the isolated A: volume."""
from pathlib import Path
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile


def main():
    certificate_only = len(sys.argv) == 4 and sys.argv[2] == '--inherited-certificate'
    if len(sys.argv) != 2 and not certificate_only:
        print('usage: static_field_binary_fields.py <nsc> [--inherited-certificate <unit-exe>]', file=sys.stderr)
        return 2
    compiler = Path(sys.argv[1]).resolve()
    root_name = os.environ.get('NSC_TEST_TEMP_ROOT') or os.environ.get('TEMP')
    if not root_name or not Path(root_name).is_dir() or Path(root_name).resolve().drive.upper() != 'A:':
        print('SKIP: NSC_TEST_TEMP_ROOT or TEMP must name an existing directory on A:')
        return 77
    root = Path(tempfile.mkdtemp(prefix='nsc-binary-fields-', dir=root_name))
    print('EVIDENCE', root, flush=True)
    env = os.environ.copy()
    for key in ('TEMP', 'TMP', 'APPDATA', 'LOCALAPPDATA'):
        env[key] = str(root)
    fixtures = Path(__file__).resolve().parent / 'static_field_query_expanded'
    before = hashlib.sha256(compiler.read_bytes()).hexdigest()
    records, failures = [], []

    def invoke(case, phase, command):
        command = [str(item) for item in command]
        try:
            result = subprocess.run(command, cwd=root, env=env, capture_output=True,
                                    text=True, errors='replace', timeout=120)
            code, output = result.returncode, result.stdout + result.stderr
        except subprocess.TimeoutExpired:
            code, output = 124, 'TIMEOUT after 120 seconds\n'
        except OSError as error:
            code, output = 127, 'PROCESS LAUNCH FAILED: ' + str(error) + '\n'
        records.append(dict(case=case, phase=phase, command=command, exit=code, output=output))
        print(case, phase, code, flush=True)
        return code, output

    def project(folder, name, output, reference=''):
        result = folder / 'Test.nsproj'
        result.write_text('<Project><PropertyGroup><Name>' + name + '</Name><OutputType>' + output +
                          '</OutputType></PropertyGroup><ItemGroup><Compile Include="Main.ns"/>' + reference +
                          '</ItemGroup></Project>', encoding='utf-8')
        return result

    def producer(case, fixture, source, libraries=()):
        folder = root / (case + '-producer')
        folder.mkdir()
        shutil.copyfile(fixtures / fixture / source, folder / 'Main.ns')
        lib = folder / 'Producer.lib'
        reference = ''.join('<LibraryReference Include="' + item.as_posix() + '" Metadata="' +
                            item.with_suffix('.nsmeta').as_posix() + '"/>' for item in libraries)
        command = [compiler, '--emit-project-oop-lib', project(folder, case + 'Producer', 'StaticLibrary', reference),
                   '-O0', '-o', lib, '--time', '--artifact-cache=' + str(folder / 'cache')]
        code, output = invoke(case, 'producer', command)
        if code or not lib.is_file() or not lib.with_suffix('.nsmeta').is_file():
            failures.append((case, 'producer'))
            return None
        return lib

    def consumer(case, fixture, source, lib, accept, require_snapshot=False, diagnostic=None, require_pruned=False):
        folder = root / case
        folder.mkdir()
        shutil.copyfile(fixtures / fixture / source, folder / 'Main.ns')
        libraries = lib if isinstance(lib, list) else [lib]
        reference = ''.join('<LibraryReference Include="' + item.as_posix() + '" Metadata="' +
                            item.with_suffix('.nsmeta').as_posix() + '"/>' for item in libraries)
        target = project(folder, case.replace('-', '_') + 'Consumer', 'Exe', reference)
        for level in ('O0', 'O2'):
            exe = folder / ('test-' + level + '.exe')
            code, output = invoke(case, level + '-build', [compiler, '--emit-project-oop-exe', target,
                '-' + level, '-o', exe, '--time', '--artifact-cache=' + str(folder / 'cache')])
            if not accept:
                if not code or not diagnostic or diagnostic not in output:
                    failures.append((case, level, 'expected-selection-rejection'))
                continue
            if code or not exe.is_file():
                failures.append((case, level, 'build'))
                continue
            if require_snapshot and 'precompiled-snapshot: loaded=1' not in output:
                failures.append((case, level, 'snapshot-not-observed'))
            if require_pruned and 'fallback=1' not in output:
                failures.append((case, level, 'pruned-fallback-not-observed'))
            if require_pruned and '=pruned' not in output:
                failures.append((case, level, 'pruned-reason-not-observed'))
            code, imports = invoke(case, level + '-imports', ['llvm-readobj', '--coff-imports', exe])
            names = {name.lower() for name in re.findall(r'^\s*Name:\s*(\S+)', imports, re.MULTILINE)}
            allowed = {'kernel32.dll', 'kernelbase.dll', 'ucrtbase.dll', 'shell32.dll'}
            if code or not names or not names <= allowed:
                failures.append((case, level, 'imports'))
                continue
            code, _ = invoke(case, level + '-execution', [exe])
            if code:
                failures.append((case, level, 'execution'))

    if certificate_only:
        unit_before = unit_after = None
        lib = producer('InheritedHidden', 'binary_generic', 'InheritedHiddenProducer.ns')
        if lib is not None:
            consumer('generic-inherited-hidden', 'binary_generic', 'InheritedHiddenConsumer.ns', lib, True)
            env['NSC_TEST_INHERITED_CERT_PROJECT'] = str(root / 'generic-inherited-hidden' / 'Test.nsproj')
            generic_lib = producer('GenericBase', 'binary_generic', 'GenericBaseProducer.ns')
            if generic_lib is not None:
                consumer('generic-base', 'binary_generic', 'GenericBaseConsumer.ns', generic_lib, True)
                env['NSC_TEST_GENERIC_BASE_CERT_PROJECT'] = str(root / 'generic-base' / 'Test.nsproj')
            unit = Path(sys.argv[3]).resolve()
            unit_before = hashlib.sha256(unit.read_bytes()).hexdigest()
            code, output = invoke('inherited-certificate', 'unit', [unit])
            unit_after = hashlib.sha256(unit.read_bytes()).hexdigest()
            if (code or 'SKIP inherited generic certificates' in output or
                    'SKIP generic base template certificates' in output or unit_before != unit_after):
                failures.append(('inherited-certificate', 'unit'))
        after = hashlib.sha256(compiler.read_bytes()).hexdigest()
        if before != after:
            failures.append(('compiler', 'hash-changed'))
        (root / 'results.json').write_text(json.dumps(dict(compiler_before=before, compiler_after=after,
            unit_before=unit_before, unit_after=unit_after,
            artifacts={str(path.relative_to(root)): hashlib.sha256(path.read_bytes()).hexdigest()
                       for path in root.rglob('*') if path.is_file() and path.suffix in ('.lib', '.nsmeta', '.nsproducts')},
            records=records, failures=failures), indent=2), encoding='utf-8')
        if failures:
            print('FAIL', failures, file=sys.stderr)
        return int(bool(failures))

    basic = producer('Basic', 'binary', 'Producer.ns')
    if basic is not None:
        consumer('basic-snapshot', 'binary', 'Consumer.ns', basic, True, require_snapshot=True)
        consumer('basic-ordinary', 'binary', 'ConsumerOrdinary.ns', basic, True)
        isolated = root / 'basic-no-sidecar-producer'
        isolated.mkdir()
        copied = isolated / 'Producer.lib'
        shutil.copyfile(basic, copied)
        shutil.copyfile(basic.with_suffix('.nsmeta'), copied.with_suffix('.nsmeta'))
        consumer('basic-no-sidecar', 'binary', 'Consumer.ns', copied, True)
    nominal = producer('Nominal', 'binary_nominal', 'Producer.ns')
    if nominal is not None:
        consumer('nominal-query', 'binary_nominal', 'Consumer.ns', nominal, True)
        consumer('nominal-ordinary', 'binary_nominal', 'ConsumerOrdinary.ns', nominal, True)
    generic = producer('Generic', 'binary_generic', 'Producer.ns')
    if generic is not None:
        consumer('generic-snapshot', 'binary_generic', 'Consumer.ns', generic, True, require_snapshot=True)
        consumer('generic-ordinary', 'binary_generic', 'ConsumerOrdinary.ns', generic, True)
        isolated = root / 'generic-no-sidecar-producer'
        isolated.mkdir()
        copied = isolated / 'Producer.lib'
        shutil.copyfile(generic, copied)
        shutil.copyfile(generic.with_suffix('.nsmeta'), copied.with_suffix('.nsmeta'))
        consumer('generic-no-sidecar', 'binary_generic', 'Consumer.ns', copied, True)
    alpha = producer('Alpha', 'binary_identity', 'Alpha.ns')
    beta = producer('Beta', 'binary_identity', 'Beta.ns')
    if alpha is not None and beta is not None:
        pair = [alpha, beta]
        consumer('identity-alpha-beta', 'binary_identity', 'AlphaThenBeta.ns', pair, True)
        consumer('identity-beta-alpha', 'binary_identity', 'BetaThenAlpha.ns', pair, True)
        consumer('identity-ordinary', 'binary_identity', 'Ordinary.ns', pair, True)
    inherited = producer('Inherited', 'binary_generic', 'InheritedProducer.ns')
    if inherited is not None:
        consumer('generic-inherited', 'binary_generic', 'InheritedConsumer.ns', inherited, True)
        consumer('generic-inherited-ordinary', 'binary_generic', 'InheritedOrdinary.ns', inherited, True)
        isolated = root / 'generic-inherited-no-sidecar-producer'
        isolated.mkdir()
        copied = isolated / 'Producer.lib'
        shutil.copyfile(inherited, copied)
        shutil.copyfile(inherited.with_suffix('.nsmeta'), copied.with_suffix('.nsmeta'))
        consumer('generic-inherited-no-sidecar', 'binary_generic', 'InheritedConsumer.ns', copied, True)
    inherited_hidden = producer('InheritedHidden', 'binary_generic', 'InheritedHiddenProducer.ns')
    if inherited_hidden is not None:
        consumer('generic-inherited-hidden', 'binary_generic', 'InheritedHiddenConsumer.ns', inherited_hidden, True)
    generic_base = producer('GenericBase', 'binary_generic', 'GenericBaseProducer.ns')
    if generic_base is not None:
        consumer('generic-base', 'binary_generic', 'GenericBaseConsumer.ns', generic_base, True)
        isolated = root / 'generic-base-no-sidecar-producer'
        isolated.mkdir()
        copied = isolated / 'Producer.lib'
        shutil.copyfile(generic_base, copied)
        shutil.copyfile(generic_base.with_suffix('.nsmeta'), copied.with_suffix('.nsmeta'))
        consumer('generic-base-no-sidecar', 'binary_generic', 'GenericBaseConsumer.ns', copied, True)
    generic_base_alpha = producer('GenericBaseAlpha', 'binary_generic_base_security', 'Alpha.ns')
    generic_base_beta = producer('GenericBaseBeta', 'binary_generic_base_security', 'Beta.ns')
    if generic_base_alpha is not None and generic_base_beta is not None:
        pair = [generic_base_alpha, generic_base_beta]
        consumer('generic-base-hidden', 'binary_generic_base_security', 'Hidden.ns', pair, True)
        consumer('generic-base-homonym-ab', 'binary_generic_base_security', 'HomonymAB.ns', pair, True)
        consumer('generic-base-homonym-ba', 'binary_generic_base_security', 'HomonymBA.ns', pair, True)
        for label, fixture in (('unrelated', 'ReceiverUnrelated.ns'), ('base', 'ReceiverBase.ns'),
                               ('other-module', 'ReceiverOtherModule.ns')):
            consumer('generic-base-receiver-' + label, 'binary_generic_base_security', fixture, pair, False,
                     diagnostic='static field selection does not match an accessible field on this receiver')
        consumer('generic-base-private-invalid', 'binary_generic_base_security', 'PrivateInvalid.ns', pair, False,
                 diagnostic="member 'Hidden' is not accessible through receiver/type")
    external_alpha = producer('ExternalAlpha', 'binary_external_generic_base', 'Alpha.ns')
    external_beta = producer('ExternalBeta', 'binary_external_generic_base', 'Beta.ns')
    if external_alpha is not None and external_beta is not None:
        external_child = producer('ExternalChild', 'binary_external_generic_base', 'Child.ns',
                                  [external_alpha, external_beta])
        if external_child is not None:
            libraries = [external_alpha, external_beta, external_child]
            for label, source in (('query', 'Consumer.ns'), ('ordinary', 'Ordinary.ns'),
                                  ('homonym', 'Homonym.ns'), ('homonym-ordinary', 'HomonymOrdinary.ns')):
                consumer('external-base-' + label, 'binary_external_generic_base', source, libraries, True)
            without_sidecars = []
            for label, library in zip(('alpha', 'beta', 'child'), libraries):
                folder = root / ('external-base-no-sidecar-' + label)
                folder.mkdir()
                copied = folder / 'Producer.lib'
                shutil.copyfile(library, copied)
                shutil.copyfile(library.with_suffix('.nsmeta'), copied.with_suffix('.nsmeta'))
                without_sidecars.append(copied)
            consumer('external-base-no-sidecar', 'binary_external_generic_base', 'Consumer.ns', without_sidecars, True)
            consumer('external-base-homonym-no-sidecar', 'binary_external_generic_base', 'Homonym.ns', without_sidecars, True)
    nominal_alpha = producer('NominalExternalAlpha', 'binary_external_nominal_base', 'Alpha.ns')
    nominal_beta = producer('NominalExternalBeta', 'binary_external_nominal_base', 'Beta.ns')
    if nominal_alpha is not None and nominal_beta is not None:
        nominal_child = producer('NominalExternalChild', 'binary_external_nominal_base', 'Child.ns',
                                 [nominal_alpha, nominal_beta])
        if nominal_child is not None:
            libraries = [nominal_alpha, nominal_beta, nominal_child]
            for label, source in (('query', 'Consumer.ns'), ('ordinary', 'Ordinary.ns'),
                                  ('homonym', 'Homonym.ns'), ('homonym-ordinary', 'HomonymOrdinary.ns')):
                consumer('nominal-external-' + label, 'binary_external_nominal_base', source, libraries, True)
            without_sidecars = []
            for label, library in zip(('alpha', 'beta', 'child'), libraries):
                folder = root / ('nominal-external-no-sidecar-' + label)
                folder.mkdir()
                copied = folder / 'Producer.lib'
                shutil.copyfile(library, copied)
                shutil.copyfile(library.with_suffix('.nsmeta'), copied.with_suffix('.nsmeta'))
                without_sidecars.append(copied)
            consumer('nominal-external-no-sidecar', 'binary_external_nominal_base', 'Consumer.ns', without_sidecars, True)
            consumer('nominal-external-homonym-no-sidecar', 'binary_external_nominal_base', 'Homonym.ns', without_sidecars, True)
    chain_alpha = producer('ChainAlpha', 'binary_external_nominal_chain', 'Alpha.ns')
    chain_beta = producer('ChainBeta', 'binary_external_nominal_chain', 'Beta.ns')
    if chain_alpha is not None and chain_beta is not None:
        chain_middle = producer('ChainMiddle', 'binary_external_nominal_chain', 'Middle.ns', [chain_alpha, chain_beta])
        if chain_middle is not None:
            chain_leaf = producer('ChainLeaf', 'binary_external_nominal_chain', 'Leaf.ns',
                                  [chain_alpha, chain_beta, chain_middle])
            if chain_leaf is not None:
                libraries = [chain_alpha, chain_beta, chain_middle, chain_leaf]
                consumer('nominal-chain', 'binary_external_nominal_chain', 'Consumer.ns', libraries, True)
                consumer('nominal-chain-ordinary', 'binary_external_nominal_chain', 'Ordinary.ns', libraries, True)
                for label, source in (('base', 'ReceiverBase.ns'), ('other-module', 'ReceiverOtherModule.ns')):
                    consumer('nominal-chain-receiver-' + label, 'binary_external_nominal_chain', source,
                             libraries, False,
                             diagnostic='static field selection does not match an accessible field on this receiver')
    binary_class = producer('Class', 'metadata', 'Producer.ns')
    if binary_class is not None:
        consumer('class-free', 'metadata', 'FreeMain.ns', binary_class, True, require_snapshot=True)
        consumer('class-free-ordinary', 'metadata', 'FreeMainOrdinary.ns', binary_class, True)
        isolated = root / 'class-no-sidecar-producer'
        isolated.mkdir()
        copied = isolated / 'Producer.lib'
        shutil.copyfile(binary_class, copied)
        shutil.copyfile(binary_class.with_suffix('.nsmeta'), copied.with_suffix('.nsmeta'))
        consumer('class-free-no-sidecar', 'metadata', 'FreeMain.ns', copied, True)
        consumer('class-inspector', 'metadata', 'Both.ns', binary_class, True, require_snapshot=True)
        consumer('class-inspector-no-sidecar', 'metadata', 'Both.ns', copied, True)
        consumer('class-inspector-ordinary', 'metadata', 'BothOrdinary.ns', binary_class, True)
        consumer('class-inspector-base', 'metadata', 'InspectorBase.ns', binary_class, True)
        consumer('class-inspector-base-ordinary', 'metadata', 'InspectorBaseOrdinary.ns', binary_class, True)
        consumer('class-inspector-interface', 'metadata', 'InspectorInterface.ns', binary_class, True)
        consumer('class-inspector-interface-ordinary', 'metadata', 'InspectorInterfaceOrdinary.ns', binary_class, True)
        consumer('class-derived', 'metadata', 'Derived.ns', binary_class, False,
                 diagnostic="static field iteration cannot verify this type's complete field declarations")
        consumer('class-derived-ordinary', 'metadata', 'DerivedOrdinary.ns', binary_class, True)
        consumer('class-private-invalid', 'metadata', 'PrivateAccessInvalid.ns', binary_class, False,
                 diagnostic="member 'Hidden' is not accessible through receiver/type")
    boundary = producer('Boundary', 'binary_scope_boundaries', 'Producer.ns')
    if boundary is not None:
        for stem in ('UnrelatedDirect', 'UnrelatedTransitive', 'HomonymQualified', 'BinaryUnrelated'):
            consumer('boundary-' + stem, 'binary_scope_boundaries', stem + '.ns', boundary, True)
            consumer('boundary-' + stem + '-ordinary', 'binary_scope_boundaries', stem + 'Ordinary.ns', boundary, True)
        for stem in ('BinaryDirect', 'BinaryTransitive', 'Partial'):
            consumer('boundary-' + stem, 'binary_scope_boundaries', stem + '.ns', boundary, False,
                     diagnostic="static field iteration cannot verify this type's complete field declarations")
            consumer('boundary-' + stem + '-ordinary', 'binary_scope_boundaries', stem + 'Ordinary.ns', boundary, True)
    hierarchy = producer('Hierarchy', 'binary_hierarchy', 'Producer.ns')
    if hierarchy is not None:
        consumer('hierarchy-external', 'binary_hierarchy', 'External.ns', hierarchy, True, require_snapshot=True)
        consumer('hierarchy-external-ordinary', 'binary_hierarchy', 'ExternalOrdinary.ns', hierarchy, True)
        isolated = root / 'hierarchy-no-sidecar-producer'
        isolated.mkdir()
        copied = isolated / 'Producer.lib'
        shutil.copyfile(hierarchy, copied)
        shutil.copyfile(hierarchy.with_suffix('.nsmeta'), copied.with_suffix('.nsmeta'))
        consumer('hierarchy-external-no-sidecar', 'binary_hierarchy', 'External.ns', copied, True)
        consumer('hierarchy-derived', 'binary_hierarchy', 'Derived.ns', hierarchy, False,
                 diagnostic="static field iteration cannot verify this type's complete field declarations")
        consumer('hierarchy-derived-ordinary', 'binary_hierarchy', 'DerivedOrdinary.ns', hierarchy, True)
        consumer('hierarchy-receiver-invalid', 'binary_hierarchy', 'DerivedBaseInvalid.ns', hierarchy, False,
                 diagnostic="static field iteration cannot verify this type's complete field declarations")
        consumer('hierarchy-receiver-ordinary-invalid', 'binary_hierarchy', 'DerivedBaseOrdinaryInvalid.ns', hierarchy, False,
                 diagnostic="member 'Protected' is not accessible through receiver/type")
        consumer('hierarchy-protected-invalid', 'binary_hierarchy', 'ProtectedInvalid.ns', hierarchy, False,
                 diagnostic="member 'Protected' is not accessible through receiver/type")
        consumer('hierarchy-private-invalid', 'binary_hierarchy', 'PrivateInvalid.ns', hierarchy, False,
                 diagnostic="member 'Hidden' is not accessible through receiver/type")
    protected = producer('ProtectedHierarchy', 'binary_hierarchy', 'PublicProtectedProducer.ns')
    if protected is not None:
        consumer('protected-direct', 'binary_hierarchy', 'ProtectedDirect.ns', protected, True, require_pruned=True)
        consumer('protected-direct-ordinary', 'binary_hierarchy', 'ProtectedDirectOrdinary.ns', protected, True)
        consumer('protected-transitive', 'binary_hierarchy', 'ProtectedDerived.ns', protected, True, require_snapshot=True)
        consumer('protected-transitive-ordinary', 'binary_hierarchy', 'ProtectedDerivedOrdinary.ns', protected, True)
        isolated = root / 'protected-no-sidecar-producer'
        isolated.mkdir()
        copied = isolated / 'Producer.lib'
        shutil.copyfile(protected, copied)
        shutil.copyfile(protected.with_suffix('.nsmeta'), copied.with_suffix('.nsmeta'))
        consumer('protected-direct-no-sidecar', 'binary_hierarchy', 'ProtectedDirect.ns', copied, True)
        consumer('protected-transitive-no-sidecar', 'binary_hierarchy', 'ProtectedDerived.ns', copied, True)
        for label, source, ordinary in (
            ('direct', 'ProtectedDirectBaseInvalid.ns', 'ProtectedDirectBaseOrdinaryInvalid.ns'),
            ('transitive', 'ProtectedBaseInvalid.ns', 'ProtectedBaseOrdinaryInvalid.ns'),
        ):
            consumer('protected-' + label + '-receiver-invalid', 'binary_hierarchy', source, protected, False,
                     diagnostic='static field selection does not match an accessible field on this receiver')
            consumer('protected-' + label + '-ordinary-invalid', 'binary_hierarchy', ordinary, protected, False,
                     diagnostic="member 'Protected' is not accessible through receiver/type")
    for shape, producer_source, ordinary_source in (
        ('Tuple', 'TupleProducer.ns', 'Ordinary.ns'),
        ('TupleDependent', 'TupleDependentProducer.ns', 'TupleDependentOrdinary.ns'),
        ('Pointer', 'PointerProducer.ns', 'Ordinary.ns'),
    ):
        complex_lib = producer(shape, 'binary_complex', producer_source)
        if complex_lib is not None:
            consumer(shape.lower() + '-name', 'binary_complex', 'Query.ns', complex_lib, True)
            consumer(shape.lower() + '-typed', 'binary_complex', 'QueryTyped.ns', complex_lib, True)
            consumer(shape.lower() + '-ordinary', 'binary_complex', ordinary_source, complex_lib, True)
    private = producer('Private', 'binary_nominal', 'PrivateProducer.ns')
    if private is not None:
        consumer('private-query', 'binary_nominal', 'PrivateConsumer.ns', private, False,
                 diagnostic='static field type is not accessible in this context')
        consumer('private-ordinary', 'binary_nominal', 'PrivateConsumerOrdinary.ns', private, True)
        consumer('private-name', 'binary_nominal', 'PrivateName.ns', private, True)
        consumer('private-get', 'binary_nominal', 'PrivateGet.ns', private, True)
        consumer('private-ordinary-var', 'binary_nominal', 'PrivateOrdinaryVar.ns', private, True)
        consumer('private-ordinary-explicit', 'binary_nominal', 'PrivateOrdinaryExplicit.ns', private, False,
                 diagnostic='is declared in a referenced project but is module-visible')
    after = hashlib.sha256(compiler.read_bytes()).hexdigest()
    if before != after:
        failures.append(('compiler', 'hash-changed'))
    artifacts = {str(path.relative_to(root)): hashlib.sha256(path.read_bytes()).hexdigest()
                 for path in root.rglob('*') if path.is_file() and path.suffix in ('.lib', '.nsmeta', '.nsproducts')}
    (root / 'results.json').write_text(json.dumps(dict(compiler_before=before, compiler_after=after,
        artifacts=artifacts, records=records, failures=failures), indent=2), encoding='utf-8')
    if failures:
        print('FAIL', failures, file=sys.stderr)
    return int(bool(failures))


if __name__ == '__main__':
    sys.exit(main())
