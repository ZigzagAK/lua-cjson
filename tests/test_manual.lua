json = require "cjson"

local function check_callback(n)
  local f, err = io.open(n, "r")

  local l = f:read("*a")

  f:close()

  local j
  local depth = {}
  local p

  local opts = {
    begin_object = function(name, level)
      p = {}
      if name then
        j[name] = p
      else
        if not j then
          j = p
        end
      end
      table.insert(depth, j)
      j = p
      return level == 2 and true or nil
    end,

    end_object = function()
      j = table.remove(depth)
    end,

    begin_array = function(name)
      p = {}
      if name then
        j[name] = p
      else
        if j then
          table.insert(j, p)
        else
          j = p
        end
      end
      table.insert(depth, j)
      j = p
    end,

    end_array = function()
      j = table.remove(depth)
    end,

    field = function(name, value, level)
      if level == 1 then
        if name then
          j[name] = value
        else
          table.insert(j, value)
        end
      end
    end
  }

  local compare
  compare = function(tab, tab_orig)
    for k,v in pairs(tab_orig)
    do
      if tab[k] == nil then
        print("tab[" .. k .. "]=nil")
        return false
      end
      if type(v) == "table" and type(tab[k]) == "table" then
        if not compare(tab[k], v) then
          return false
        end
      elseif v ~= tab[k] then
        print(v .. "~=" .. tab[k])
        return false
      end
    end
    return true
  end

  local compare
  compare = function(tab, tab_orig)
    for k,v in pairs(tab_orig)
    do
      if tab[k] == nil then
        print("tab[" .. k .. "]=nil")
        return false
      end
      if type(v) == "table" and type(tab[k]) == "table" then
        if not compare(tab[k], v) then
          return false
        end
      elseif v ~= tab[k] then
        --print(v .. "~=" .. tab[k])
        return false
      end
    end
    return true
  end

  json.decode(l, opts)

  local j_orig = json.decode(l)

  if compare(j, j_orig) and compare(j_orig, j) then
    return true
  else
    print("Fail")
    print(json.encode(j))
    print("==============")
    print(json.encode(j_orig))
  end
end

check_callback("rfc-example2.json")
