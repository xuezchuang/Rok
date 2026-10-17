"""Import only the reproducible Unity commander UI atlas set into UE."""

import os
import sys


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
if SCRIPT_DIR not in sys.path:
    sys.path.insert(0, SCRIPT_DIR)

from ImportRokUiReferenceTextures import import_commander_atlases


def main():
    return import_commander_atlases()


if __name__ == "__main__":
    main()
