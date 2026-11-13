"""Exercise the launcher with disposable prefixes and Wine/process stubs."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import time
import unittest


ROOT = Path(__file__).resolve().parents[2]
URI = 'misterhorsepm://callback?test=a%2Fb&value=two%20words'

SPY = '''#!/usr/bin/python3
import json, os, pathlib, sys
root = pathlib.Path(os.environ['TEST_STATE'])
name = pathlib.Path(sys.argv[0]).name
args = sys.argv[1:]
with (root / 'calls.jsonl').open('a') as f:
    f.write(json.dumps([name, args, os.environ['WINEPREFIX'], os.environ['ADOBEWINE_HOME']]) + '\\n')
if name == 'wine' and args == ['tasklist']:
    counter = root / 'tasks'
    count = int(counter.read_text()) + 1 if counter.exists() else 1
    counter.write_text(str(count))
    if os.environ.get('TEST_RUNNING') == '1' and count <= int(os.environ.get('TEST_EXIT_AFTER', '100')):
        print(os.environ.get('TEST_EXE', 'Photoshop.exe') + ' 1234 Console')
elif name == 'python3':
    counter = root / 'windows'
    count = int(counter.read_text()) if counter.exists() else 0
    counter.write_text(str(count + 1))
    values = os.environ.get('TEST_WINDOWS', '1').split(',')
    value = values[min(count, len(values) - 1)]
    if value == 'error':
        sys.exit(1)
    print(value)
elif name == 'flatpak':
    os.execv(os.environ['TEST_LAUNCHER'], [os.environ['TEST_LAUNCHER']] + args[3:])
'''

GIO = '''
import sys
from gi.repository import Gio
if sys.argv[1] == 'default':
    app = Gio.AppInfo.get_default_for_uri_scheme('misterhorsepm')
    print(app.get_id() if app else '')
elif sys.argv[1] == 'files':
    import json
    results = []
    for path in sys.argv[2:]:
        file = Gio.File.new_for_path(path)
        mime = file.query_info('standard::content-type', Gio.FileQueryInfoFlags.NONE).get_content_type()
        app = Gio.AppInfo.get_default_for_type(mime, False)
        results.append([mime, app.get_id() if app else None])
    print(json.dumps(results))
elif sys.argv[1] == 'launch-file':
    file = Gio.File.new_for_path(sys.argv[2])
    mime = file.query_info('standard::content-type', Gio.FileQueryInfoFlags.NONE).get_content_type()
    app = Gio.AppInfo.get_default_for_type(mime, False)
    assert app is not None and app.launch([file], None)
else:
    app = Gio.DesktopAppInfo.new_from_filename(sys.argv[1])
    assert app is not None and app.supports_uris()
    assert app.launch_uris([sys.argv[2]], None)
'''


class LauncherTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix='adobewine-launcher-')
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.source = self.root / 'source'
        for relative in ('bin/env.sh', 'bin/app-catalog.sh', 'bin/adobewine',
                         'config/apps.txt', 'config/adobewine.xml', 'scripts/install-launchers.sh'):
            target = self.source / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(ROOT / relative, target)
        self.home = self.root / 'wine home'
        self.prefix = self.home / 'prefix'
        self.data = self.root / 'data'
        self.apps = self.data / 'applications'
        self.state = self.root / 'state'
        self.state.mkdir()
        self.runtime = self.home / 'runtime/bin'
        self.runtime.mkdir(parents=True)
        for name in ('wine', 'python3', 'sleep', 'nvidia-smi', 'flatpak'):
            target = self.runtime / name
            target.write_text(SPY)
            target.chmod(0o755)
        self.env = dict(os.environ, HOME=str(self.root), ADOBEWINE_HOME=str(self.home),
                        WINEPREFIX=str(self.prefix), TEST_STATE=str(self.state),
                        TEST_LAUNCHER=str(self.source / 'bin/adobewine'),
                        XDG_DATA_HOME=str(self.data), XDG_DATA_DIRS=str(self.root / 'empty'),
                        XDG_CONFIG_HOME=str(self.root / 'config'),
                        XDG_CONFIG_DIRS=str(self.root / 'empty'),
                        XDG_CACHE_HOME=str(self.root / 'cache'),
                        PATH=f'{self.runtime}:/usr/bin:/bin')
        for key in ('ADOBEWINE_EXEC', 'REPORTS', 'TEST_RUNNING', 'TEST_WINDOWS',
                    'TEST_EXIT_AFTER', 'TEST_EXE', 'GIO_EXTRA_MODULES'):
            self.env.pop(key, None)
        config = Path(self.env['XDG_CONFIG_HOME']) / 'adobewine/config'
        config.parent.mkdir(parents=True)
        config.write_text('REPORTS=off\n')
        fonts = self.prefix / 'drive_c/Program Files/Common Files/Adobe/Fonts'
        fonts.mkdir(parents=True)
        (fonts / 'MyriadPro-Regular.otf').touch()
        (self.prefix / 'drive_c/users/Public').mkdir(parents=True)
        self.file = self.root / "Second file & client's project.psd"
        self.file.touch()

    def run_script(self, relative, *args):
        return subprocess.run(['bash', str(self.source / relative), *map(str, args)],
                              env=self.env, text=True, capture_output=True, check=True, timeout=10)

    def calls(self, name):
        log = self.state / 'calls.jsonl'
        return [call for call in map(json.loads, log.read_text().splitlines())
                if call[0] == name] if log.exists() else []

    def assert_not_killed(self):
        self.assertFalse(any(call[1][0] == 'taskkill' for call in self.calls('wine')))

    def test_visible_apps_receive_files_without_exit_delay(self):
        self.env.update(TEST_RUNNING='1', TEST_WINDOWS='1')
        catalog = (ROOT / 'config/apps.txt').read_text().splitlines()
        for line in catalog:
            command, _, _, executable, _, _ = line.split('|')
            with self.subTest(app=command):
                self.env['TEST_EXE'] = executable.rsplit('/', 1)[-1]
                self.run_script('bin/adobewine', command, self.file)
                self.assertEqual(self.calls('wine')[-1][1][-1],
                                 'Z:' + str(self.file).replace('/', '\\'))
        self.assertEqual(self.calls('sleep'), [])
        self.assert_not_killed()

    def test_new_app_does_not_wait(self):
        self.run_script('bin/adobewine', 'photoshop', self.file)
        self.assertEqual(self.calls('sleep'), [])
        self.assert_not_killed()

    def test_multiple_files_keep_argument_boundaries(self):
        other = self.root / 'Another file.psd'
        other.touch()
        self.env.update(TEST_RUNNING='1', TEST_WINDOWS='1')
        self.run_script('bin/adobewine', 'photoshop', self.file, other)
        self.assertEqual(self.calls('wine')[-1][1][1:],
                         ['Z:' + str(path).replace('/', '\\') for path in (self.file, other)])
        self.assertEqual(self.calls('sleep'), [])
        self.assert_not_killed()

    def test_windowless_process_gets_exit_grace_before_targeted_kill(self):
        self.env.update(TEST_RUNNING='1', TEST_WINDOWS='0')
        self.run_script('bin/adobewine', 'photoshop', self.file)
        self.assertEqual(len(self.calls('sleep')), 10)
        kills = [call[1] for call in self.calls('wine') if call[1][0] == 'taskkill']
        self.assertEqual(kills, [['taskkill', '/f', '/im', 'Photoshop.exe']])

    def test_exiting_process_is_not_killed(self):
        self.env.update(TEST_RUNNING='1', TEST_WINDOWS='0', TEST_EXIT_AFTER='2')
        self.run_script('bin/adobewine', 'photoshop', self.file)
        self.assertEqual(len(self.calls('sleep')), 1)
        self.assert_not_killed()

    def test_window_appearing_during_grace_is_not_killed(self):
        self.env.update(TEST_RUNNING='1', TEST_WINDOWS='0,1')
        self.run_script('bin/adobewine', 'photoshop', self.file)
        self.assert_not_killed()

    def test_missing_window_inspection_does_not_wait_or_kill(self):
        self.env.update(TEST_RUNNING='1', TEST_WINDOWS='error')
        result = self.run_script('bin/adobewine', 'photoshop', self.file)
        self.assertIn('is still running', result.stdout)
        self.assertEqual(self.calls('sleep'), [])
        self.assert_not_killed()

    def test_failed_window_recheck_does_not_kill(self):
        self.env.update(TEST_RUNNING='1', TEST_WINDOWS='0,error')
        self.run_script('bin/adobewine', 'photoshop', self.file)
        self.assert_not_killed()

    def gio(self, *args):
        available = subprocess.run(['/usr/bin/python3', '-c', 'from gi.repository import Gio'],
                                   capture_output=True)
        if available.returncode:
            self.skipTest('PyGObject is needed for desktop URI dispatch checks')
        return subprocess.run(['/usr/bin/python3', '-c', GIO, *map(str, args)],
                              env=self.env, capture_output=True, text=True, check=True, timeout=10)

    def install_handler(self):
        if not all(shutil.which(name) for name in ('update-desktop-database', 'update-mime-database')):
            self.skipTest('desktop-file-utils and shared-mime-info are needed for MIME checks')
        self.run_script('scripts/install-launchers.sh')
        handler = self.apps / 'adobewine-misterhorse.desktop'
        subprocess.run(['desktop-file-validate', str(handler)], check=True, capture_output=True)
        return handler

    def launch_handler(self, handler):
        self.gio(handler, URI)
        deadline = time.monotonic() + 5
        while not self.calls('wine') and time.monotonic() < deadline:
            time.sleep(0.02)
        calls = self.calls('wine')
        self.assertEqual(len(calls), 1)
        self.assertEqual(calls[0][1], [str(self.prefix / 'drive_c/Program Files/'
                                        'Mister Horse Product Manager/ProductManager.exe'), URI])
        self.assertEqual(calls[0][2:], [str(self.prefix), str(self.home)])
        self.assertEqual(self.calls('sleep'), [])
        self.assert_not_killed()

    def test_protocol_is_available_before_install_and_registration_is_idempotent(self):
        handler = self.install_handler()
        self.assertEqual(self.gio('default').stdout.strip(), handler.name)
        contents = handler.read_text()
        self.run_script('scripts/install-launchers.sh')
        self.assertEqual(handler.read_text(), contents)
        self.launch_handler(handler)
        self.assertFalse((Path(self.env['XDG_CONFIG_HOME']) / 'mimeapps.list').exists())

    def test_callback_preserves_custom_prefix_and_quoted_arguments(self):
        self.prefix = self.root / 'préfixe % " $ ` \\ & space'
        self.env['WINEPREFIX'] = str(self.prefix)
        self.launch_handler(self.install_handler())

    def test_flatpak_command_receives_callback(self):
        self.env['ADOBEWINE_EXEC'] = 'flatpak run --command=adobewine io.github.le_birnes.AdobeWine'
        self.launch_handler(self.install_handler())
        self.assertEqual(self.calls('flatpak')[0][1][:3],
                         ['run', '--command=adobewine', 'io.github.le_birnes.AdobeWine'])

    def test_removal_unregisters_protocol_and_preserves_other_apps(self):
        self.install_handler()
        other = self.apps / 'unrelated.desktop'
        other.write_text('[Desktop Entry]\nType=Application\nName=Other\nExec=true\n')
        self.run_script('scripts/install-launchers.sh', '--remove')
        self.assertTrue(other.exists())
        self.assertFalse(list(self.apps.glob('adobewine-*.desktop')))
        self.assertEqual(self.gio('default').stdout.strip(), '')

    def test_existing_protocol_preference_is_preserved(self):
        self.apps.mkdir(parents=True)
        (self.apps / 'other-manager.desktop').write_text(
            '[Desktop Entry]\nType=Application\nName=Other Manager\nExec=true %u\n'
            'MimeType=x-scheme-handler/misterhorsepm;\n')
        preferences = Path(self.env['XDG_CONFIG_HOME']) / 'mimeapps.list'
        contents = ('[Default Applications]\n'
                    'x-scheme-handler/misterhorsepm=other-manager.desktop;\n'
                    'text/plain=editor.desktop;\n')
        preferences.write_text(contents)
        self.install_handler()
        self.assertEqual(preferences.read_text(), contents)
        self.assertEqual(self.gio('default').stdout.strip(), 'other-manager.desktop')
        self.run_script('scripts/install-launchers.sh', '--remove')
        self.assertEqual(preferences.read_text(), contents)

    def install_native_apps(self, commands):
        for line in (self.source / 'config/apps.txt').read_text().splitlines():
            command, _, folder, executable, _, _ = line.split('|')
            if command in commands:
                path = self.prefix / 'drive_c/Program Files/Adobe' / folder.replace('{year}', '2026') / executable
                path.parent.mkdir(parents=True, exist_ok=True)
                path.touch()
        self.install_handler()
        for handler in self.apps.glob('adobewine-*.desktop'):
            subprocess.run(['desktop-file-validate', str(handler)], check=True, capture_output=True)

    def native_files(self):
        extensions = {
            'photoshop': ('psd', 'psb', 'psdt'),
            'illustrator': ('ai', 'ait'),
            'aftereffects': ('aep', 'aepx', 'aet'),
            'premiere': ('prproj',),
            'audition': ('sesx',),
            'indesign': ('indd', 'indt', 'indb', 'indl', 'idml', 'idap'),
            'incopy': ('icml', 'icmt', 'icma', 'icap'),
            'animate': ('fla', 'xfl'),
            'lightroomclassic': ('lrcat',),
        }
        files = {}
        for command, suffixes in extensions.items():
            for suffix in suffixes:
                path = self.root / f"Client's project & review.{suffix}"
                path.write_bytes(b'<?xml version="1.0"?><project/>')
                files[path] = command
        return files

    def test_native_formats_are_recognized_and_associated_with_installed_apps(self):
        files = self.native_files()
        self.install_native_apps(set(files.values()))
        for path in list(files):
            uppercase = path.with_suffix(path.suffix.upper())
            uppercase.write_bytes(path.read_bytes())
            files[uppercase] = files[path]
        results = json.loads(self.gio('files', *files).stdout)
        for path, (mime, handler) in zip(files, results):
            with self.subTest(file=path.name):
                self.assertNotIn(mime, ('text/plain', 'application/xml', 'application/octet-stream'))
                self.assertEqual(handler, f'adobewine-{files[path]}.desktop')
        self.assertFalse((Path(self.env['XDG_CONFIG_HOME']) / 'mimeapps.list').exists())

    def test_file_manager_dispatch_preserves_project_path(self):
        files = self.native_files()
        self.install_native_apps(set(files.values()))
        for path, command in files.items():
            with self.subTest(file=path.name):
                before = len(self.calls('wine'))
                self.gio('launch-file', path)
                deadline = time.monotonic() + 5
                while len(self.calls('wine')) < before + 2 and time.monotonic() < deadline:
                    time.sleep(0.02)
                call = self.calls('wine')[-1]
                entry = next(line.split('|') for line in (self.source / 'config/apps.txt').read_text().splitlines()
                             if line.startswith(command + '|'))
                executable = 'C:\\Program Files\\Adobe\\' + '/'.join(
                    (entry[2].replace('{year}', '2026'), entry[3])).replace('/', '\\')
                self.assertEqual(call[1][0], executable)
                self.assertEqual(call[1][-1], 'Z:' + str(path).replace('/', '\\'))
        self.assert_not_killed()

    def test_flatpak_command_receives_native_project(self):
        self.env['ADOBEWINE_EXEC'] = 'flatpak run --command=adobewine io.github.le_birnes.AdobeWine'
        self.install_native_apps({'aftereffects'})
        project = self.root / "Client's project & review.aep"
        project.write_text('test')
        self.gio('launch-file', project)
        deadline = time.monotonic() + 5
        while len(self.calls('wine')) < 2 and time.monotonic() < deadline:
            time.sleep(0.02)
        self.assertEqual(self.calls('flatpak')[0][1],
                         ['run', '--command=adobewine', 'io.github.le_birnes.AdobeWine',
                          'aftereffects', str(project)])
        self.assertEqual(self.calls('wine')[-1][1][-1], 'Z:' + str(project).replace('/', '\\'))

    def test_native_registration_preserves_preferences_and_removes_only_owned_types(self):
        self.install_native_apps({'aftereffects', 'indesign'})
        preferences = Path(self.env['XDG_CONFIG_HOME']) / 'mimeapps.list'
        other = self.apps / 'other-editor.desktop'
        other.write_text('[Desktop Entry]\nType=Application\nName=Other Editor\nExec=true %F\n'
                         'MimeType=application/x-aftereffects;application/pdf;text/plain;\n')
        contents = ('[Default Applications]\napplication/x-aftereffects=other-editor.desktop;\n'
                    'application/pdf=other-editor.desktop;\ntext/plain=other-editor.desktop;\n')
        preferences.write_text(contents)
        package = self.data / 'mime/packages/unrelated.xml'
        package.write_text('<mime-info xmlns="http://www.freedesktop.org/standards/shared-mime-info">'
                           '<mime-type type="application/x-unrelated">'
                           '<glob pattern="*.unrelated"/></mime-type></mime-info>')
        self.run_script('scripts/install-launchers.sh')
        source = self.data / 'mime/packages/adobewine.xml'
        registered = source.read_bytes()
        self.run_script('scripts/install-launchers.sh')
        self.assertEqual(source.read_bytes(), registered)
        self.assertEqual(preferences.read_text(), contents)
        project = self.root / 'Project.aep'
        project.write_text('test')
        self.assertEqual(json.loads(self.gio('files', project).stdout)[0][1], other.name)
        self.run_script('scripts/install-launchers.sh', '--remove')
        self.assertFalse(source.exists())
        self.assertTrue(package.exists())
        self.assertEqual(preferences.read_text(), contents)
        self.assertNotIn('*.aep', (self.data / 'mime/globs2').read_text())
        self.assertIn('*.unrelated', (self.data / 'mime/globs2').read_text())

    def test_uninstalled_apps_are_not_advertised_as_handlers(self):
        self.install_native_apps({'aftereffects'})
        files = self.native_files()
        results = json.loads(self.gio('files', *files).stdout)
        for path, (_, handler) in zip(files, results):
            expected = 'adobewine-aftereffects.desktop' if files[path] == 'aftereffects' else None
            self.assertEqual(handler, expected)


if __name__ == '__main__':
    unittest.main()
