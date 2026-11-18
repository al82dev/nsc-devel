from pathlib import Path
import subprocess
import sys
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from nsc_builder import stdlib_build
from nsc_builder.build import compiler_library_build_commands
from nsc_builder.configuration import BuilderConfig


class StandardLibraryBuildTests(unittest.TestCase):
    def test_plans_libraries_for_each_compiler_configuration(self):
        config = BuilderConfig(compiler_build_root='A:/test-compiler', jobs=3)
        commands = compiler_library_build_commands(config, ('Debug', 'Release'))
        self.assertEqual(len(commands), 2)
        for label, command, _ in commands:
            self.assertIn('bibliotecas estandar', label)
            self.assertEqual(command[0], 'python')
            compiler = Path(command[command.index('--compiler') + 1])
            self.assertEqual(compiler.name, 'nsc.exe')
            self.assertEqual(command[command.index('--jobs') + 1], '3')
            self.assertIn('--runtime', command)
            self.assertIn('--imports', command)
            self.assertIn('--linker', command)
            self.assertIn('--package-prefix', command)

    def test_invokes_all_four_optimizations_with_selected_compiler(self):
        args = ['stdlib_build', '--libraries', 'A:/libraries', '--compiler', 'A:/build/bin/nsc.exe',
                '--runtime', 'A:/build/generated/runtime.lib', '--imports', 'A:/build/imports',
                '--linker', 'A:/build/bin/nslink.exe', '--package-prefix', 'A:/build', '--jobs', '3']
        with patch.object(sys, 'argv', args), patch.object(stdlib_build.subprocess, 'run') as run:
            run.return_value = subprocess.CompletedProcess([], 0)
            self.assertEqual(stdlib_build.main(), 0)
        self.assertEqual(run.call_count, 5)
        command = run.call_args_list[0].args[0]
        self.assertEqual(command[0], 'python')
        self.assertIn('--all-levels', command)
        self.assertEqual(command.count('--package'), 5)
        self.assertEqual(Path(command[command.index('--compiler') + 1]).name, 'nsc.exe')
        # EN: Each compiler build keeps its own library artifacts. / ES: Cada construccion del compilador
        #     conserva sus propios artefactos de bibliotecas.
        self.assertEqual(Path(command[command.index('--build-root') + 1]).name, 'nativesharp-libraries-build')
        self.assertTrue(all('--build-root' in call.args[0] for call in run.call_args_list))
        self.assertEqual([call.args[0][call.args[0].index('--opt') + 1] for call in run.call_args_list[1:]],
                         ['O0', 'O1', 'O2', 'O3'])
        self.assertEqual(Path(run.call_args.kwargs['env']['NSC_LINKER']).name, 'nslink.exe')

    def test_does_not_stage_packages_after_a_failed_build(self):
        args = ['stdlib_build', '--libraries', 'A:/libraries', '--compiler', 'A:/build/bin/nsc.exe',
                '--runtime', 'A:/build/generated/runtime.lib', '--imports', 'A:/build/imports',
                '--linker', 'A:/build/bin/nslink.exe', '--package-prefix', 'A:/build', '--jobs', '3']
        with patch.object(sys, 'argv', args), patch.object(stdlib_build.subprocess, 'run') as run:
            run.return_value = subprocess.CompletedProcess([], 9)
            self.assertEqual(stdlib_build.main(), 9)
        self.assertEqual(run.call_count, 1)


if __name__ == '__main__':
    unittest.main()
