-- Flames rising from the bottom row; pressed keys flare up.
params = {
  intensity = { min = 0.2, max = 1, default = 0.7, label = "Intensity" },
}

local flare = {}

function frame(ctx)
  for _, k in ipairs(ctx.pressed) do flare[k] = 1 end
  for _, key in ipairs(ctx.keys) do
    local height = 1 - key.y / ctx.height                      -- 0 at the top, 1 at the bottom
    local flicker = 0.5 + 0.5 * math.sin(ctx.t * 7 + key.x * 1.7 + math.sin(ctx.t * 3 + key.x))
    local f = flare[key.index] or 0
    local heat = clamp(height * ctx.param.intensity * (0.6 + 0.4 * flicker) + f, 0, 1)
    ctx.set(key.index, 255 * clamp(heat * 1.4, 0, 1), 200 * heat * heat, 30 * heat ^ 4)
    flare[key.index] = math.max(0, f - ctx.dt * 1.5)
  end
end
