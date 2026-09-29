-- ely-arcade-platform build script (the arcade menu). Shared logic lives in sdk/.
dofile("../sdk/premake/ely_sdk.lua")

ely.prepare_dirs()
ely.workspace("ely-arcade-platform")
ely.raylib_project()
ely.sdk_project("../sdk")
ely.app_project("ely-arcade-platform", "../src", "../sdk")
