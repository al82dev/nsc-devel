import os
from pathlib import Path
import subprocess
import sys
import tempfile


def run(command, cwd):
    return subprocess.run([str(item) for item in command], cwd=cwd, capture_output=True,
                          text=True, errors='replace', timeout=120)


def main():
    compiler = Path(sys.argv[1])
    with tempfile.TemporaryDirectory(prefix='nsc-generic-internal-owner-') as temporary:
        root = Path(temporary)
        producer = root / 'Producer.nsproj'
        producer.write_text('<Project><PropertyGroup><Name>Producer</Name><Language>3</Language>'
                            '<OutputType>Library</OutputType></PropertyGroup><ItemGroup>'
                            '<Compile Include="Producer.ns" /></ItemGroup></Project>', encoding='utf-8')
        counter_body = ('long[] block = new long[1]; int result = 0; '
                        'unsafe("counter test") { Counter* counter = (Counter*)&block[0]; '
                        '(*counter).Set(); result = (int)(*counter).Value; } '
                        'delete[] block; return result;') if os.environ.get('NSC_METADATA_POINTER_CASE') == '1' else 'return 42;'
        if os.environ.get('NSC_METADATA_STATIC_CASE') == '1':
            counter_body = 'return Helper.Value();'
        if os.environ.get('NSC_METADATA_IMPORTED_STATIC_CASE') == '1':
            counter_body = 'return ImportedHelper.Value();'
        if os.environ.get('NSC_METADATA_ALIAS_STATIC_CASE') == '1':
            counter_body = 'return Pick.ImportedHelper.Value();'
        if os.environ.get('NSC_METADATA_BODY_IMPORT') == '1':
            counter_body = 'using Sample.Support; return ImportedHelper.Value();'
        if os.environ.get('NSC_METADATA_SHADOW_CASE') == '1':
            counter_body = 'Counter Helper = default(Counter); return Helper.Read();'
        field_shadow = os.environ.get('NSC_METADATA_FIELD_SHADOW_CASE') == '1'
        if field_shadow:
            counter_body = 'return Helper.Read();'
        alias = os.environ.get('NSC_METADATA_ALIAS_STATIC_CASE') == '1'
        imported = os.environ.get('NSC_METADATA_IMPORTED_STATIC_CASE') == '1' or alias
        scoped = os.environ.get('NSC_METADATA_NAMESPACE_IMPORT') == '1'
        prefix = ('using Pick = Sample.Support;\n' if alias else 'using Sample.Support;\n') if (
            imported and os.environ.get('NSC_METADATA_BODY_IMPORT') != '1' and not scoped) else ''
        suffix = ('\nnamespace Sample.Support { internal static class ImportedHelper { public static int Value() { return 42; } } }\n'
                  'namespace Other { internal static class ImportedHelper { public static int Value() { return 7; } } }\n') if imported else ''
        (root / 'Producer.ns').write_text(prefix + '''namespace Sample {
 IMPORT_HERE
 internal struct Counter { public long Value; public void Set() { Value = 42l; }
  public int Read() { return 42; } }
 internal static class Helper { public static int Value() { return 42; } }
 internal static class Hidden { FIELD_SHADOW public static int Value<T>() where T : new() { COUNTER_BODY } }
 internal static class Outer { internal static class Inner {
  public static int Value<T>() where T : new() { return 42; } } }
 public static class Api { public static int Make<T>() where T : new() { return Hidden.Value<T>(); }
  public static int MakeNested<T>() where T : new() { return Outer.Inner.Value<T>(); } }
}'''.replace('COUNTER_BODY', counter_body).replace(
    'FIELD_SHADOW', 'private static Counter Helper;' if field_shadow else '').replace(
    'IMPORT_HERE', ('using Pick = Sample.Support;' if alias else 'using Sample.Support;') if scoped else '') + suffix,
    encoding='utf-8')
        if imported or field_shadow:
            source_control = root / 'SourceControl.nsproj'
            source_control.write_text('<Project><PropertyGroup><Name>Producer</Name><Language>3</Language>'
                                      '</PropertyGroup><ItemGroup><Compile Include="Producer.ns" />'
                                      '<Compile Include="SourceControl.ns" /></ItemGroup></Project>', encoding='utf-8')
            (root / 'SourceControl.ns').write_text('''using Sample;
class Payload { public Payload() { } }
class Program { public static int Main() { return Api.Make<Payload>(); } }
''', encoding='utf-8')
            for level in ('O0', 'O2'):
                exe = root / ('source-control-' + level + '.exe')
                result = run([compiler, '--emit-project-oop-exe', source_control, '-' + level, '-o', exe], root)
                assert result.returncode == 0, ('source control', level, result.stdout + result.stderr)
                result = run([exe], root)
                assert result.returncode == 42, ('source control', level, result.returncode)
                print('source control', level, 'exit=42', flush=True)
        library = root / 'Producer.lib'
        result = run([compiler, '--emit-project-oop-lib', producer, '-O0', '-o', library], root)
        assert result.returncode == 0, ('producer', result.stdout + result.stderr)
        print('producer library and metadata generated', flush=True)

        consumer = root / 'Consumer.nsproj'
        consumer.write_text('<Project><PropertyGroup><Name>Consumer</Name><Language>3</Language>'
                            '</PropertyGroup><ItemGroup><Compile Include="Consumer.ns" />'
                            '<LibraryReference Include="Producer.lib" Metadata="Producer.nsmeta" />'
                            '</ItemGroup></Project>', encoding='utf-8')
        source = root / 'Consumer.ns'
        source.write_text('''using Sample;
class Payload { public Payload() { } }
class Program { public static int Main() {
 return Api.Make<Payload>() == 42 && Api.MakeNested<Payload>() == 42 ? 42 : 1; } }
''', encoding='utf-8')
        for level in ('O0', 'O2'):
            exe = root / ('Consumer-' + level + '.exe')
            result = run([compiler, '--emit-project-oop-exe', consumer, '-o', exe, '-' + level], root)
            assert result.returncode == 0, (level, result.stdout + result.stderr)
            if os.name == 'nt':
                result = run([exe], root)
                assert result.returncode == 42, (level, result.returncode, result.stdout, result.stderr)
                print('consumer', level, 'exit=42', flush=True)
        source.write_text('''using Sample;
class Payload { public Payload() { } }
class Program { public static int Main() { return Hidden.Value<Payload>(); } }
''', encoding='utf-8')
        result = run([compiler, '--check-project-method-bodies', consumer], root)
        output = result.stdout + result.stderr
        assert result.returncode != 0 and 'error:' in output and 'Hidden' in output, (
            'internal helper became accessible or failed for an unrelated reason', output)
        print('direct internal helper rejected', flush=True)
        source.write_text('''using Sample;
class Payload { public Payload() { } }
class Program { public static int Main() { return Outer.Inner.Value<Payload>(); } }
''', encoding='utf-8')
        result = run([compiler, '--check-project-method-bodies', consumer], root)
        output = result.stdout + result.stderr
        assert result.returncode != 0 and 'error:' in output and 'Outer' in output, (
            'nested internal helper became accessible or failed for an unrelated reason', output)
        print('nested internal helper rejected', flush=True)
        if os.environ.get('NSC_METADATA_SHADOW_CASE') == '1':
            source.write_text('''using Sample;
class Program { public static int Main() { return Helper.Value(); } }
''', encoding='utf-8')
            result = run([compiler, '--check-project-method-bodies', consumer], root)
            output = result.stdout + result.stderr
            assert result.returncode != 0 and 'error:' in output and 'Helper' in output, (
                'shadowed internal type became accessible or failed for an unrelated reason', output)
            print('shadowed internal type rejected', flush=True)
        if os.environ.get('NSC_METADATA_POINTER_CASE') == '1':
            source.write_text('''using Sample;
class Program { public static int Main() { Counter value; return 0; } }
''', encoding='utf-8')
            result = run([compiler, '--check-project-method-bodies', consumer], root)
            output = result.stdout + result.stderr
            assert result.returncode != 0 and 'error:' in output and 'Counter' in output, (
                'internal payload type became accessible or failed for an unrelated reason', output)
            print('internal payload type rejected', flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
