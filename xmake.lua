set_project("eva")
set_xmakever("2.9.4")
set_languages("c++2a")

add_rules("plugin.compile_commands.autoupdate")
add_rules("mode.debug", "mode.release")

add_requires("toml++")

includes("eva")
includes("test")
