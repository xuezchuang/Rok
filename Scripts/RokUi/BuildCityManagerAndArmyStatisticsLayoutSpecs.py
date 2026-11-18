"""Author detached Designer layouts for city buffs and army statistics rows."""

from BuildMailAndArmyListLayoutSpecs import layout_node, write_spec


def build():
    city = [
        layout_node("CityManagerDetailExtent", (0, 0, 1024, 156)),
        layout_node("CityManagerDetailStride", (1040, 0, 40, 156)),
        layout_node("CityManagerDetailMinimum", (1100, 0, 40, 547)),
    ]
    army = [
        layout_node("ArmyStatisticsRowExtent", (0, 0, 965, 92)),
        layout_node("ArmyStatisticsWaitingExtent", (0, 108, 965, 90)),
        layout_node("ArmyStatisticsEmptyExtent", (0, 214, 965, 90)),
        layout_node("ArmyStatisticsEvenShade", (0, 320, 220, 64)),
        layout_node("ArmyStatisticsOddShade", (236, 320, 220, 64)),
    ]
    army[-2]["props"]["BrushColor"] = "(R=0.20,G=0.21,B=0.20,A=0.98)"
    army[-1]["props"]["BrushColor"] = "(R=0.16,G=0.17,B=0.16,A=0.98)"
    return city, army


def main():
    city, army = build()
    write_spec("CityManagerDetailLayout", "city_manager_detail_layout", city, [
        "The detail row extent sets the WBP row size; Stride height sets the vertical step.",
        "Minimum height sets the scroll content floor. Marker positions arrange the Designer samples only.",
        "Existing CityManagerDetailRow owns its contents and style; C++ preserves configuration and item actions.",
    ], "Source/Rok/RokUnityFeatureScreenWidget.cpp: BuildCityManager")
    write_spec("ArmyStatisticsListLayout", "army_statistics_list_layout", army, [
        "The three extents independently size regular, waiting and synchronized-empty WBP rows.",
        "EvenShade and OddShade brush colors alternate by row index. Marker positions are Designer samples only.",
        "Existing row/message WBPs own their contents; C++ preserves troop data and tab selection.",
    ], "Source/Rok/RokUnityFeatureScreenWidget.cpp: BuildArmyStatistics")


if __name__ == "__main__":
    main()
