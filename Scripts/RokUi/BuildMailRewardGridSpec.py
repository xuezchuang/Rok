"""Extract an editable five-column reward grid from the existing mixed fixture spec."""

from copy import deepcopy

from PrepareFeaturePageSpecs import canvas, find_node, read_spec, write_spec


def main():
    mixed = read_spec("MailRewardsMixed")
    empty = read_spec("MailRewardsEmpty")
    grid = deepcopy(find_node(mixed["nodes"], "MailRewardFixtureGrid"))
    placements = [
        canvas(f"MailRewardGridCell_{column}", find_node([grid], f"MailRewardFixtureRoot_{column}")["slot"])
        for column in range(5)
    ]
    placements.append(canvas("MailRewardGridSecondRow", find_node([grid], "MailRewardFixtureRoot_5")["slot"]))
    layout = canvas("MailRewardGridLayout", grid["slot"], placements)
    layout["props"]["Visibility"] = "Collapsed"
    grid["children"] = []

    notice = deepcopy(find_node(empty["nodes"], "MailDetailUnavailable"))
    notice["name"] = "MailRewardFixtureEmpty"
    notice["props"]["Text"] = 'NSLOCTEXT("RokFeature", "MailNoRewardPayload", "该邮件没有附件数据。")'
    notice["props"]["Font"] = notice["props"]["Font"].replace("Size=19.000000", "Size=17.000000")
    notice["slot"]["props"]["LayoutData"] = (
        "(Offsets=(Left=22.000000,Top=237.000000,Right=785.000000,Bottom=32.000000))"
    )
    write_spec(mixed, "feature_MailRewardGridPage", [grid, layout, notice])


if __name__ == "__main__":
    main()
