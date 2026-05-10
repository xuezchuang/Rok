import os

import unreal


def main():
    script_path = os.path.join(
        unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()),
        "Scripts",
        "CreateRokPrototypeScene.py",
    )
    with open(script_path, "r", encoding="utf-8") as script_file:
        code = compile(script_file.read(), script_path, "exec")
    exec(code, {"__name__": "__main__", "__file__": script_path})


if __name__ == "__main__":
    main()
