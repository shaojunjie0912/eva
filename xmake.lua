set_project("eva")
set_xmakever("2.9.4")
set_languages("c++2a")

add_rules("plugin.compile_commands.autoupdate")
add_rules("mode.debug", "mode.release")

add_requires("toml++")

target("eva", function()
    set_kind("static")
    set_encodings("source:utf-8")
    add_files("src/*.cpp")
    add_includedirs("include")
    add_packages("toml++")
end)
