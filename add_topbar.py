#!/usr/bin/env python3
"""
Add TopBar UI section to lvgl-demo.spj
"""

import json
import sys

def create_topbar():
    """Create TopBar with SOC, Logo, and Time containers"""
    return {
        "guid": "GUID_TOP_BAR",
        "deepid": -1000001,
        "dont_export": False,
        "locked": False,
        "properties": [
            {"nid": 10, "strtype": "OBJECT/Name", "strval": "TopBar", "InheritedType": 10},
            {"nid": 50, "flags": 17, "strtype": "OBJECT/Position", "intarray": [0, 0], "InheritedType": 7},
            {"nid": 60, "flags": 17, "strtype": "OBJECT/Size", "intarray": [800, 70], "InheritedType": 7},
            {"nid": 70, "strtype": "OBJECT/Align", "strval": "TOP_MID", "InheritedType": 3},
            {
                "Flow": 0,
                "Wrap": False,
                "Reversed": False,
                "MainAlignment": 1,
                "CrossAlignment": 0,
                "TrackAlignment": 0,
                "LayoutType": 2,
                "nid": 30,
                "strtype": "OBJECT/Layout_type",
                "strval": "Flex",
                "InheritedType": 13
            },
            {"nid": 110, "strtype": "OBJECT/Clickable", "strval": "False", "InheritedType": 2},
            {"nid": 230, "strtype": "OBJECT/Scrollable", "strval": "False", "InheritedType": 2},
            {
                "part": "lv.PART.MAIN",
                "childs": [
                    {
                        "nid": 10000,
                        "strtype": "_style/StyleState",
                        "strval": "DEFAULT",
                        "childs": [
                            {"nid": 10510, "strtype": "_style/Bg_Opa", "integer": 204, "InheritedType": 6},
                            {"nid": 10520, "strtype": "_style/Bg_Color", "intarray": [30, 41, 59, 255], "InheritedType": 7},
                            {"nid": 10710, "strtype": "_style/Border_Color", "intarray": [249, 115, 22, 76], "InheritedType": 7},
                            {"nid": 10711, "strtype": "_style/Border_Width", "integer": 1, "InheritedType": 6},
                            {"nid": 10713, "strtype": "_style/Border_Side", "integer": 8, "InheritedType": 6},
                            {"nid": 10810, "strtype": "_style/Pad_Left", "integer": 20, "InheritedType": 6},
                            {"nid": 10811, "strtype": "_style/Pad_Right", "integer": 20, "InheritedType": 6}
                        ],
                        "InheritedType": 1
                    }
                ],
                "nid": 1060,
                "strtype": "OBJECT/Style_main",
                "InheritedType": 11
            }
        ],
        "saved_objtypeKey": "OBJECT",
        "children": [
            create_soc_container(),
            create_logo_container(),
            create_time_container()
        ]
    }

def create_soc_container():
    """SOC% and Range container"""
    return {
        "guid": "GUID_CONTAINER_SOC",
        "deepid": -1000010,
        "dont_export": False,
        "locked": False,
        "properties": [
            {"nid": 10, "strtype": "OBJECT/Name", "strval": "ContainerSOC", "InheritedType": 10},
            {"nid": 60, "flags": 17, "strtype": "OBJECT/Size", "intarray": [150, 60], "InheritedType": 7},
            {
                "Flow": 1,
                "Wrap": False,
                "LayoutType": 2,
                "nid": 30,
                "strtype": "OBJECT/Layout_type",
                "strval": "Flex",
                "InheritedType": 13
            },
            {
                "part": "lv.PART.MAIN",
                "childs": [
                    {"nid": 10000, "childs": [
                        {"nid": 10510, "strtype": "_style/Bg_Opa", "integer": 0, "InheritedType": 6},
                        {"nid": 10710, "strtype": "_style/Border_Width", "integer": 0, "InheritedType": 6}
                    ]}
                ],
                "nid": 1060,
                "strtype": "OBJECT/Style_main",
                "InheritedType": 11
            }
        ],
        "saved_objtypeKey": "OBJECT",
        "children": [
            {
                "guid": "GUID_LABEL_SOC",
                "deepid": -1000011,
                "properties": [
                    {"nid": 10, "strtype": "OBJECT/Name", "strval": "LabelSOC", "InheritedType": 10},
                    {"nid": 1030, "strtype": "LABEL/Text", "strval": "100%", "InheritedType": 10},
                    {
                        "part": "lv.PART.MAIN",
                        "childs": [
                            {"nid": 10000, "childs": [
                                {"nid": 11510, "strtype": "_style/Text_Color", "intarray": [16, 185, 129, 255], "InheritedType": 7},
                                {"nid": 11530, "strtype": "_style/Text_Font", "strval": "montserrat_32", "InheritedType": 3}
                            ]}
                        ],
                        "nid": 1050,
                        "strtype": "LABEL/Style_main",
                        "InheritedType": 11
                    }
                ],
                "saved_objtypeKey": "LABEL"
            },
            {
                "guid": "GUID_LABEL_RANGE",
                "deepid": -1000012,
                "properties": [
                    {"nid": 10, "strtype": "OBJECT/Name", "strval": "LabelRange", "InheritedType": 10},
                    {"nid": 1030, "strtype": "LABEL/Text", "strval": "245 km", "InheritedType": 10},
                    {
                        "part": "lv.PART.MAIN",
                        "childs": [
                            {"nid": 10000, "childs": [
                                {"nid": 11510, "strtype": "_style/Text_Color", "intarray": [148, 163, 184, 255], "InheritedType": 7},
                                {"nid": 11530, "strtype": "_style/Text_Font", "strval": "montserrat_14", "InheritedType": 3}
                            ]}
                        ],
                        "nid": 1050,
                        "strtype": "LABEL/Style_main",
                        "InheritedType": 11
                    }
                ],
                "saved_objtypeKey": "LABEL"
            }
        ]
    }

def create_logo_container():
    """Logo and tagline container"""
    return {
        "guid": "GUID_CONTAINER_LOGO",
        "deepid": -1000020,
        "dont_export": False,
        "locked": False,
        "properties": [
            {"nid": 10, "strtype": "OBJECT/Name", "strval": "ContainerLogo", "InheritedType": 10},
            {"nid": 60, "flags": 17, "strtype": "OBJECT/Size", "intarray": [150, 60], "InheritedType": 7},
            {
                "Flow": 1,
                "LayoutType": 2,
                "nid": 30,
                "strtype": "OBJECT/Layout_type",
                "strval": "Flex",
                "InheritedType": 13
            },
            {
                "part": "lv.PART.MAIN",
                "childs": [
                    {"nid": 10000, "childs": [
                        {"nid": 10510, "strtype": "_style/Bg_Opa", "integer": 0, "InheritedType": 6},
                        {"nid": 10710, "strtype": "_style/Border_Width", "integer": 0, "InheritedType": 6}
                    ]}
                ],
                "nid": 1060,
                "strtype": "OBJECT/Style_main",
                "InheritedType": 11
            }
        ],
        "saved_objtypeKey": "OBJECT",
        "children": [
            {
                "guid": "GUID_IMAGE_LOGO",
                "deepid": -1000021,
                "properties": [
                    {"nid": 10, "strtype": "OBJECT/Name", "strval": "ImageLogo", "InheritedType": 10},
                    {"nid": 60, "flags": 51, "strtype": "OBJECT/Size", "intarray": [50, 50], "InheritedType": 7},
                    {"nid": 1020, "strtype": "IMAGE/Asset", "strval": "assets/EMBOO_LogoSunsetOrange.png", "InheritedType": 5},
                    {"nid": 1050, "strtype": "IMAGE/Scale", "integer": 256, "InheritedType": 6}
                ],
                "saved_objtypeKey": "IMAGE"
            },
            {
                "guid": "GUID_LABEL_POWERED",
                "deepid": -1000022,
                "properties": [
                    {"nid": 10, "strtype": "OBJECT/Name", "strval": "LabelPowered", "InheritedType": 10},
                    {"nid": 1030, "strtype": "LABEL/Text", "strval": "POWERED BY EMBOO", "InheritedType": 10},
                    {
                        "part": "lv.PART.MAIN",
                        "childs": [
                            {"nid": 10000, "childs": [
                                {"nid": 11510, "strtype": "_style/Text_Color", "intarray": [148, 163, 184, 255], "InheritedType": 7},
                                {"nid": 11530, "strtype": "_style/Text_Font", "strval": "montserrat_10", "InheritedType": 3}
                            ]}
                        ],
                        "nid": 1050,
                        "strtype": "LABEL/Style_main",
                        "InheritedType": 11
                    }
                ],
                "saved_objtypeKey": "LABEL"
            }
        ]
    }

def create_time_container():
    """Time display container"""
    return {
        "guid": "GUID_CONTAINER_TIME",
        "deepid": -1000030,
        "dont_export": False,
        "locked": False,
        "properties": [
            {"nid": 10, "strtype": "OBJECT/Name", "strval": "ContainerTime", "InheritedType": 10},
            {"nid": 60, "flags": 17, "strtype": "OBJECT/Size", "intarray": [150, 60], "InheritedType": 7},
            {
                "Flow": 1,
                "LayoutType": 2,
                "nid": 30,
                "strtype": "OBJECT/Layout_type",
                "strval": "Flex",
                "InheritedType": 13
            },
            {
                "part": "lv.PART.MAIN",
                "childs": [
                    {"nid": 10000, "childs": [
                        {"nid": 10510, "strtype": "_style/Bg_Opa", "integer": 0, "InheritedType": 6},
                        {"nid": 10710, "strtype": "_style/Border_Width", "integer": 0, "InheritedType": 6}
                    ]}
                ],
                "nid": 1060,
                "strtype": "OBJECT/Style_main",
                "InheritedType": 11
            }
        ],
        "saved_objtypeKey": "OBJECT",
        "children": [
            {
                "guid": "GUID_LABEL_TIME",
                "deepid": -1000031,
                "properties": [
                    {"nid": 10, "strtype": "OBJECT/Name", "strval": "LabelTime", "InheritedType": 10},
                    {"nid": 1030, "strtype": "LABEL/Text", "strval": "14:23", "InheritedType": 10},
                    {
                        "part": "lv.PART.MAIN",
                        "childs": [
                            {"nid": 10000, "childs": [
                                {"nid": 11510, "strtype": "_style/Text_Color", "intarray": [255, 255, 255, 255], "InheritedType": 7},
                                {"nid": 11530, "strtype": "_style/Text_Font", "strval": "montserrat_28", "InheritedType": 3}
                            ]}
                        ],
                        "nid": 1050,
                        "strtype": "LABEL/Style_main",
                        "InheritedType": 11
                    }
                ],
                "saved_objtypeKey": "LABEL"
            },
            {
                "guid": "GUID_LABEL_TIME_CAPTION",
                "deepid": -1000032,
                "properties": [
                    {"nid": 10, "strtype": "OBJECT/Name", "strval": "LabelTimeCaption", "InheritedType": 10},
                    {"nid": 1030, "strtype": "LABEL/Text", "strval": "LOCAL TIME", "InheritedType": 10},
                    {
                        "part": "lv.PART.MAIN",
                        "childs": [
                            {"nid": 10000, "childs": [
                                {"nid": 11510, "strtype": "_style/Text_Color", "intarray": [148, 163, 184, 255], "InheritedType": 7},
                                {"nid": 11530, "strtype": "_style/Text_Font", "strval": "montserrat_10", "InheritedType": 3}
                            ]}
                        ],
                        "nid": 1050,
                        "strtype": "LABEL/Style_main",
                        "InheritedType": 11
                    }
                ],
                "saved_objtypeKey": "LABEL"
            }
        ]
    }

def main():
    spj_file = "/home/user/test-lvgl-cross-compile/lvgl-demo/lvgl-demo.spj"

    print("Loading SPJ file...")
    with open(spj_file, 'r', encoding='utf-8') as f:
        spj_data = json.load(f)

    # Find the "screen main" in children
    screen_main = None
    for child in spj_data["root"]["children"]:
        # Check if this is screen main
        for prop in child.get("properties", []):
            if prop.get("strtype") == "OBJECT/Name" and prop.get("strval") == "screen main":
                screen_main = child
                break
        if screen_main:
            break

    if not screen_main:
        print("ERROR: Could not find 'screen main'")
        return 1

    print("Found 'screen main'")

    # Check if TopBar already exists
    topbar_exists = False
    for child in screen_main.get("children", []):
        for prop in child.get("properties", []):
            if prop.get("strtype") == "OBJECT/Name" and prop.get("strval") == "TopBar":
                topbar_exists = True
                break

    if topbar_exists:
        print("TopBar already exists, skipping...")
        return 0

    # Add TopBar as first child
    topbar = create_topbar()
    if "children" not in screen_main:
        screen_main["children"] = []

    # Insert TopBar at the beginning
    screen_main["children"].insert(0, topbar)

    print("Added TopBar to screen main")

    # Save the modified SPJ
    with open(spj_file, 'w', encoding='utf-8') as f:
        json.dump(spj_data, f, indent=2)

    print(f"✅ Updated {spj_file}")
    print("\nNext steps:")
    print("1. Open lvgl-demo.spj in SquareLine Studio")
    print("2. Export UI files to ui-dashboard/src/ui")
    print("3. Rebuild: cd ui-dashboard/build && make -j4")

    return 0

if __name__ == "__main__":
    sys.exit(main())
