"""Verify scalar ref/out view provenance across a NativeSharp binary boundary."""
from pathlib import Path
from xml.sax.saxutils import escape
import os
import subprocess
import sys
import tempfile


def main():
    compiler = Path(sys.argv[1]).resolve()
    core_name = os.environ.get('NSC_TEST_CORE_BINARY')
    temp_name = os.environ.get('NSC_TEST_TEMP_ROOT') or os.environ.get('TEMP')
    if not core_name or not Path(core_name).is_file() or not Path(core_name).with_suffix('.nsmeta').is_file():
        print('SKIP: NSC_TEST_CORE_BINARY must name an isolated Core library with metadata')
        return 77
    if not temp_name or Path(temp_name).drive.upper() != 'A:' or not Path(temp_name).is_dir():
        print('SKIP: NSC_TEST_TEMP_ROOT or TEMP must name an existing directory on A:')
        return 77
    core = Path(core_name).resolve()
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-binary-view-ref-out-', dir=temp_name) as temporary:
        root = Path(temporary)
        env = os.environ.copy()
        for key in ('TEMP', 'TMP', 'APPDATA', 'LOCALAPPDATA'):
            env[key] = str(root)

        def invoke(command):
            command = [str(item) for item in command]
            result = subprocess.run(command, cwd=root, env=env, capture_output=True,
                                    text=True, errors='replace', timeout=120)
            print('COMMAND', subprocess.list2cmdline(command), 'EXIT', result.returncode,
                  result.stdout + result.stderr, flush=True)
            return result

        def reference(path):
            return ('<LibraryReference Include="' + escape(path.as_posix(), {'"': '&quot;'}) +
                    '" Metadata="' + escape(path.with_suffix('.nsmeta').as_posix(), {'"': '&quot;'}) + '"/>')

        def project(name, files, libraries, output_type='Exe'):
            return ('<Project><PropertyGroup><Name>' + name + '</Name><OutputType>' + output_type +
                    '</OutputType></PropertyGroup><ItemGroup>' +
                    ''.join('<Compile Include="' + item + '"/>' for item in files) +
                    ''.join(reference(item) for item in libraries) + '</ItemGroup></Project>')

        (root / 'Producer.ns').write_text('''using NativeSharp.Core;
public class Payload { public int Data; public Payload() { Data = 42; } }
public class Sink {
 public static void SaveRef(SharedView<Payload> input, ref SharedView<Payload> output) { output = input; }
 public static void SaveOut(SharedView<Payload> input, out SharedView<Payload> output) { output = input; }
 public static void ForwardRef(ref SharedView<Payload> input, ref SharedView<Payload> output) { output = input; }
 public static void ForwardOut(ref SharedView<Payload> input, out SharedView<Payload> output) { output = input; }
}''', encoding='utf-8')
        producer = root / 'Producer.nsproj'
        producer.write_text(project('ViewOutputProducer', ['Producer.ns'], [core], 'StaticLibrary'), encoding='utf-8')
        if invoke([compiler, '--check-project-method-bodies', producer]).returncode:
            return 1
        for level in ('O0', 'O2'):
            library = root / ('producer-' + level + '.lib')
            if invoke([compiler, '--emit-project-oop-lib', producer, '-' + level, '-o', library,
                       '--no-artifact-cache']).returncode:
                failures.append((level, 'producer'))
                continue
            (root / 'Signature.ns').write_text('''using NativeSharp.Core;
class Program { public static int Main() {
 SharedView<Payload> input = default(SharedView<Payload>);
 SharedView<Payload> output = default(SharedView<Payload>);
 Sink.SaveRef(input, ref output); Sink.SaveOut(input, out output);
 Sink.ForwardRef(ref input, ref output); Sink.ForwardOut(ref input, out output);
 return 0; } }''', encoding='utf-8')
            signature = root / 'Signature.nsproj'
            signature.write_text(project('ViewOutputSignature', ['Signature.ns'], [core, library]), encoding='utf-8')
            if invoke([compiler, '--check-project-method-bodies', signature]).returncode:
                failures.append((level, 'binary-signature'))
                continue
            for family in ('Rc', 'Arc'):
                for mode in ('ref', 'out'):
                    for route in ('value', 'ref'):
                        for variant in ('live', 'escape', 'replace'):
                            owner = family + '<Payload> owner = SharedPointers.Make' + family + '<Payload>(); '
                            result = 'SharedView<Payload> result = default(SharedView<Payload>); '
                            method = ('Forward' if route == 'ref' else 'Save') + ('Ref' if mode == 'ref' else 'Out')
                            argument = 'ref input' if route == 'ref' else 'owner.Borrow()'
                            call = 'Sink.' + method + '(' + argument + ', ' + mode + ' result); '
                            if route == 'ref':
                                call = '{ SharedView<Payload> input = owner.Borrow(); ' + call + '} '
                            read = 'return result.Get().Data;'
                            body = (result + '{ ' + owner + call + '} ' + read if variant == 'escape' else
                                    owner + result + call +
                                    (('owner = SharedPointers.Make' + family + '<Payload>(); ') if variant == 'replace' else '') + read)
                            (root / 'Consumer.ns').write_text(
                                'using NativeSharp.Core; class Program { public static int Main() { ' + body + ' } }',
                                encoding='utf-8')
                            for source_mode in ('source', 'binary'):
                                label = (level, family, mode, route, variant, source_mode)
                                consumer = root / 'Consumer.nsproj'
                                consumer.write_text(project('ViewOutputConsumer',
                                    ['Producer.ns', 'Consumer.ns'] if source_mode == 'source' else ['Consumer.ns'],
                                    [core] if source_mode == 'source' else [core, library]), encoding='utf-8')
                                checked = invoke([compiler, '--check-project-method-bodies', consumer])
                                output = checked.stdout + checked.stderr
                                if variant != 'live':
                                    if checked.returncode == 0 or not any(message in output for message in
                                            ('view output cannot outlive its possible owner',
                                             'while a borrowed local is alive')):
                                        failures.append(label + ('wrong-rejection',))
                                    continue
                                if checked.returncode:
                                    failures.append(label + ('check',))
                                    continue
                                executable = root / 'live.exe'
                                if invoke([compiler, '--emit-project-oop-exe', consumer, '-' + level,
                                           '-o', executable, '--no-artifact-cache']).returncode:
                                    failures.append(label + ('build',))
                                elif invoke([executable]).returncode != 42:
                                    failures.append(label + ('run',))
    if failures:
        print('FAIL', failures, file=sys.stderr, flush=True)
    return int(bool(failures))


if __name__ == '__main__':
    sys.exit(main())
