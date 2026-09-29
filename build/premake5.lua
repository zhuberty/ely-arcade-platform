-- ely-arcade-platform build script (the arcade menu). Shared logic lives in sdk/.
dofile("../sdk/premake/arcade_sdk.lua")

arcade.prepare_dirs()
arcade.workspace("ely-arcade-platform")
arcade.raylib_project()
arcade.sdk_project("../sdk")
arcade.app_project("ely-arcade-platform", "../src", "../sdk")
