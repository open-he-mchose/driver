-- Rainbow wave across the keyboard.
params = {
  speed = { min = 0.1, max = 5, default = 1, label = "Speed" },
  width = { min = 2, max = 40, default = 12, label = "Colours across the board" },
  brightness = { min = 0.1, max = 1, default = 0.7, label = "Brightness" },
}

function frame(ctx)
  local p = ctx.param
  for _, key in ipairs(ctx.keys) do
    local hue = (key.x + key.w / 2) * 360 / p.width - ctx.t * 90 * p.speed
    ctx.set(key.index, hsv(hue, 1, p.brightness))
  end
end
