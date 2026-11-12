"""Common path and child-process environment for isolated Windows probes."""
import os
from pathlib import Path

def absolute_a(value):
    path = Path(value)
    if os.name != 'nt' or not path.is_absolute() or path.drive.upper() != 'A:':
        raise ValueError('each path must be an absolute path on A:')
    path = path.resolve()
    if path.drive.upper() != 'A:':
        raise ValueError('resolved paths must remain on A:')
    return path

def setup(root, repository):
    if root == repository or root.is_relative_to(repository):
        raise ValueError('evidence directory must be outside the repository')
    root.mkdir(parents=True, exist_ok=False)
    temporary = root / 'temp'
    temporary.mkdir()
    environment = os.environ.copy()
    environment['TEMP'] = str(temporary)
    environment['TMP'] = str(temporary)
    return environment
