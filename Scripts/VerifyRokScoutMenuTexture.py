"""Scout menu source/static verification; UE fixture and actual actor entry remain separate."""
import json
from PIL import Image
from BuildRokScoutMenuTexture import ROOT, OUTPUT, REPORT, extract


def verify():
    expected_image, expected_report = extract()
    assert json.loads(REPORT.read_text(encoding='utf-8')) == expected_report, 'Stale Scout icon source chain'
    result = Image.open(OUTPUT).convert('RGBA')
    assert result.size == (88, 88) and result.tobytes() == expected_image.tobytes(), 'Source sprite pixels differ'
    panel = (ROOT / 'Source/Rok/RokBuildingPanelWidget.cpp').read_text(encoding='utf-8-sig')
    refresh = panel.split('void URokBuildingPanelWidget::Refresh()', 1)[1].split('void URokBuildingPanelWidget::ApplyMenuLayout', 1)[0]
    assert 'State.UnityBuildingTypeId == 28 && State.Level >= 1 && State.Level <= 25' in refresh
    visual = panel.split('void URokBuildingPanelWidget::ApplyFeatureButtonVisual(', 1)[1].split('void URokBuildingPanelWidget::UpdateMenuPosition()', 1)[0]
    assert 'UnityBuildingTypeId == 28 && !bUpgradeInProgress' in visual
    assert 'MakeImageBrush(ScoutButtonTexture.Get(), FVector2D(88.0f))' in visual
    assert 'ScoutButtonTexture = LoadUiTexture(UiBuildingMenuScoutPath);' in panel
    report = {'source_verified': True, 'pixel_difference_count': 0,
              'menu_visible_levels': [1, 25], 'fixture_test': 'Rok.Ui.Scout.BuildingMenuFixtureRoute',
              'real_city_camp_actor_verified': False, 'visual_verified': False}
    (ROOT / 'Saved/RokUnityMigration/scout_menu_texture_verify.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps(report))


if __name__ == '__main__': verify()
