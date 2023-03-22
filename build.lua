grvpath = "../grv"
package.path = package.path .. ";" .. grvpath .. "/util/?.lua"
require("grvbld")

grvbld.build_config.inc = {grvpath .. "/include"}
grvbld.build_exe("todo", grvpath .. "/src/grv.c", "main.c")

