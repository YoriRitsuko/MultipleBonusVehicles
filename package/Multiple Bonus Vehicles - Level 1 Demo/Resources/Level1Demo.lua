-- Add our two locators to the stock chunk so the demo can skip shipping the map.
local mapPath = "/GameDir/art/l1z1.p3d"
local fixturePath = GetModPath() .. "/Resources/l1z1.p3d"
if not Exists(mapPath, true, false) or not Exists(fixturePath, true, false) then
    return
end

local map = ReadFile(mapPath)
local fixture = ReadFile(fixturePath)
local p3dHeader = string.pack("<I4I4", 0xff443350, 12)

if map and fixture and #map == 1176186 and #fixture == 118 and
   map:sub(1, 8) == p3dHeader and fixture:sub(1, 8) == p3dHeader and
   string.unpack("<I4", map, 9) == #map and string.unpack("<I4", fixture, 9) == #fixture then
    Output(p3dHeader .. string.pack("<I4", #map + #fixture - 12) ..
           map:sub(13) .. fixture:sub(13))
end
