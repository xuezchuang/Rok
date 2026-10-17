import argparse
import json
import pathlib
import sys
from rok_data_paths import resolve


PROJECT_ROOT = pathlib.Path(__file__).resolve().parents[1]
PRODUCTION_MAP_FILES = {
    "L_RokUnityGameAppCity.umap",
    "L_RokUnityWorldMapPrototype.umap",
}
WORLD_MAP_REFERENCE = (
    "/Game/Maps/L_RokUnityWorldMapPrototype."
    "L_RokUnityWorldMapPrototype"
)
CITY_MAP_REFERENCE = (
    "/Game/Maps/L_RokUnityGameAppCity."
    "L_RokUnityGameAppCity"
)
OUTPUT_PATH = (
    PROJECT_ROOT
    / "Saved"
    / "RokUnityMigration"
    / "rok_map_scene_flow_verify.json"
)


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--runtime-log",
        type=pathlib.Path,
        help="Optional standalone runtime log for bidirectional travel.",
    )
    return parser.parse_args()


def main():
    args = parse_args()
    failures = []
    production_dir = PROJECT_ROOT / "Content" / "Maps"
    diagnostic_dir = PROJECT_ROOT / "Content" / "RokDiagnostics" / "Maps"
    config_path = PROJECT_ROOT / "Config" / "DefaultEngine.ini"

    observed_production = {
        path.name for path in production_dir.glob("*.umap")
    }
    if observed_production != PRODUCTION_MAP_FILES:
        failures.append(
            "Content/Maps must contain only the two production maps: "
            "expected={0}, observed={1}".format(
                sorted(PRODUCTION_MAP_FILES),
                sorted(observed_production),
            )
        )

    observed_diagnostics = sorted(
        path.name for path in diagnostic_dir.glob("*.umap")
    )
    if not observed_diagnostics:
        failures.append("No diagnostic maps were archived")

    config_text = config_path.read_text(encoding="utf-8-sig")
    for key in ("EditorStartupMap", "GameDefaultMap"):
        expected_line = "{0}={1}".format(key, WORLD_MAP_REFERENCE)
        if expected_line not in config_text:
            failures.append(
                "DefaultEngine.ini is missing {0}".format(expected_line)
            )

    strategy_header = (
        PROJECT_ROOT / "Source" / "Rok" / "RokStrategyCameraPawn.h"
    ).read_text(encoding="utf-8-sig")
    strategy_source = (
        PROJECT_ROOT / "Source" / "Rok" / "RokStrategyCameraPawn.cpp"
    ).read_text(encoding="utf-8-sig")
    world_source = (
        PROJECT_ROOT / "Source" / "Rok" / "RokWorldMapCameraPawn.cpp"
    ).read_text(encoding="utf-8-sig")

    required_source_tokens = {
        "city zoom-out policy": "ShouldReturnToWorldMap",
        "shared city zoom input": "ApplyZoomInput",
        "city-to-world travel": WORLD_MAP_REFERENCE,
        "world entry option": "RokWorldEntryDxf=",
        "world-to-city travel": CITY_MAP_REFERENCE,
        "completed flow guard": "RokMapFlowCompleted=",
    }
    combined_source = "\n".join(
        (strategy_header, strategy_source, world_source)
    )
    for role, token in required_source_tokens.items():
        if token not in combined_source:
            failures.append(
                "Missing {0} source token: {1}".format(role, token)
            )

    runtime_log = None
    runtime_verified = False
    runtime_counts = {
        "world_map_loads": 0,
        "city_map_loads": 0,
    }
    if args.runtime_log:
        runtime_log = (
            args.runtime_log
            if args.runtime_log.is_absolute()
            else resolve(args.runtime_log)
        )
        if not runtime_log.exists():
            failures.append(
                "Runtime log does not exist: {0}".format(runtime_log)
            )
        else:
            runtime_text = runtime_log.read_text(
                encoding="utf-8",
                errors="replace",
            )
            runtime_markers = {
                "world zoom entered city": (
                    "Rok world-map automation player-city entry: "
                    "result=started"
                ),
                "city level loaded": (
                    "Load map complete /Game/Maps/"
                    "L_RokUnityGameAppCity"
                ),
                "city zoom returned to world": (
                    "Rok main-city automation world-map return: "
                    "result=started"
                ),
                "world entry dxf applied": (
                    "Rok world-map entry initialized:"
                ),
                "round trip completed": (
                    "Rok map-flow runtime automation completed: "
                    "final_level=/Game/Maps/"
                    "L_RokUnityWorldMapPrototype."
                ),
                "clean exit": (
                    "Rok map-flow runtime automation requesting clean exit."
                ),
            }
            for role, marker in runtime_markers.items():
                if marker not in runtime_text:
                    failures.append(
                        "Runtime log is missing {0}: {1}".format(
                            role,
                            marker,
                        )
                    )
            runtime_counts = {
                "world_map_loads": runtime_text.count(
                    "Load map complete "
                    "/Game/Maps/L_RokUnityWorldMapPrototype"
                ),
                "city_map_loads": runtime_text.count(
                    "Load map complete "
                    "/Game/Maps/L_RokUnityGameAppCity"
                ),
            }
            if runtime_counts["world_map_loads"] < 2:
                failures.append(
                    "Runtime did not load the world map twice"
                )
            if runtime_counts["city_map_loads"] < 1:
                failures.append("Runtime did not load the city map")
            if "automation world-map return: result=failed" in runtime_text:
                failures.append(
                    "Runtime city-to-world zoom automation failed"
                )
            if "automation player-city entry: result=failed" in runtime_text:
                failures.append(
                    "Runtime world-to-city zoom automation failed"
                )
            runtime_verified = not any(
                failure.startswith("Runtime")
                for failure in failures
            )

    result = {
        "schema": "rok.map-scene-flow.verify.v2",
        "production_maps": sorted(observed_production),
        "diagnostic_map_count": len(observed_diagnostics),
        "diagnostic_maps": observed_diagnostics,
        "main_map": WORLD_MAP_REFERENCE,
        "city_map": CITY_MAP_REFERENCE,
        "bidirectional_zoom_flow": not any(
            "source token" in failure for failure in failures
        ),
        "runtime_log": (
            str(runtime_log.relative_to(PROJECT_ROOT)).replace("\\", "/")
            if runtime_log
            and runtime_log.is_relative_to(PROJECT_ROOT)
            else str(runtime_log) if runtime_log else None
        ),
        "runtime_verified": runtime_verified,
        "runtime_counts": runtime_counts,
        "failure_count": len(failures),
        "failures": failures,
    }
    OUTPUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT_PATH.write_text(
        json.dumps(result, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
