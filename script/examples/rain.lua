-- Rain drops falling down the columns.
params = {
  drops = { min = 1, max = 20, default = 6, label = "Drops per second" },
  speed = { min = 1, max = 15, default = 6, label = "Fall speed (rows per second)" },
}

local drops = {}

function frame(ctx)
  local p = ctx.param
  if math.random() < p.drops * ctx.dt then
    table.insert(drops, { x = math.random() * ctx.width, y = -0.5 })
  end
  for _, d in ipairs(drops) do d.y = d.y + p.speed * ctx.dt end
  for _, key in ipairs(ctx.keys) do
    local level = 0
    for _, d in ipairs(drops) do
      if d.x >= key.x and d.x < key.x + key.w then
        local dy = d.y - (key.y + 0.5)
        if dy > -0.5 and dy < 2 then level = math.max(level, dy < 0 and 1 + 2 * dy or 1 - dy / 2) end
      end
    end
    ctx.set(key.index, 20 * level, 90 * level, 255 * level)
  end
  for i = #drops, 1, -1 do
    if drops[i].y > ctx.height + 2 then table.remove(drops, i) end
  end
end
