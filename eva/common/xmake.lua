target("log", function()
    set_kind("static")
    set_encodings("source:utf-8")
    add_files("src/*.cpp")
    add_includedirs("include", { public = true })
end)
