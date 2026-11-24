-- Keys glow by how often they were pressed: blue (rarely) -> red (often). Cools down slowly.
params = {
  cooling = { min = 0, max = 0.2, default = 0.02, label = "Cooling per second" },
}

local heat = {}

function frame(ctx)
  for _, k in ipairs(ctx.pressed) do heat[k] = math.min(1, (heat[k] or 0) + 0.08) end
  for _, key in ipairs(ctx.keys) do
    local h = heat[key.index] or 0
    h = math.max(0, h - ctx.param.cooling * ctx.dt)
    heat[key.index] = h
    if h <= 0 then
      ctx.set(key.index, 0, 0, 12)
    else
      ctx.set(key.index, hsv(240 - 240 * h, 1, 0.25 + 0.75 * h))
    end
  end
end
