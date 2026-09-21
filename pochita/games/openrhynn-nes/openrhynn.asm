.setcpu "6502"

PPUCTRL   = $2000
PPUMASK   = $2001
PPUSTATUS = $2002
OAMADDR   = $2003
PPUSCROLL = $2005
PPUADDR   = $2006
PPUDATA   = $2007
OAMDMA    = $4014
JOY1      = $4016
OAM       = $0200

PAD_A      = $80
PAD_B      = $40
PAD_SELECT = $20
PAD_START  = $10
PAD_UP     = $08
PAD_DOWN   = $04
PAD_LEFT   = $02
PAD_RIGHT  = $01

STATE_TITLE = 0
STATE_GAME  = 1
STATE_EXIT  = 2

SAVE_TEXT = $6000
SAVE_DATA = $6200
SAVE_LEVEL = SAVE_DATA + 4
SAVE_XP = SAVE_DATA + 5
SAVE_KILLS = SAVE_DATA + 6
SAVE_QUEST = SAVE_DATA + 7
SAVE_GOLD = SAVE_DATA + 8
SAVE_ITEM = SAVE_DATA + 9
SAVE_X = SAVE_DATA + 10
SAVE_Y = SAVE_DATA + 11
SAVE_WOLF1 = SAVE_DATA + 12
SAVE_WOLF2 = SAVE_DATA + 13
SAVE_WOLF3 = SAVE_DATA + 14

.segment "HEADER"
    .byte "NES", $1A
    .byte 2                 ; 32 KiB PRG ROM
    .byte 1                 ; 8 KiB CHR ROM
    .byte $02               ; mapper 0, horizontal mirroring, battery SRAM
    .byte $00
    .byte $01               ; one 8 KiB PRG RAM bank
    .byte $00, $00, $00, $00, $00, $00, $00

.segment "ZEROPAGE"
frame_ready:  .res 1
buttons:      .res 1
buttons_prev: .res 1
pressed:      .res 1
game_state:   .res 1
exit_yes:     .res 1
player_x:     .res 1
player_y:     .res 1
level:        .res 1
xp:           .res 1
kills:        .res 1
quest:        .res 1
gold:         .res 1
item:         .res 1
wolf1_alive:  .res 1
wolf2_alive:  .res 1
wolf3_alive:  .res 1
target_x:     .res 1
target_y:     .res 1
actor_x:      .res 1
actor_y:      .res 1
actor_tile:   .res 1
ptr:          .res 2
temp:         .res 1

.segment "CODE"

.proc reset
    sei
    cld
    ldx #$40
    stx $4017
    ldx #$FF
    txs
    inx
    stx PPUCTRL
    stx PPUMASK
    stx $4010

    jsr wait_vblank

    lda #$00
    tax
@clear_ram:
    sta $0000,x
    sta $0100,x
    sta $0200,x
    sta $0300,x
    sta $0400,x
    sta $0500,x
    sta $0600,x
    sta $0700,x
    inx
    bne @clear_ram

    jsr wait_vblank
    jsr load_save
    lda #STATE_TITLE
    sta game_state
    jsr hide_all_sprites
    jsr draw_title

@main:
    lda frame_ready
    beq @main
    lda #0
    sta frame_ready
    jsr read_controller

    lda game_state
    beq @title
    cmp #STATE_GAME
    beq @game
    jsr process_exit
    jmp @update
@title:
    jsr process_title
    jmp @update
@game:
    jsr process_game
@update:
    jsr update_oam
    jmp @main
.endproc

.proc nmi
    pha
    txa
    pha
    tya
    pha
    lda #0
    sta OAMADDR
    lda #$02
    sta OAMDMA
    lda #0
    sta PPUSCROLL
    sta PPUSCROLL
    lda #1
    sta frame_ready
    pla
    tay
    pla
    tax
    pla
    rti
.endproc

.proc irq
    rti
.endproc

.proc wait_vblank
@wait:
    bit PPUSTATUS
    bpl @wait
    rts
.endproc

.proc ppu_begin
    lda #0
    sta PPUCTRL
    sta PPUMASK
    jsr wait_vblank
    bit PPUSTATUS
    rts
.endproc

.proc ppu_end
    lda #0
    sta PPUSCROLL
    sta PPUSCROLL
    lda #%10001000          ; NMI, sprites from pattern table 1
    sta PPUCTRL
    lda #%00011110          ; background + sprites, including left edge
    sta PPUMASK
    rts
.endproc

.proc load_palette
    lda #$3F
    sta PPUADDR
    lda #$00
    sta PPUADDR
    ldx #0
@loop:
    lda palette,x
    sta PPUDATA
    inx
    cpx #32
    bne @loop
    rts
.endproc

.proc clear_nametable
    lda #$20
    sta PPUADDR
    lda #$00
    sta PPUADDR
    lda #0
    ldx #4
    ldy #0
@page:
    sta PPUDATA
    iny
    bne @page
    dex
    bne @page
    rts
.endproc

.proc load_game_nametable
    lda #<game_nametable
    sta ptr
    lda #>game_nametable
    sta ptr+1
    lda #$20
    sta PPUADDR
    lda #$00
    sta PPUADDR
    ldx #4
    ldy #0
@copy:
    lda (ptr),y
    sta PPUDATA
    iny
    bne @copy
    inc ptr+1
    dex
    bne @copy
    rts
.endproc

; A = PPU address high, X = low, ptr = zero-terminated string.
.proc write_string
    sta PPUADDR
    stx PPUADDR
    ldy #0
@loop:
    lda (ptr),y
    beq @done
    sta PPUDATA
    iny
    bne @loop
@done:
    rts
.endproc

.proc write_two_digits
    ldx #0
@tens:
    cmp #10
    bcc @done_tens
    sec
    sbc #10
    inx
    bne @tens
@done_tens:
    pha
    txa
    clc
    adc #'0'
    sta PPUDATA
    pla
    clc
    adc #'0'
    sta PPUDATA
    rts
.endproc

.proc draw_title
    jsr ppu_begin
    jsr clear_nametable
    jsr load_palette

    lda #<title_main
    sta ptr
    lda #>title_main
    sta ptr+1
    lda #$21
    ldx #$48
    jsr write_string

    lda #<title_sub
    sta ptr
    lda #>title_sub
    sta ptr+1
    lda #$21
    ldx #$C4
    jsr write_string

    lda #<title_start
    sta ptr
    lda #>title_start
    sta ptr+1
    lda #$22
    ldx #$8A
    jsr write_string

    lda #<title_save
    sta ptr
    lda #>title_save
    sta ptr+1
    lda #$22
    ldx #$EA
    jsr write_string
    jsr ppu_end
    rts
.endproc

.proc draw_game
    jsr ppu_begin
    jsr load_game_nametable
    jsr load_palette

    lda #<hud_title
    sta ptr
    lda #>hud_title
    sta ptr+1
    lda #$20
    ldx #$01
    jsr write_string

    lda #<hud_level
    sta ptr
    lda #>hud_level
    sta ptr+1
    lda #$20
    ldx #$21
    jsr write_string
    lda level
    clc
    adc #'0'
    sta PPUDATA

    lda #<hud_xp
    sta ptr
    lda #>hud_xp
    sta ptr+1
    lda #$20
    ldx #$28
    jsr write_string
    lda xp
    jsr write_two_digits

    lda #<hud_kills
    sta ptr
    lda #>hud_kills
    sta ptr+1
    lda #$20
    ldx #$30
    jsr write_string
    lda kills
    clc
    adc #'0'
    sta PPUDATA

    lda quest
    beq @quest0
    cmp #1
    beq @quest1
    lda #<quest_done
    sta ptr
    lda #>quest_done
    sta ptr+1
    jmp @quest_write
@quest0:
    lda #<quest_find
    sta ptr
    lda #>quest_find
    sta ptr+1
    jmp @quest_write
@quest1:
    lda #<quest_hunt
    sta ptr
    lda #>quest_hunt
    sta ptr+1
@quest_write:
    lda #$20
    ldx #$41
    jsr write_string

    lda #<hud_controls
    sta ptr
    lda #>hud_controls
    sta ptr+1
    lda #$23
    ldx #$81
    jsr write_string
    jsr ppu_end
    rts
.endproc

.proc draw_exit
    jsr ppu_begin
    jsr clear_nametable
    jsr load_palette
    lda #<exit_title
    sta ptr
    lda #>exit_title
    sta ptr+1
    lda #$21
    ldx #$48
    jsr write_string
    lda #<exit_saved
    sta ptr
    lda #>exit_saved
    sta ptr+1
    lda #$21
    ldx #$C5
    jsr write_string
    lda exit_yes
    beq @no
    lda #<exit_yes_text
    sta ptr
    lda #>exit_yes_text
    sta ptr+1
    jmp @choice
@no:
    lda #<exit_no_text
    sta ptr
    lda #>exit_no_text
    sta ptr+1
@choice:
    lda #$22
    ldx #$49
    jsr write_string
    jsr ppu_end
    rts
.endproc

.proc read_controller
    lda #1
    sta JOY1
    lda #0
    sta JOY1
    sta buttons
    ldx #8
@loop:
    lda JOY1
    lsr a
    rol buttons
    dex
    bne @loop
    lda buttons_prev
    eor #$FF
    and buttons
    sta pressed
    lda buttons
    sta buttons_prev
    rts
.endproc

.proc process_title
    lda pressed
    and #(PAD_START | PAD_A)
    beq @done
    lda #STATE_GAME
    sta game_state
    jsr draw_game
@done:
    rts
.endproc

.proc process_game
    lda pressed
    and #PAD_SELECT
    beq @movement
    lda #0
    sta exit_yes
    lda #STATE_EXIT
    sta game_state
    jsr hide_all_sprites
    jsr draw_exit
    rts

@movement:
    lda buttons
    and #PAD_UP
    beq @down
    lda player_y
    cmp #49
    bcc @down
    dec player_y
@down:
    lda buttons
    and #PAD_DOWN
    beq @left
    lda player_y
    cmp #216
    bcs @left
    inc player_y
@left:
    lda buttons
    and #PAD_LEFT
    beq @right
    lda player_x
    cmp #17
    bcc @right
    dec player_x
@right:
    lda buttons
    and #PAD_RIGHT
    beq @action
    lda player_x
    cmp #232
    bcs @action
    inc player_x
@action:
    lda pressed
    and #PAD_A
    beq @done
    jsr perform_action
@done:
    rts
.endproc

.proc process_exit
    lda pressed
    and #(PAD_LEFT | PAD_RIGHT)
    beq @confirm
    lda exit_yes
    eor #1
    sta exit_yes
    jsr draw_exit
@confirm:
    lda pressed
    and #PAD_B
    beq @a
    lda #STATE_GAME
    sta game_state
    jsr draw_game
    rts
@a:
    lda pressed
    and #PAD_A
    beq @done
    lda exit_yes
    beq @cancel
    jsr save_game
    lda #STATE_TITLE
    sta game_state
    jsr hide_all_sprites
    jsr draw_title
    rts
@cancel:
    lda #STATE_GAME
    sta game_state
    jsr draw_game
@done:
    rts
.endproc

.proc is_near
    lda player_x
    sec
    sbc target_x
    bcs @x_positive
    eor #$FF
    clc
    adc #1
@x_positive:
    cmp #24
    bcs @far
    lda player_y
    sec
    sbc target_y
    bcs @y_positive
    eor #$FF
    clc
    adc #1
@y_positive:
    cmp #24
    bcs @far
    sec
    rts
@far:
    clc
    rts
.endproc

.proc perform_action
    lda #40
    sta target_x
    lda #96
    sta target_y
    jsr is_near
    bcc @wolves
    jsr talk_elder
    rts
@wolves:
    lda quest
    beq @done
    lda wolf1_alive
    beq @wolf2
    lda #144
    sta target_x
    lda #80
    sta target_y
    jsr is_near
    bcc @wolf2
    lda #0
    sta wolf1_alive
    jsr wolf_defeated
    rts
@wolf2:
    lda wolf2_alive
    beq @wolf3
    lda #192
    sta target_x
    lda #144
    sta target_y
    jsr is_near
    bcc @wolf3
    lda #0
    sta wolf2_alive
    jsr wolf_defeated
    rts
@wolf3:
    lda wolf3_alive
    beq @done
    lda #104
    sta target_x
    lda #184
    sta target_y
    jsr is_near
    bcc @done
    lda #0
    sta wolf3_alive
    jsr wolf_defeated
@done:
    rts
.endproc

.proc talk_elder
    lda quest
    bne @return_quest
    lda #1
    sta quest
    jsr save_game
    jsr draw_game
    rts
@return_quest:
    cmp #1
    bne @done
    lda kills
    cmp #3
    bcc @done
    lda #2
    sta quest
    lda #1
    sta item
    lda gold
    clc
    adc #20
    sta gold
    jsr save_game
    jsr draw_game
@done:
    rts
.endproc

.proc wolf_defeated
    inc kills
    lda xp
    clc
    adc #5
    sta xp
    lda gold
    clc
    adc #3
    sta gold
    lda xp
    cmp #15
    bcc @save
    lda level
    cmp #2
    bcs @save
    inc level
@save:
    jsr save_game
    jsr draw_game
    rts
.endproc

.proc update_oam
    lda game_state
    cmp #STATE_GAME
    beq @actors
    jsr hide_all_sprites
    rts
@actors:
    lda player_x
    sta actor_x
    lda player_y
    sta actor_y
    lda #0
    sta actor_tile
    ldx #0
    jsr draw_actor

    lda #40
    sta actor_x
    lda #96
    sta actor_y
    lda #4
    sta actor_tile
    ldx #16
    jsr draw_actor

    lda wolf1_alive
    beq @hide1
    lda #144
    sta actor_x
    lda #80
    sta actor_y
    lda #8
    sta actor_tile
    ldx #32
    jsr draw_actor
    jmp @wolf2
@hide1:
    ldx #32
    jsr hide_actor
@wolf2:
    lda wolf2_alive
    beq @hide2
    lda #192
    sta actor_x
    lda #144
    sta actor_y
    lda #8
    sta actor_tile
    ldx #48
    jsr draw_actor
    jmp @wolf3
@hide2:
    ldx #48
    jsr hide_actor
@wolf3:
    lda wolf3_alive
    beq @hide3
    lda #104
    sta actor_x
    lda #184
    sta actor_y
    lda #8
    sta actor_tile
    ldx #64
    jsr draw_actor
    rts
@hide3:
    ldx #64
    jsr hide_actor
    rts
.endproc

.proc draw_actor
    lda actor_y
    sta OAM,x
    inx
    lda actor_tile
    sta OAM,x
    inx
    lda #0
    sta OAM,x
    inx
    lda actor_x
    sta OAM,x
    inx

    lda actor_y
    sta OAM,x
    inx
    lda actor_tile
    clc
    adc #1
    sta OAM,x
    inx
    lda #0
    sta OAM,x
    inx
    lda actor_x
    clc
    adc #8
    sta OAM,x
    inx

    lda actor_y
    clc
    adc #8
    sta OAM,x
    inx
    lda actor_tile
    clc
    adc #2
    sta OAM,x
    inx
    lda #0
    sta OAM,x
    inx
    lda actor_x
    sta OAM,x
    inx

    lda actor_y
    clc
    adc #8
    sta OAM,x
    inx
    lda actor_tile
    clc
    adc #3
    sta OAM,x
    inx
    lda #0
    sta OAM,x
    inx
    lda actor_x
    clc
    adc #8
    sta OAM,x
    rts
.endproc

.proc hide_actor
    lda #$F8
    sta OAM,x
    sta OAM+4,x
    sta OAM+8,x
    sta OAM+12,x
    rts
.endproc

.proc hide_all_sprites
    ldx #0
    lda #$F8
@loop:
    sta OAM,x
    inx
    inx
    inx
    inx
    bne @loop
    rts
.endproc

.proc load_save
    lda SAVE_DATA
    cmp #'R'
    bne @new
    lda SAVE_DATA+1
    cmp #'H'
    bne @new
    lda SAVE_DATA+2
    cmp #'Y'
    bne @new
    lda SAVE_DATA+3
    cmp #'1'
    bne @new
    lda SAVE_LEVEL
    sta level
    lda SAVE_XP
    sta xp
    lda SAVE_KILLS
    sta kills
    lda SAVE_QUEST
    sta quest
    lda SAVE_GOLD
    sta gold
    lda SAVE_ITEM
    sta item
    lda SAVE_X
    sta player_x
    lda SAVE_Y
    sta player_y
    lda SAVE_WOLF1
    sta wolf1_alive
    lda SAVE_WOLF2
    sta wolf2_alive
    lda SAVE_WOLF3
    sta wolf3_alive
    rts
@new:
    lda #1
    sta level
    sta wolf1_alive
    sta wolf2_alive
    sta wolf3_alive
    lda #0
    sta xp
    sta kills
    sta quest
    sta gold
    sta item
    lda #72
    sta player_x
    lda #112
    sta player_y
    jsr save_game
    rts
.endproc

.proc save_game
    lda #'R'
    sta SAVE_DATA
    lda #'H'
    sta SAVE_DATA+1
    lda #'Y'
    sta SAVE_DATA+2
    lda #'1'
    sta SAVE_DATA+3
    lda level
    sta SAVE_LEVEL
    lda xp
    sta SAVE_XP
    lda kills
    sta SAVE_KILLS
    lda quest
    sta SAVE_QUEST
    lda gold
    sta SAVE_GOLD
    lda item
    sta SAVE_ITEM
    lda player_x
    sta SAVE_X
    lda player_y
    sta SAVE_Y
    lda wolf1_alive
    sta SAVE_WOLF1
    lda wolf2_alive
    sta SAVE_WOLF2
    lda wolf3_alive
    sta SAVE_WOLF3

    ldx #0
@copy_text:
    lda save_text_template,x
    sta SAVE_TEXT,x
    inx
    cpx #save_text_length
    bne @copy_text
    lda level
    clc
    adc #'0'
    sta SAVE_TEXT + save_level_offset
    lda kills
    clc
    adc #'0'
    sta SAVE_TEXT + save_kills_offset
    lda quest
    clc
    adc #'0'
    sta SAVE_TEXT + save_quest_offset
    lda item
    clc
    adc #'0'
    sta SAVE_TEXT + save_item_offset
    lda xp
    ldy #save_xp_offset
    jsr save_two_digits
    lda gold
    ldy #save_gold_offset
    jsr save_two_digits
    rts
.endproc

.proc save_two_digits
    ldx #0
@tens:
    cmp #10
    bcc @store
    sec
    sbc #10
    inx
    bne @tens
@store:
    pha
    txa
    clc
    adc #'0'
    sta SAVE_TEXT,y
    iny
    pla
    clc
    adc #'0'
    sta SAVE_TEXT,y
    rts
.endproc

.segment "RODATA"
palette:
    .byte $0F,$19,$09,$30, $0F,$30,$10,$00, $0F,$17,$27,$37, $0F,$06,$16,$26
    .byte $0F,$21,$11,$30, $0F,$16,$27,$30, $0F,$07,$17,$30, $0F,$28,$38,$30

title_main:     .asciiz "OPENRHYNN"
title_sub:      .asciiz "THE ELDERWOOD CHRONICLE"
title_start:    .asciiz "PRESS START"
title_save:     .asciiz "BATTERY SAVE ENABLED"
hud_title:      .asciiz "OPENRHYNN  POCHITA"
hud_level:      .asciiz "LV:"
hud_xp:         .asciiz "XP:"
hud_kills:      .asciiz "WOLVES:"
quest_find:     .asciiz "FIND ELDER - PRESS A"
quest_hunt:     .asciiz "QUEST: HUNT 3 WOLVES"
quest_done:     .asciiz "ELDERWOOD SAVED!"
hud_controls:   .asciiz "A ACTION  SELECT EXIT"
exit_title:     .asciiz "EXIT OPENRHYNN?"
exit_saved:     .asciiz "PROGRESS WILL BE SAVED"
exit_yes_text:  .asciiz "YES!    NO"
exit_no_text:   .asciiz "YES    NO!"

save_text_template:
    .byte "OPENRHYNN_SAVE_V1",10
    .byte "NAME=POCHITA",10
    .byte "LEVEL="
save_level_digit:
    .byte "0",10
    .byte "XP="
save_xp_digits:
    .byte "00",10
    .byte "KILLS="
save_kills_digit:
    .byte "0",10
    .byte "QUEST="
save_quest_digit:
    .byte "0",10
    .byte "GOLD="
save_gold_digits:
    .byte "00",10
    .byte "ITEM="
save_item_digit:
    .byte "0",10,0
save_text_end:

save_text_length = save_text_end - save_text_template
save_level_offset = save_level_digit - save_text_template
save_xp_offset = save_xp_digits - save_text_template
save_kills_offset = save_kills_digit - save_text_template
save_quest_offset = save_quest_digit - save_text_template
save_gold_offset = save_gold_digits - save_text_template
save_item_offset = save_item_digit - save_text_template

game_nametable:
    .incbin "openrhynn.nam"

.segment "VECTORS"
    .word nmi
    .word reset
    .word irq

.segment "CHARS"
    .incbin "openrhynn.chr"
