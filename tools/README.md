# ROK Audit CLI

`rok_audit.py` generates a read-only Markdown report for the Unity-to-Unreal migration state. It checks expected UE folders, maps, assets, config files, naming conventions, and optional Unity-vs-UE asset coverage.

Example usage from the project root:

```powershell
python tools\rok_audit.py --ue D:\ue\Rok --unity D:\ue\Rok\ref\client-unity --out Saved\Reports\rok_audit.md
```

UE-only audit:

```powershell
python tools\rok_audit.py --ue D:\ue\Rok --out Saved\Reports\rok_audit.md
```
