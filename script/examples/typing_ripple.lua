-- Every key press starts a ring of light that spreads over the keyboard.
params = {
  speed = { min = 2, max = 30, default = 12, label = "Ring speed (keys per second)" },
  hue = { min = 0, max = 360, default = 180, label = "Colour (hue)" },
  background = { min = 0, max = 0.5, default = 0.05, label = "Background" },
}

local ripples = {}
local pos = {}

function init(ctx)
  for _, key in ipairs(ctx.keys) do pos[key.index] = { x = key.x + key.w / 2, y = key.y + 0.5 } end
end

function frame(ctx)
  for _, k in ipairs(ctx.pressed) do
    local p = pos[k]
    if p then table.insert(ripples, { x = p.x, y = p.y, t = ctx.t }) end
  end
  local p = ctx.param
  for _, key in ipairs(ctx.keys) do
    local c = pos[key.index]
    local level = p.background
    for _, r in ipairs(ripples) do
      local radius = (ctx.t - r.t) * p.speed
      local d = math.abs(math.sqrt((c.x - r.x) ^ 2 + (c.y - r.y) ^ 2) - radius)
      level = level + math.max(0, 1 - d) * math.max(0, 1 - radius / 20)
    end
    ctx.set(key.index, hsv(p.hue, 1, clamp(level, 0, 1)))
  end
  for i = #ripples, 1, -1 do
    if (ctx.t - ripples[i].t) * p.speed > 20 then table.remove(ripples, i) end
  end
end
