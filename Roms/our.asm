    ;;--- 程序开始 ---;;  
  
    ; INES 文件头  
    .inesprg 1   ; 1 段代码  
    .ineschr 1   ; 1 段数据  
    .inesmir 1   ; 总是1  
    .inesmap 0   ; 使用mapper 0  

  
    .bank 1  
    .org $FFFA  
    .dw 0 ; VBlank中断向量  
    .dw Start ; 复位向量  
    .dw VBlank_Routine  ; 当BRK指令执行时的中断向量，改天再谈 
  
    .bank 0   ; bank 0 - 代码段 
    .org $0000
VBlankOrNo  .db 0

    .org $0300 ; OAM镜像从这里开始  
Sprite1_Y:     .db  0   ; 1号主角的纵坐标  
Sprite1_T:     .db  0   ; 1号主角的瓷砖编号  
Sprite1_S:     .db  0   ; 1号主角的特殊待遇  
Sprite1_X:     .db  0   ; 1号主角的横坐标
 
    .org $8000  ; 代码从 $8000开始  
VBlank_Routine:  
    ;VBlank子程序开始  
    inc VBlankOrNo  
    ; 使变量VBlankOrNo增1  
    ;jmp end
    rti  ; RTI 意思是中断返回 (Interrupt RETurn or ReTurn from Interrupt)  

      
Start:  ;还记得大明湖畔的复位向量吗？  
    lda #%00001000  ; 就像那天说的一样设置PPU  
    sta $2000       ;   
    lda #%00011110  ;   
    sta $2001       ;   
  
    ldx #$00    ; 准备载入调色板  
  
    lda #$3F    ;   
    sta $2006   ;   
    lda #$00    ;   
    sta $2006  
  
loadpal:                ;   
    lda tilepal, x  ;   
    sta $2007       ;   
    inx             ;   
    cpx #32         ;   
    bne loadpal     ;   



    lda #$20  
    sta $2006 ; 写起始地址$2020  
    sta $2006   
  
    ldx #$00  
loadNames:  
    lda ourMap, X ; 从地址(ourMap + X)处取一个字节，加载到A  
    inx  
    sta $2007  
    cpx #64 ; 我们上面一共写了64个字节；  
    bne loadNames ; 循环  

init:
    lda #100
    sta Sprite1_X
    sta Sprite1_Y  



infin: 
    ;rti
    

WaitForVBlank:         ; 等待 VBlank   
    lda $2002 ; 读取 $2002的值  
    bpl WaitForVBlank  ; 如果bit7 == 0，那就接着waitblank  
    
    ;lda VBlankOrNo ; A = VBlankOrNO  
    ;cmp #1         ; if A == 1 说明发生了VBlank  
    ;bne WaitForVBlank ; 没有发生VBlank，循环  
    ;dec VBlankOrNo ; 发生了VBlank，使变量VBlankOrNo减一，变为0  
    
    
     
  
  

    
    jsr RefreshManState

    jsr CheckManMove


    jmp infin   ; 死循环 

RefreshManState:
    lda #$3  ; 也可以写成 #3, 因为显然3的十进制和十六进制表示是相同的  
    sta $4014 ; 一旦写入, 拷贝就开始 


    ;lda #$00   ; 写入SPR-RAM  
    ;sta $2003  ;   
    ;lda #$00   ;   
    ;sta $2003  ;   
  
    lda Sprite1_Y
    sta $2004 ;   
    lda #$00  ;   
    sta $2004 ;   
    lda #$00 ;   
    sta $2004 ;   
    lda Sprite1_X;   
    sta $2004 ;   
    ; 注意顺序呀！ 
    rts

; ==================================

ManMoveUp:
    lda Sprite1_Y
    sbc #1
    sta Sprite1_Y
    jmp CheckManMoveEnd

ManMoveDown:
    lda Sprite1_Y
    adc #1
    sta Sprite1_Y
    jmp CheckManMoveEnd

ManMoveLeft:
    lda Sprite1_X
    sbc #1
    sta Sprite1_X
    jmp CheckManMoveEnd

ManMoveRight:
    lda Sprite1_X
    adc #1
    sta Sprite1_X
    jmp CheckManMoveEnd

CheckManMove:
    lda #$01    ; |  
    sta $4016   ;  \  
    lda #$00    ;   - 设置手柄用来读  
    sta $4016   ; _/  
  
    lda $4016  ; 读取按键A    
    lda $4016  ; 读取按键B  
    lda $4016  ; 读取按键SELECT  
    lda $4016  ; 读取按键START  
  
    lda $4016  ; UP  
    and #1     ; 判断是否按下  
    bne ManMoveUp  ; 按下就走
    lda $4016  ; DOWN  
    and #1     ; 判断是否按下  
    bne ManMoveDown  ; 按下就走
    lda $4016  ; LEFT  
    and #1     ; 判断是否按下  
    bne ManMoveLeft  ; 按下就走
    lda $4016  ; RIGHT     
    and #1     ; 判断是否按下  
    bne ManMoveRight  ; 按下就走

CheckManMoveEnd:

    rts ;jmp CheckManMoveEnd



; ==================================
  
tilepal:
    .incbin "our.pal" ; 包含调色板，贴标签  
ourMap:
    .incbin "our.map" ; 假设our.map是生成的二进制map文件  
  
    .bank 2   ; 数据段  
    .org $0000  ;   
    .incbin "our.bkg"  ; 空白背景数据  
    .incbin "our.spr"  ; 我们绘制的主角数据  
    ; 注意顺序呀！  
  
    ;;--- 代码结束 ---;; 
end: