import unreal


print(f"ROK_MCP_PROJECT_DIR={unreal.Paths.project_dir()}")
print(
    "ROK_MCP_EDITOR_WORLD="
    f"{unreal.EditorLevelLibrary.get_editor_world().get_name()}"
)
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
