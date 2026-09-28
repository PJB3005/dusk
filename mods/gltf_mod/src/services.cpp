#include "mods/service.hpp"
#include "mods/svc/actor.h"
#include "mods/svc/camera.h"
#include "mods/svc/gfx.h"
#include "mods/svc/hook.h"
#include "mods/svc/interp.h"
#include "mods/svc/log.h"
#include "mods/svc/resource.h"
#include "mods/svc/stage.h"
#include "mods/svc/ui.h"

DEFINE_MOD();
IMPORT_SERVICE(ActorService, svc_actor);
IMPORT_SERVICE(CameraService, svc_camera);
IMPORT_SERVICE(ConfigService, svc_config);
IMPORT_SERVICE(GfxService, svc_gfx);
IMPORT_SERVICE(HookService, svc_hook);
IMPORT_SERVICE(InterpService, svc_interp);
IMPORT_SERVICE(LogService, svc_log);
IMPORT_SERVICE(ResourceService, svc_resource);
IMPORT_SERVICE(StageService, svc_stage);
IMPORT_SERVICE(UiService, svc_ui);
