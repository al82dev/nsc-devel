"""Run focused source-field regressions and retain reproducible evidence on A:."""
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
    if len(sys.argv) != 2:
        print('usage: static_field_source_fields.py <nsc>', file=sys.stderr)
        return 2
    compiler = Path(sys.argv[1]).resolve()
    temp = os.environ.get('NSC_TEST_TEMP_ROOT') or os.environ.get('TEMP')
    if not temp or not Path(temp).is_dir() or Path(temp).resolve().drive.upper() != 'A:':
        print('SKIP: NSC_TEST_TEMP_ROOT or TEMP must name an existing directory on A:')
        return 77
    root = Path(tempfile.mkdtemp(prefix='nsc-source-fields-', dir=temp))
    print('EVIDENCE', root, flush=True)
    env = os.environ.copy()
    for key in ('TEMP', 'TMP', 'APPDATA', 'LOCALAPPDATA'):
        env[key] = str(root)
    base = Path(__file__).resolve().parent
    standard = base / 'static_field_query'
    expanded = base / 'static_field_query_expanded'
    cases = {
        'heterogeneous': (standard, None), 'empty': (standard, None),
        'sibling_scopes': (standard, None), 'inherited_visibility': (standard, None),
        'receiver_once': (standard, None), 'existing_type_name': (standard, None),
        'class_simple': (standard, None), 'private_inside': (standard, None),
        'private_outside': (standard, None), 'protected_derived_this': (standard, None),
        'compound_field': (standard, None), 'generic_struct': (expanded, None),
        'generic_class': (expanded, None), 'private_type_inside': (expanded, None),
        'private_type_shadow_inside': (expanded, None),
        'nested-manual': (expanded / 'nested_fields', None),
        'nested-nested': (expanded / 'nested_fields', None),
        'nested-nested-three': (expanded / 'nested_fields', None),
        'nested-nested-inherited': (expanded / 'nested_fields', None),
        'nested-shadow': (expanded / 'nested_fields', None),
        'lexical-shadow': (expanded / 'nested_fields', None),
        'nested-shadow-restored': (expanded / 'nested_fields', None),
        'shadow-type-distinct': (expanded / 'nested_fields', None),
        'outer-type-header': (expanded / 'nested_fields', None),
        'descriptor-escape': (expanded / 'nested_fields', "expected ';', found 'escaped'"),
        'nested-shadow-wrong-receiver': (expanded / 'nested_fields',
                                         'static field selection does not match an accessible field on this receiver'),
        'nested-wrong-receiver': (expanded / 'nested_fields',
                                  'static field selection does not match an accessible field on this receiver'),
        'nested-nested-three-wrong': (expanded / 'nested_fields',
                                      'static field selection does not match an accessible field on this receiver'),
        'nested-empty-invalid': (expanded / 'nested_fields', 'unsupported static field descriptor member'),
        'nested-flow-invalid': (expanded / 'nested_fields', None),
        'flow-ordinary-break': (expanded / 'static_flow', None),
        'flow-static-break': (expanded / 'static_flow', None),
        'flow-ordinary-continue': (expanded / 'static_flow', None),
        'flow-static-continue': (expanded / 'static_flow', None),
        'flow-inner-while-continue': (expanded / 'static_flow', None),
        'flow-inner-while-break': (expanded / 'static_flow', None),
        'flow-zero-fields-break': (expanded / 'static_flow', None),
        'flow-one-field-break': (expanded / 'static_flow', None),
        'flow-nested-break': (expanded / 'static_flow', None),
        'flow-static-break-cleanup': (expanded / 'static_flow', None),
        'flow-static-continue-cleanup': (expanded / 'static_flow', None),
        'flow-inner-switch-break': (expanded / 'static_flow', None),
        'flow-outer-labelled-break': (expanded / 'static_flow', None),
        'flow-outer-labelled-continue': (expanded / 'static_flow', None),
        'flow-outer-break-cleanup': (expanded / 'static_flow', None),
        'flow-outer-continue-cleanup': (expanded / 'static_flow', None),
        'flow-nested-outer-break': (expanded / 'static_flow', None),
        'flow-deferred-outer-break': (expanded / 'static_flow', None),
        'flow-missing-label': (expanded / 'static_flow',
                               'break or continue cannot target a static field iteration or an outer scope'),
        'flow-empty-missing-label': (expanded / 'static_flow',
                                     'break or continue cannot target a static field iteration or an outer scope'),
        'flow-empty-inner-break-value': (expanded / 'static_flow',
                                         'cannot verify an outer break value in an empty selection'),
        'flow-empty-outer-break-value': (expanded / 'static_flow',
                                         'cannot verify an outer break value in an empty selection'),
        'flow-lambda-outer-break': (expanded / 'static_flow',
                                    'break or continue inside a static field lambda is not supported'),
        'manual-explicit-capture': (expanded / 'lambda_fields', None),
        'lambda-explicit-receiver': (expanded / 'lambda_fields', None),
        'lambda-explicit-get': (expanded / 'lambda_fields', None),
        'lambda-descriptor-parameter': (expanded / 'lambda_fields', None),
        'lambda-no-descriptor': (expanded / 'lambda_fields', None),
        'lambda-descriptor-escape': (expanded / 'lambda_fields', 'unsupported static field descriptor expression'),
        'lambda-loop-ordinary': (expanded / 'lambda_fields', None),
        'lambda-loop-fields': (expanded / 'lambda_fields',
                               'break or continue inside a static field lambda is not supported'),
        'manual-identity': (expanded / 'generic_lambda_fields', None),
        'fields-identity': (expanded / 'generic_lambda_fields', None),
        'manual-name': (expanded / 'generic_lambda_fields', None),
        'fields-name': (expanded / 'generic_lambda_fields', None),
        'hetero-manual': (expanded / 'generic_lambda_fields', None),
        'hetero-fields': (expanded / 'generic_lambda_fields', None),
        'generic-wrong-receiver': (expanded / 'generic_lambda_fields',
                                   'static field selection does not match an accessible field on this receiver'),
        'generic-shadow': (expanded / 'generic_lambda_fields', 'static field body has unsupported descriptor type syntax'),
        'generic-descriptor-escape': (expanded / 'generic_lambda_fields', 'unsupported static field descriptor expression'),
        'nameof-values': (expanded / 'nameof_fields', None),
        'nameof-heterogeneous-values': (expanded / 'nameof_fields', None),
        'nameof-shadow-values': (expanded / 'nameof_fields', None),
        'nameof-side-effect': (expanded / 'nameof_fields', None),
        'nameof-descriptor-get-method': (expanded / 'nameof_fields', None),
        'nameof-descriptor-get-noargs': (expanded / 'nameof_fields', None),
        'nameof-ordinary-method-group': (expanded / 'nameof_fields', None),
        'nameof-descriptor-missing': (expanded / 'nameof_fields', 'static field descriptor inside nameof is not supported'),
        'nameof-empty-descriptor': (expanded / 'nameof_fields', 'static field descriptor inside nameof is not supported'),
        'nameof-descriptor-outside': (expanded / 'nameof_fields', "unknown identifier 'field'"),
        'tuple-ordinary': (expanded / 'source_tuple', None),
        'tuple-name': (expanded / 'source_tuple', None),
        'tuple-get': (expanded / 'source_tuple', None),
        'tuple-type': (expanded / 'source_tuple', None),
        'tuple-named-ordinary': (expanded / 'source_tuple', None),
        'tuple-named-type': (expanded / 'source_tuple', None),
        'tuple-namespace-ordinary': (expanded / 'source_tuple', None),
        'tuple-namespace-type': (expanded / 'source_tuple', None),
        'tuple-homonym-ordinary': (expanded / 'source_tuple', None),
        'tuple-homonym-fields': (expanded / 'source_tuple', None),
        'tuple-private-ordinary': (expanded / 'source_tuple', None),
        'tuple-private-type': (expanded / 'source_tuple', None),
        'tuple-private-outside-ordinary': (expanded / 'source_tuple',
                                           "nested type 'Outer+P' is private"),
        'tuple-private-outside-fields': (expanded / 'source_tuple',
                                         "static field iteration cannot verify this type's complete field declarations"),
        'tuple-wrong-receiver': (expanded / 'source_tuple',
                                 'static field selection does not match an accessible field on this receiver'),
        'Owner': (expanded / 'generic_protected', None),
        'OwnerOrdinary': (expanded / 'generic_protected', None),
        'Outside': (expanded / 'generic_protected', None),
        'OutsideOrdinary': (expanded / 'generic_protected', None),
        'Derived': (expanded / 'generic_protected', None),
        'DerivedOrdinary': (expanded / 'generic_protected', None),
        'DerivedBaseInvalid': (expanded / 'generic_protected',
                               'static field selection does not match an accessible field on this receiver'),
        'DerivedBaseOrdinaryInvalid': (expanded / 'generic_protected',
                                       "member 'Hidden' is not accessible through receiver/type"),
        'ordinary': (expanded / 'local_generic_inheritance', None),
        'query': (expanded / 'local_generic_inheritance', None),
        'homonym-ordinary': (expanded / 'local_generic_inheritance', None),
        'homonym-ab': (expanded / 'local_generic_inheritance', None),
        'homonym-ba': (expanded / 'local_generic_inheritance', None),
        'receiver-base': (expanded / 'local_generic_inheritance',
                          'static field selection does not match an accessible field on this receiver'),
        'homonym-receiver': (expanded / 'local_generic_inheritance',
                             'static field selection does not match an accessible field on this receiver'),
        'private-ordinary-invalid': (expanded / 'local_generic_inheritance',
                                     "member 'Hidden' is not accessible through receiver/type"),
        'nominal-ordinary': (expanded / 'local_nominal_inheritance', None),
        'nominal-query': (expanded / 'local_nominal_inheritance', None),
        'nominal-homonym-ordinary': (expanded / 'local_nominal_inheritance', None),
        'nominal-homonym-ab': (expanded / 'local_nominal_inheritance', None),
        'nominal-homonym-ba': (expanded / 'local_nominal_inheritance', None),
        'nominal-receiver-base': (expanded / 'local_nominal_inheritance',
                                  'static field selection does not match an accessible field on this receiver'),
        'nominal-homonym-receiver': (expanded / 'local_nominal_inheritance',
                                     'static field selection does not match an accessible field on this receiver'),
        'nominal-private-ordinary-invalid': (expanded / 'local_nominal_inheritance',
                                             "member 'Hidden' is not accessible through receiver/type"),
        'invalid_descriptor_scope': (standard, "expected ';', found 'escaped'"),
        'invalid_empty_descriptor': (standard, 'unsupported static field descriptor member'),
        'protected_base_receiver_invalid': (standard, 'static field selection does not match an accessible field on this receiver'),
    }
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

    for name, (source_dir, diagnostic) in cases.items():
        folder = root / name
        folder.mkdir()
        shutil.copyfile(source_dir / (name + '.ns'), folder / 'Main.ns')
        project = folder / 'Test.nsproj'
        project.write_text('<Project><PropertyGroup><Name>SourceFieldsConsumer</Name><OutputType>Exe</OutputType>'
                           '</PropertyGroup><ItemGroup><Compile Include="Main.ns"/></ItemGroup></Project>',
                           encoding='utf-8')
        for level in ('O0', 'O2'):
            executable = folder / ('test-' + level + '.exe')
            code, output = invoke(name, level + '-build', [compiler, '--emit-project-oop-exe', project,
                '-' + level, '-o', executable, '--artifact-cache=' + str(folder / 'cache')])
            if diagnostic is not None:
                if not code or diagnostic not in output:
                    failures.append((name, level, 'expected-diagnostic'))
                continue
            if code or not executable.is_file():
                failures.append((name, level, 'build'))
                continue
            code, output = invoke(name, level + '-imports', ['llvm-readobj', '--coff-imports', executable])
            names = {item.lower() for item in re.findall(r'^\s*Name:\s*(\S+)', output, re.MULTILINE)}
            allowed = {'kernel32.dll', 'kernelbase.dll', 'ucrtbase.dll', 'shell32.dll'}
            if code or not names or not names <= allowed:
                failures.append((name, level, 'imports'))
                continue
            code, _ = invoke(name, level + '-execution', [executable])
            if code:
                failures.append((name, level, 'execution'))
    reference_fixtures = expanded / 'source_project_inheritance'
    project_references = {}
    for name in ('generic-producer', 'beta-producer', 'internal-producer'):
        folder = root / name
        folder.mkdir()
        shutil.copyfile(reference_fixtures / (name + '.ns'), folder / 'Main.ns')
        project = folder / 'Producer.nsproj'
        project.write_text('<Project><PropertyGroup><Name>Producer' + name.replace('-', '') +
                           '</Name><OutputType>StaticLibrary</OutputType></PropertyGroup>'
                           '<ItemGroup><Compile Include="Main.ns"/></ItemGroup></Project>', encoding='utf-8')
        project_references[name] = project
    project_cases = {
        'generic-ordinary': (('generic-producer',), None),
        'generic-query': (('generic-producer',), None),
        'generic-private-invalid': (('generic-producer',), "member 'Hidden' is not accessible through receiver/type"),
        'homonym-ordinary': (('generic-producer', 'beta-producer'), None),
        'homonym-query': (('generic-producer', 'beta-producer'), None),
        'generic-hidden-ordinary': (('generic-producer',), None),
        'generic-hidden-query': (('generic-producer',), None),
        'generic-receiver-base': (('generic-producer',),
                                  'static field selection does not match an accessible field on this receiver'),
        'homonym-receiver-other': (('generic-producer', 'beta-producer'),
                                   'static field selection does not match an accessible field on this receiver'),
        'internal-ordinary': (('internal-producer',), 'module-visible'),
        'internal-query': (('internal-producer',), 'static field iteration cannot verify the complete class hierarchy'),
    }
    for name, (references, diagnostic) in project_cases.items():
        case = 'project-' + name
        folder = root / case
        folder.mkdir()
        shutil.copyfile(reference_fixtures / (name + '.ns'), folder / 'Main.ns')
        project = folder / 'Test.nsproj'
        reference_xml = ''.join('<ProjectReference Include="' + project_references[item].as_posix() + '"/>'
                                for item in references)
        project.write_text('<Project><PropertyGroup><Name>ProjectFieldsConsumer</Name><OutputType>Exe</OutputType>'
                           '</PropertyGroup><ItemGroup><Compile Include="Main.ns"/>' + reference_xml +
                           '</ItemGroup></Project>', encoding='utf-8')
        for level in ('O0', 'O2'):
            executable = folder / ('test-' + level + '.exe')
            code, output = invoke(case, level + '-build', [compiler, '--emit-project-oop-exe', project,
                '-' + level, '-o', executable, '--artifact-cache=' + str(folder / 'cache')])
            if diagnostic is not None:
                if not code or diagnostic not in output:
                    failures.append((case, level, 'expected-diagnostic'))
                continue
            if code or not executable.is_file():
                failures.append((case, level, 'build'))
                continue
            code, output = invoke(case, level + '-imports', ['llvm-readobj', '--coff-imports', executable])
            names = {item.lower() for item in re.findall(r'^\s*Name:\s*(\S+)', output, re.MULTILINE)}
            allowed = {'kernel32.dll', 'kernelbase.dll', 'ucrtbase.dll', 'shell32.dll'}
            if code or not names or not names <= allowed:
                failures.append((case, level, 'imports'))
                continue
            code, _ = invoke(case, level + '-execution', [executable])
            if code:
                failures.append((case, level, 'execution'))
    after = hashlib.sha256(compiler.read_bytes()).hexdigest()
    if before != after:
        failures.append(('compiler', 'hash-changed'))
    (root / 'results.json').write_text(json.dumps(dict(compiler_before=before, compiler_after=after,
        records=records, failures=failures), indent=2), encoding='utf-8')
    if failures:
        print('FAIL', failures, file=sys.stderr)
    return int(bool(failures))


if __name__ == '__main__':
    sys.exit(main())
