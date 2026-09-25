local WHITE = true
local BLACK = false
local board = {}
local turn = WHITE
local cursor = { row = 6, col = 4 }
local selected = nil
local legal_moves = {}
local last_move = nil
local status = 'WHITE MOVE'
local game_over = false

local function inside(row, col)
  return row >= 0 and row < 8 and col >= 0 and col < 8
end

local function piece(row, col)
  if not inside(row, col) then
    return nil
  end
  return board[row + 1][col + 1]
end

local function set_piece(row, col, value)
  board[row + 1][col + 1] = value
end

local function is_white(value)
  return value ~= nil and value:match('%u') ~= nil
end

local function side(value)
  if value == nil then
    return nil
  end
  return is_white(value) and WHITE or BLACK
end

local function kind(value)
  return value:lower()
end

local function opponent_matches(value, moving_side)
  return value ~= nil and side(value) ~= moving_side and kind(value) ~= 'k'
end

local function reset_game()
  board = {}
  for row = 1, 8 do
    board[row] = {}
  end
  board[1] = { 'r', 'n', 'b', 'q', 'k', 'b', 'n', 'r' }
  board[8] = { 'R', 'N', 'B', 'Q', 'K', 'B', 'N', 'R' }
  for col = 3, 6 do
    board[3][col] = 'p'
    board[7][col] = 'P'
  end
  turn = WHITE
  cursor = { row = 6, col = 4 }
  selected = nil
  legal_moves = {}
  last_move = nil
  status = 'WHITE MOVE'
  game_over = false
end

local function add_move(moves, row, col, new_row, new_col)
  if not inside(new_row, new_col) then
    return
  end
  local value = piece(new_row, new_col)
  if value ~= nil and (side(value) == turn or kind(value) == 'k') then
    return
  end
  moves[#moves + 1] = { row = row, col = col, new_row = new_row, new_col = new_col }
end

local function add_sliding(moves, row, col, row_step, col_step)
  local new_row = row + row_step
  local new_col = col + col_step
  while inside(new_row, new_col) do
    local value = piece(new_row, new_col)
    if value == nil then
      add_move(moves, row, col, new_row, new_col)
    else
      if opponent_matches(value, turn) then
        add_move(moves, row, col, new_row, new_col)
      end
      break
    end
    new_row = new_row + row_step
    new_col = new_col + col_step
  end
end

local function pseudo_moves(row, col)
  local moves = {}
  local value = piece(row, col)
  if value == nil or side(value) ~= turn then
    return moves
  end
  local value_kind = kind(value)
  local moving_side = side(value)
  if value_kind == 'p' then
    local direction = moving_side == WHITE and -1 or 1
    local start_row = moving_side == WHITE and 6 or 1
    local one_row = row + direction
    if inside(one_row, col) and piece(one_row, col) == nil then
      add_move(moves, row, col, one_row, col)
      if row == start_row and piece(row + direction * 2, col) == nil then
        add_move(moves, row, col, row + direction * 2, col)
      end
    end
    for _, col_step in ipairs({ -1, 1 }) do
      local capture_col = col + col_step
      if inside(one_row, capture_col) then
        local captured = piece(one_row, capture_col)
        if captured ~= nil and side(captured) ~= moving_side and kind(captured) ~= 'k' then
          add_move(moves, row, col, one_row, capture_col)
        end
      end
    end
  elseif value_kind == 'n' then
    for _, step in ipairs({ { -2, -1 }, { -2, 1 }, { -1, -2 }, { -1, 2 }, { 1, -2 }, { 1, 2 }, { 2, -1 }, { 2, 1 } }) do
      add_move(moves, row, col, row + step[1], col + step[2])
    end
  elseif value_kind == 'b' then
    for _, step in ipairs({ { -1, -1 }, { -1, 1 }, { 1, -1 }, { 1, 1 } }) do
      add_sliding(moves, row, col, step[1], step[2])
    end
  elseif value_kind == 'r' then
    for _, step in ipairs({ { -1, 0 }, { 1, 0 }, { 0, -1 }, { 0, 1 } }) do
      add_sliding(moves, row, col, step[1], step[2])
    end
  elseif value_kind == 'q' then
    for _, step in ipairs({ { -1, 0 }, { 1, 0 }, { 0, -1 }, { 0, 1 }, { -1, -1 }, { -1, 1 }, { 1, -1 }, { 1, 1 } }) do
      add_sliding(moves, row, col, step[1], step[2])
    end
  else
    for _, step in ipairs({ { -1, -1 }, { -1, 0 }, { -1, 1 }, { 0, -1 }, { 0, 1 }, { 1, -1 }, { 1, 0 }, { 1, 1 } }) do
      add_move(moves, row, col, row + step[1], col + step[2])
    end
  end
  return moves
end

local function find_king(white_side)
  local target = white_side and 'K' or 'k'
  for row = 0, 7 do
    for col = 0, 7 do
      if piece(row, col) == target then
        return row, col
      end
    end
  end
  return nil
end

local function attacked(row, col, by_white)
  local pawn_row = by_white and row + 1 or row - 1
  for _, col_step in ipairs({ -1, 1 }) do
    local pawn = piece(pawn_row, col + col_step)
    if pawn ~= nil and side(pawn) == by_white and kind(pawn) == 'p' then
      return true
    end
  end
  for _, step in ipairs({ { -2, -1 }, { -2, 1 }, { -1, -2 }, { -1, 2 }, { 1, -2 }, { 1, 2 }, { 2, -1 }, { 2, 1 } }) do
    local knight = piece(row + step[1], col + step[2])
    if knight ~= nil and side(knight) == by_white and kind(knight) == 'n' then
      return true
    end
  end
  for _, step in ipairs({ { -1, -1 }, { -1, 0 }, { -1, 1 }, { 0, -1 }, { 0, 1 }, { 1, -1 }, { 1, 0 }, { 1, 1 } }) do
    local king = piece(row + step[1], col + step[2])
    if king ~= nil and side(king) == by_white and kind(king) == 'k' then
      return true
    end
  end
  local directions = {
    { -1, 0, true }, { 1, 0, true }, { 0, -1, true }, { 0, 1, true },
    { -1, -1, false }, { -1, 1, false }, { 1, -1, false }, { 1, 1, false },
  }
  for _, direction in ipairs(directions) do
    local scan_row = row + direction[1]
    local scan_col = col + direction[2]
    while inside(scan_row, scan_col) do
      local value = piece(scan_row, scan_col)
      if value ~= nil then
        if side(value) == by_white then
          local value_kind = kind(value)
          if (direction[3] and (value_kind == 'r' or value_kind == 'q')) or
              (not direction[3] and (value_kind == 'b' or value_kind == 'q')) then
            return true
          end
        end
        break
      end
      scan_row = scan_row + direction[1]
      scan_col = scan_col + direction[2]
    end
  end
  return false
end

local function in_check(white_side)
  local king_row, king_col = find_king(white_side)
  return king_row ~= nil and attacked(king_row, king_col, not white_side)
end

local function leaves_king_safe(move)
  local moving = piece(move.row, move.col)
  local moving_side = side(moving)
  local captured = piece(move.new_row, move.new_col)
  set_piece(move.row, move.col, nil)
  set_piece(move.new_row, move.new_col, moving)
  local safe = not in_check(moving_side)
  set_piece(move.row, move.col, moving)
  set_piece(move.new_row, move.new_col, captured)
  return safe
end

local function legal_from(row, col)
  local legal = {}
  for _, move in ipairs(pseudo_moves(row, col)) do
    if leaves_king_safe(move) then
      legal[#legal + 1] = move
    end
  end
  return legal
end

local function has_legal_move(white_side)
  local previous_turn = turn
  turn = white_side
  for row = 0, 7 do
    for col = 0, 7 do
      if #legal_from(row, col) > 0 then
        turn = previous_turn
        return true
      end
    end
  end
  turn = previous_turn
  return false
end

local function only_kings_remain()
  local material = 0
  for row = 0, 7 do
    for col = 0, 7 do
      local value = piece(row, col)
      if value ~= nil and kind(value) ~= 'k' then
        material = material + 1
      end
    end
  end
  return material == 0
end

local function update_status()
  game_over = false
  if only_kings_remain() then
    game_over = true
    status = 'DRAW - KINGS'
  elseif not has_legal_move(turn) then
    game_over = true
    if in_check(turn) then
      status = turn == WHITE and 'BLACK WINS' or 'WHITE WINS'
    else
      status = 'DRAW - STALEMATE'
    end
  elseif in_check(turn) then
    status = turn == WHITE and 'WHITE IN CHECK' or 'BLACK IN CHECK'
  else
    status = turn == WHITE and 'WHITE MOVE' or 'BLACK MOVE'
  end
end

local function apply_move(move)
  local value = piece(move.row, move.col)
  set_piece(move.row, move.col, nil)
  set_piece(move.new_row, move.new_col, value)
  if kind(value) == 'p' and (move.new_row == 0 or move.new_row == 7) then
    set_piece(move.new_row, move.new_col, is_white(value) and 'Q' or 'q')
  end
  last_move = move
  selected = nil
  legal_moves = {}
  turn = not turn
  update_status()
end

local function is_legal_target(row, col)
  for _, move in ipairs(legal_moves) do
    if move.new_row == row and move.new_col == col then
      return true
    end
  end
  return false
end

local function select_or_move()
  if game_over then
    return
  end
  local value = piece(cursor.row, cursor.col)
  if selected == nil then
    if value ~= nil and side(value) == turn then
      selected = { row = cursor.row, col = cursor.col }
      legal_moves = legal_from(cursor.row, cursor.col)
      status = turn == WHITE and 'WHITE SELECT' or 'BLACK SELECT'
    else
      status = turn == WHITE and 'WHITE PIECE' or 'BLACK PIECE'
    end
    return
  end
  for _, move in ipairs(legal_moves) do
    if move.new_row == cursor.row and move.new_col == cursor.col then
      apply_move(move)
      return
    end
  end
  if value ~= nil and side(value) == turn then
    selected = { row = cursor.row, col = cursor.col }
    legal_moves = legal_from(cursor.row, cursor.col)
    status = turn == WHITE and 'WHITE SELECT' or 'BLACK SELECT'
  else
    status = 'ILLEGAL MOVE'
  end
end

function on_key(key, down)
  if not down then
    return
  end
  if key == 'left' then
    cursor.col = math.max(0, cursor.col - 1)
  elseif key == 'right' then
    cursor.col = math.min(7, cursor.col + 1)
  elseif key == 'up' then
    cursor.row = math.max(0, cursor.row - 1)
  elseif key == 'down' then
    cursor.row = math.min(7, cursor.row + 1)
  elseif key == 'start' then
    select_or_move()
  elseif key == 'option' then
    reset_game()
  end
end

function on_update(_)
end

local function same_square(row_a, col_a, row_b, col_b)
  return row_a == row_b and col_a == col_b
end

function on_draw()
  local origin_x = 8
  local origin_y = 24
  local cell = 28
  engine.clear(0x0841)
  engine.rect(0, 0, 240, 20, 0x18e3)
  engine.text(7, 6, 'CHESS 2P', 0xffff)
  engine.text(174, 6, turn == WHITE and 'WHITE' or 'BLACK', turn == WHITE and 0xffff or 0xfd20)
  for row = 0, 7 do
    for col = 0, 7 do
      local x = origin_x + col * cell
      local y = origin_y + row * cell
      local color = (row + col) % 2 == 0 and 0xf7de or 0x4b4b
      if last_move ~= nil and (same_square(row, col, last_move.row, last_move.col) or
          same_square(row, col, last_move.new_row, last_move.new_col)) then
        color = color == 0xf7de and 0xffff or 0xbdf7
      end
      if selected ~= nil and same_square(row, col, selected.row, selected.col) then
        color = 0xffe0
      end
      engine.rect(x, y, cell, cell, color)
      if selected ~= nil and is_legal_target(row, col) then
        engine.rect(x + 11, y + 11, 6, 6, 0x18e3)
      end
      local value = piece(row, col)
      if value ~= nil then
        local white_piece = is_white(value)
        local badge = white_piece and 0xffff or 0x0841
        local ink = white_piece and 0x0841 or 0xffff
        engine.rect(x + 4, y + 4, 20, 20, badge)
        engine.text(x + 11, y + 10, value, ink)
      end
    end
  end
  local cursor_x = origin_x + cursor.col * cell
  local cursor_y = origin_y + cursor.row * cell
  local cursor_color = turn == WHITE and 0xffff or 0xfd20
  engine.rect(cursor_x, cursor_y, cell, 2, cursor_color)
  engine.rect(cursor_x, cursor_y + cell - 2, cell, 2, cursor_color)
  engine.rect(cursor_x, cursor_y, 2, cell, cursor_color)
  engine.rect(cursor_x + cell - 2, cursor_y, 2, cell, cursor_color)
  engine.text(4, 256, status, game_over and 0xffe0 or 0xffff)
end

reset_game()
