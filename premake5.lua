newoption {
    trigger = "enable-diagnostics",
    value="BOOL",
    default="true",
    description = "Toggles diagnostics tools (Debug menu, screenshot taking)",
    category="Build Options",
    allowed = {
        { "true",   "Enable diagnostic tools" },
        { "false",  "Disable diagnostic tools" }
    }
}

local assetsSrcPath = "assets-src"
local assetsPath = "assets"
local fontsPath = "assets-src/fonts"
local outPath = "out"

workspace "sc23el-msc-project"
    language "C++"
    cppdialect "C++23"

    platforms { "x64" }
    configurations { "debug", "release" }

    flags "NoPCH"
    flags "MultiProcessorCompile"

    startproject "ssr"

    debugdir "%{wks.location}"
    objdir "_build_/%{cfg.buildcfg}-%{cfg.platform}-%{cfg.toolset}"
    targetsuffix "-%{cfg.buildcfg}-%{cfg.platform}-%{cfg.toolset}"

    -- Default toolset options
    filter "toolset:gcc or toolset:clang"
        linkoptions { "-pthread" }
        buildoptions { "-march=native", "-Wall", "-pthread" }

    filter "toolset:msc-*"
        defines { "_CRT_SECURE_NO_WARNINGS=1" }
        defines { "_SCL_SECURE_NO_WARNINGS=1" }
        buildoptions { "/utf-8" }

    filter "*"

    -- default libraries
    filter "system:linux"
        links {
            "dl", "gtk-3", "gdk-3", "gobject-2.0", "glib-2.0", "gio-2.0", "cairo", "z"
        }
        buildoptions { "`pkg-config --cflags gtk+-3.0`" }
        linkoptions { "`pkg-config --libs gtk+-3.0`" }


filter "system:windows"
        links { "opengl32", "gdi32" }

    filter "*"

    -- default outputs
    filter "kind:StaticLib"
        targetdir "lib/"

    filter "kind:ConsoleApp"
        targetdir "bin/"
        targetextension ".exe"

    filter "*"

    --configurations
    filter "debug"
        symbols "On"
        defines { "_DEBUG=1" }

    filter "release"
        optimize "On"
        defines { "NDEBUG=1" }

    filter "*"

-- Third party dependencies
include "third-party"

-- GLSLC helpers
dofile( "util/glslc.lua" )

-- Projects
project "ssr"
    local sources = {
        "ssr/**.cpp",
        "ssr/**.hpp",
        "ssr/**.hxx"
    }

    kind "ConsoleApp"
    location "ssr"

    files( sources )

    defines { "ASSETS_SRC_PATH_=\"" .. assetsSrcPath .. "\"" }
    defines { "ASSETS_PATH_=\"" .. assetsPath .. "\"" }
    defines { "FONTS_PATH_=\"" .. fontsPath .. "\"" }
    defines { "OUT_PATH_=\"" .. outPath .. "\"" }

    dependson "ssr-shaders"

    links "vkutils"
    links "x-volk"
    links "x-stb"
    links "x-glfw"
    links "x-vma"
    filter "options:enable-diagnostics=true"
        defines { "ENABLE_DIAGNOSTICS" }
        links { "x-imgui", "x-nfd" }

    dependson "x-glm"

project "ssr-shaders"
    local shaders = {
        "ssr/shaders/*.vert",
        "ssr/shaders/*.frag",
        "ssr/shaders/*.comp",
        "ssr/shaders/*.geom",
        "ssr/shaders/*.tesc",
        "ssr/shaders/*.tese"
    }

    kind "Utility"
    location "ssr/shaders"

    files( shaders )

    handle_glsl_files( "-O", assetsPath.."/shaders", {} )

project "assets-bake"
    local sources = {
        "assets-bake/**.cpp",
        "assets-bake/**.hpp",
        "assets-bake/**.hxx"
    }

    kind "ConsoleApp"
    location "assets-bake"

    files( sources )

    defines { "ASSETS_SRC_PATH_=\"" .. assetsSrcPath .. "\"" }
    defines { "ASSETS_PATH_=\"" .. assetsPath .. "\"" }

    links "vkutils" -- for vkutils::Error
    links "x-tgen"
    links "x-zstd"

    dependson "x-glm"
    dependson "x-rapidobj"

project "vkutils"
    local sources = {
        "vkutils/**.cpp",
        "vkutils/**.hpp",
        "vkutils/**.hxx"
    }

    kind "StaticLib"
    location "vkutils"

    files( sources )

    defines { "ASSETS_PATH_=\"" .. assetsPath .. "\"" }

    filter "options:enable-diagnostics=true"
        defines { "ENABLE_DIAGNOSTICS" }