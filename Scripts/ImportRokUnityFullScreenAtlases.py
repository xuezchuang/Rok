import sys
from pathlib import Path


SCRIPT_DIRECTORY = Path(__file__).resolve().parent
if str(SCRIPT_DIRECTORY) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIRECTORY))

from ImportRokUiReferenceTextures import (
    import_full_screen_atlases,
    import_full_screen_dependencies,
)


if __name__ == "__main__":
    import_full_screen_dependencies()
    import_full_screen_atlases()
