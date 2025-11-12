SET(SOURCES screens/ui_screen_main.c
    screens/ui_screen_charging.c
    ui.c
    components/ui_comp_hook.c
    ui_helpers.c
    images/ui_img_emboo_logowhite_png.c
    images/ui_img_emboo_logosunsetorange_png.c
    fonts/ui_font_Speed.c
    fonts/ui_font_Heading.c
    fonts/ui_font_Value_Small.c
    fonts/ui_font_Label_Small.c)

add_library(ui ${SOURCES})
