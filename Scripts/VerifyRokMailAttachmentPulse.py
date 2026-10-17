"""Verify the actual mail attachment parent animation, not Spine or authority."""
import json
import re
from pathlib import Path
from BuildRokUnityUiAnimationManifest import Builder
from VerifyRokLeaderboardOverviewLayout import documents, node, vector

ROOT = Path(__file__).resolve().parents[1]
UI = 'ref/client-unity/Assets/BundleAssets/UI/'
CLIP = UI + 'UIAnimation/shared_uianimation/UA_Item_HotTask_bag.anim'
PREFAB = UI + 'UIPrefabs/ui_prefabs_mail/UIMail/UI_Item_MailBtn.prefab'


def main():
    builder = Builder()
    raw = builder.read(PREFAB)
    parts = node(documents(raw), 'img_bag')
    rect = parts[0]
    assert vector(rect, 'm_SizeDelta') == (28, 25)
    assert vector(rect, 'm_Pivot') == (0, 1)
    assert vector(rect, 'm_AnchoredPosition') == (-191.8, 82.8)
    assert any('_arab_Layout: 2' in p for p in parts)
    assert any('m_Color: {r: 1, g: 1, b: 1, a: 0}' in p for p in parts)
    legacy = next(p for p in parts if p.startswith('Animation:'))
    assert 'm_PlayAutomatically: 1' in legacy and 'm_Enabled: 1' in legacy
    clip = builder.clip(CLIP)
    assert clip['source_guid'] in legacy
    clip_text = builder.read(CLIP)
    assert 'm_Legacy: 1' in clip_text and 'm_WrapMode: 2' in clip_text
    curve = clip_text.split('  m_ScaleCurves:', 1)[1].split('  m_FloatCurves:', 1)[0]
    keys = [(float(t), float(x), float(y), float(z)) for t, x, y, z in re.findall(
        r'time: ([\d.]+)\s+value: \{x: ([\d.]+), y: ([\d.]+), z: ([\d.]+)\}', curve)]
    expected = [(0, 1, 1, 1), (.2, 1.15, 1.15, 1), (.36666667, 1, 1, 1),
                (.56666666, 1.15, 1.15, 1), (.75, 1, 1, 1), (2, 1, 1, 1)]
    assert keys == expected
    assert len(re.findall(r'inSlope: \{x: 0, y: 0, z: 0\}', curve)) == 6
    assert len(re.findall(r'outSlope: \{x: 0, y: 0, z: 0\}', curve)) == 6
    assert len(re.findall(r'weightedMode: 0', curve)) == 6
    assert 'm_PositionCurves: []' in clip_text and 'm_EulerCurves: []' in clip_text
    subview = builder.read('ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/Common/Logic/UI_Item_MailBtn_SubView.cs')
    assert 'emailInfo.takeEnclosure == false && emailInfo.rewards != null' in subview
    assert 'm_img_bag_PolygonImage.gameObject.SetActive(true)' in subview
    assert 'm_img_bag_PolygonImage.gameObject.SetActive(false)' in subview
    mediator = builder.read('ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/Mail/MailMediator.cs')
    assert 'private void OnEmailItemEnter(ListView.ListItem item)' in mediator
    assert 'subView.Refresh(m_currentTypeEmails[index], index, m_selectEmailIndex)' in mediator
    arab = builder.read('ref/client-unity/Assets/Scripts/Client/Compoment/ArabLayout/ArabLayoutCompment.cs')
    assert 'pivot.x = 1 - pivot.x;' in arab and 'isArabUI == Skyunion.LanguageUtils.IsArabic()' in arab
    assert 'value: UE_TreasureBox_stay' in raw and 'value: gift5' in raw
    helper = (ROOT / 'Source/Rok/RokMailAttachmentPulse.h').read_text(encoding='utf-8')
    assert '{0.0, 0.2, 0.36666667, 0.56666666, 0.75, 2.0}' in helper
    assert '{1.0, 1.15, 1.0, 1.15, 1.0, 1.0}' in helper
    assert 'FVector2D(1.0, 0.0)' in helper
    report = {'source_verified': True, 'sources': builder.sources, 'clip': clip,
        'legacy_wrap': 'Loop (m_WrapMode=2); not Animator controller', 'keys_xyz': keys,
        'source_parent_size': [28, 25], 'serialized_pivot': [0, 1],
        'chinese_unity_pivot': [1, 1], 'umg_pivot': [1, 0],
        'visibility_gate': 'takeEnclosure == false && rewards != null',
        'scope': 'Only parent scale pulse on existing reference attachment. No claim/authority mutation; Spine child stay excluded.',
        'remaining_gates': ['Main-agent FeatureScreen integration and UE build', 'Rok.Ui.Mail.AttachmentPulse',
                            'Native original-clip samples vs actual UE runtime pulse', 'Full Spine UE_TreasureBox_stay remains unported']}
    out = ROOT / 'Saved/RokUnityMigration/mail_attachment_pulse_source_verify.json'
    out.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps({'source_verified': True, 'keys': len(keys), 'output': str(out)}))


if __name__ == '__main__':
    main()
