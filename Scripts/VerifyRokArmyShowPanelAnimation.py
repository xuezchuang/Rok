"""Source verification for ArmyShow rules/list navigation, not visual acceptance."""
import json
from pathlib import Path
from BuildRokUnityUiAnimationManifest import Builder

ROOT = Path(__file__).resolve().parents[1]
UI = "ref/client-unity/Assets/BundleAssets/UI/"
MEDIATOR = "ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/Troop/UI_Win_ArmyShowMediator.cs"


def main():
    builder = Builder()
    mediator = builder.read(MEDIATOR)
    show_armys = mediator.split("private void ShowArmys()", 1)[1].split("private void ShowList()", 1)[0]
    assert 'if (!m_open)' in show_armys and 'm_open = true;' in show_armys
    assert show_armys.index('else') < show_armys.index('view.m_pl_blet_Animator.Play("Show")')
    assert 'view.m_sv_armys_Animator.Play("Show")' in show_armys
    show_list = mediator.split("private void ShowList()", 1)[1].split("private void ShowDetail", 1)[0]
    assert 'view.m_sv_desc_Animator.Play("Show")' in show_list
    refresh = mediator.split('private void OnRefreshTroopView()', 1)[1].split('private void AssetLoadFinish()', 1)[0]
    assert '.FillContent(count)' in refresh and '.ForceRefresh()' in refresh
    assert '.Play(' not in refresh and 'ShowArmys(' not in refresh
    clips = []
    for name, start in [("Level", 50), ("Content", -50)]:
        prefix = UI + "UIAnimation/shared_uianimation/UA_BuildingDesc_" + name
        clip = builder.clip(prefix + "Show.anim")
        controller = builder.read(prefix + ".controller")
        state = controller.split('m_Name: Show', 1)[1].split('--- !u!', 1)[0]
        assert clip['source_guid'] in state
        assert clip['duration_seconds'] == 0.25 and not clip['loop']
        assert len(clip['tracks']) == 2
        for track in clip['tracks']:
            assert track['path'] == ''
            assert [k['time'] for k in track['keys']] == [0, 0.25]
            assert [k['value'] for k in track['keys']] == ([start, 0] if track['attribute'] == 'm_AnchoredPosition.x' else [0, 1])
            assert all(k['weighted_mode'] == k['in_slope'] == k['out_slope'] == 0 for k in track['keys'])
        clips.append(clip)
    report = {'source_verified': True, 'sources': builder.sources, 'clips': clips,
              'ue_target': 'Source/Rok/RokUnityFeatureScreenWidget.cpp',
              'runtime_entry': 'OpenArmyShow -> ArmyShowRulesButton -> ArmyShowRulesBackButton',
              'first_open': 'No child animation, matching m_open guard',
              'data_refresh': 'OnRefreshTroopView refreshes list contents without restarting Animator',
              'remaining_gates': ['UE5.8 build and Rok.Ui.Animation.ArmyShow.PanelNavigation', 'Real HUD interaction and Unity visual comparison']}
    output = ROOT / 'Saved/RokUnityMigration/army_show_panel_animation_source_verify.json'
    output.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding='utf-8')
    print(json.dumps({'source_verified': True, 'clips': 2, 'output': str(output)}))


if __name__ == '__main__':
    main()
