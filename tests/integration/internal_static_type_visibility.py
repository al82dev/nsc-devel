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
    with tempfile.TemporaryDirectory(prefix='nsc-internal-static-type-') as temporary:
        root = Path(temporary)
        producer = root / 'producer'
        consumer = root / 'consumer'
        producer.mkdir()
        consumer.mkdir()
        producer_project = producer / 'Producer.nsproj'
        consumer_project = consumer / 'Consumer.nsproj'
        producer_source = '''namespace Sample {
 internal class Hidden { public static int Plain() { return 42; }
 public static int Generic<T>() where T : new() { return 42; } }
 public class Visible { internal static int InternalMethod() { return 42; } }
 public class Api { public static int Plain() { return Hidden.Plain(); }
 public static int Make<T>() where T : new() { return Hidden.Generic<T>(); } }
}'''
        (producer / 'Producer.ns').write_text(producer_source, encoding='utf-8')
        consumer_project.write_text('<Project><PropertyGroup><Name>Consumer</Name><Language>3</Language>'
            '</PropertyGroup><ItemGroup><Compile Include="Consumer.ns" />'
            '<ProjectReference Include="../producer/Producer.nsproj" />'
            '</ItemGroup></Project>', encoding='utf-8')

        def configure_producer(friend):
            item = '<Friend Include="Consumer" />' if friend else ''
            producer_project.write_text('<Project><PropertyGroup><Name>Producer</Name><Language>3</Language>'
                '</PropertyGroup><ItemGroup><Compile Include="Producer.ns" />' + item +
                '</ItemGroup></Project>', encoding='utf-8')

        def configure_consumer(statement, imported=True):
            prefix = 'using Sample; ' if imported else ''
            (consumer / 'Consumer.ns').write_text(prefix + 'class Payload { public Payload() { } }\n'
                'class Program { public static int Main() { ' + statement + ' } }\n', encoding='utf-8')

        configure_producer(False)
        for name, statement, imported, identifier in (
            ('plain', 'return Hidden.Plain();', True, 'Hidden'),
            ('generic', 'return Hidden.Generic<Payload>();', True, 'Hidden'),
            ('no_import', 'return Hidden.Plain();', False, 'Hidden'),
            ('internal_method', 'return Visible.InternalMethod();', True, 'InternalMethod'),
            ('local_type', 'Hidden item = default(Hidden); return 0;', True, 'Hidden'),
        ):
            configure_consumer(statement, imported)
            result = run([compiler, '--check-project-method-bodies', consumer_project], consumer)
            output = result.stdout + result.stderr
            assert result.returncode != 0 and 'error:' in output and identifier in output, (name, output)
            print(name, 'rejected', flush=True)

        configure_consumer('return Api.Plain() + Api.Make<Payload>() - 42;')
        result = run([compiler, '--check-project-method-bodies', consumer_project], consumer)
        assert result.returncode == 0, ('producer API', result.stdout + result.stderr)
        if os.name == 'nt':
            for level in ('O0', 'O2'):
                exe = consumer / (level + '.exe')
                result = run([compiler, '--emit-project-oop-exe', consumer_project, '-o', exe,
                              '-' + level], consumer)
                assert result.returncode == 0, (level, result.stdout + result.stderr)
                result = run([exe], consumer)
                assert result.returncode == 42, (level, result.returncode, result.stdout, result.stderr)
                print('producer API', level, 'exit=42', flush=True)

        configure_producer(True)
        configure_consumer('return Hidden.Plain();')
        result = run([compiler, '--check-project-method-bodies', consumer_project], consumer)
        assert result.returncode == 0, ('friend access', result.stdout + result.stderr)
        print('friend access accepted', flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
