SET(SOURCES screens/ui_screen_main.c
    screens/ui_screen_charging.c
    ui.c
    components/ui_comp_hook.c
    ui_helpers.c
    images/ui_img_emboo_logosunsetorange_png.c)

add_library(ui ${SOURCES})
