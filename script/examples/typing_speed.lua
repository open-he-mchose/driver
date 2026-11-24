-- Typing speedometer on the top row (Esc, F1-F12, ...): the row fills from left to right as you type
-- faster, green -> yellow -> red, with a white peak marker that slowly falls back.
-- Speed in words per minute (1 word = 5 key presses), measured over a sliding window.
params = {
  max_wpm    = { min = 30, max = 250, default = 120, label = "WPM for a full bar" },
  window     = { min = 1, max = 10, default = 3, label = "Measuring window (s)" },
  smoothing  = { min = 0, max = 0.95, default = 0.6, label = "Smoothing" },
  peak       = { min = 0, max = 1, default = 1, label = "Peak marker (0 = off)" },
  background = { min = 0, max = 2, default = 1, label = "Rest of keyboard: 0 off, 1 dim, 2 speed tint" },
}

local presses = {}     -- times of recent key presses
local speed = 0        -- smoothed WPM
local peak, peak_t = 0, 0
local bar = {}         -- top-row keys from left to right
local top = {}

function init(ctx)
  for _, key in ipairs(ctx.keys) do
    if key.y < 0.5 then table.insert(bar, key) end
  end
  table.sort(bar, function(a, b) return a.x < b.x end)
  for _, key in ipairs(bar) do top[key.index] = true end
end

-- green (120 deg) at 0 -> red (0 deg) at 1
local function level_colour(f, v)
  return hsv(120 * (1 - clamp(f, 0, 1)), 1, v)
end

function frame(ctx)
  local p = ctx.param
  for _ = 1, #ctx.pressed do table.insert(presses, ctx.t) end
  while #presses > 0 and presses[1] < ctx.t - p.window do table.remove(presses, 1) end

  local wpm = #presses / math.min(p.window, math.max(ctx.t, 0.5)) * 60 / 5
  speed = speed * p.smoothing + wpm * (1 - p.smoothing)
  if speed > peak then peak, peak_t = speed, ctx.t end
  if ctx.t - peak_t > 1.5 then peak = math.max(speed, peak - p.max_wpm * 0.25 * ctx.dt) end

  local n = #bar
  local fill = clamp(speed / p.max_wpm, 0, 1) * n           -- lit keys, fractional
  local peak_key = math.min(n, math.floor(clamp(peak / p.max_wpm, 0, 1) * n + 0.5))

  for i, key in ipairs(bar) do
    local f = (i - 0.5) / n                                  -- position along the bar
    local on = clamp(fill - (i - 1), 0, 1)                   -- partly lit last key
    local r, g, b = level_colour(f, 0.08 + 0.92 * on)
    if p.peak >= 0.5 and i == peak_key and peak > 1 then r, g, b = 255, 255, 255 end
    ctx.set(key.index, r, g, b)
  end

  local mode = math.floor(p.background + 0.5)
  for _, key in ipairs(ctx.keys) do
    if not top[key.index] then
      if mode == 0 then
        ctx.set(key.index, 0, 0, 0)
      elseif mode == 1 then
        ctx.set(key.index, 0, 0, 20)
      else
        ctx.set(key.index, level_colour(speed / p.max_wpm, 0.12 + 0.25 * clamp(speed / p.max_wpm, 0, 1)))
      end
    end
  end
end
