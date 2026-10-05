#include "ui.hpp"
#include "mods/svc/ui.h"

#include "config.hpp"
#include "helpers/result.hpp"

namespace slugcat::gltf::ui {

namespace {

UiElementHandle elemPath = 0;
UiElementHandle elemScale = 0;

ModResult build(ModContext*, UiElementHandle panel, void*, ModError*) {
    svc_ui->pane_add_section(mod_ctx, panel, "Settings");

    UiControlDesc control1 = UI_CONTROL_DESC_INIT;
    control1.kind = UI_CONTROL_FILE_PICKER;
    control1.label = "Path";
    control1.help_rml = "Path to .vrm";
    control1.binding = UI_BINDING_CONFIG_VAR;
    control1.config_var = config::cVarPathHandle;
    svc_ui->pane_add_control(mod_ctx, panel, &control1, &elemPath);

    UiControlDesc control2 = UI_CONTROL_DESC_INIT;
    control2.kind = UI_CONTROL_NUMBER;
    control2.label = "VRM Scale";
    control2.help_rml = "Scale of the VRM in the world";
    control2.binding = UI_BINDING_CONFIG_VAR;
    control2.config_var = config::cVarVrmScaleHandle;
    control2.min = 1;
    control2.max = 1000;
    control2.step = 1;
    svc_ui->pane_add_control(mod_ctx, panel, &control2, &elemScale);

    return MOD_OK;
}

}  // namespace

void init() {
    UiModsPanelDesc panel = UI_MODS_PANEL_DESC_INIT;
    panel.build = build;
    helpers::checkResult(svc_ui->register_mods_panel(mod_ctx, &panel));
}

}  // namespace slugcat::gltf::ui
