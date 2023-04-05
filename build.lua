grvpath = "../grv"
package.path = package.path .. ";" .. grvpath .. "/lua/?.lua"
require("grvbld")

grvbld.build_config.inc = {grvpath .. "/include"}
grvbld.build_exe("todo", grvpath .. "/src/grv.c", "main.c")

