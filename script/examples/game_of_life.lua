-- Conway's Game of Life on the key grid; typing brings cells to life.
params = {
  speed = { min = 1, max = 15, default = 4, label = "Generations per second" },
}

local alive, neighbours, acc = {}, {}, 0

function init(ctx)
  for _, a in ipairs(ctx.keys) do
    local list = {}
    for _, b in ipairs(ctx.keys) do
      if a ~= b and math.abs((a.x + a.w / 2) - (b.x + b.w / 2)) < 1.3 and math.abs(a.y - b.y) < 1.3 then
        table.insert(list, b.index)
      end
    end
    neighbours[a.index] = list
  end
end

function frame(ctx)
  for _, k in ipairs(ctx.pressed) do alive[k] = true end
  acc = acc + ctx.dt * ctx.param.speed
  if acc >= 1 then
    acc = 0
    local next = {}
    for _, key in ipairs(ctx.keys) do
      local n = 0
      for _, j in ipairs(neighbours[key.index]) do if alive[j] then n = n + 1 end end
      next[key.index] = (alive[key.index] and (n == 2 or n == 3)) or (not alive[key.index] and n == 3)
    end
    alive = next
  end
  for _, key in ipairs(ctx.keys) do
    if alive[key.index] then ctx.set(key.index, 230, 230, 230) else ctx.set(key.index, 0, 0, 0) end
  end
end
