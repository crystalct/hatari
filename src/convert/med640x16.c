/* Screen Conversion, Medium Res to 640x16Bit */

void ConvertMediumRes_640x16Bit(void)
{
 unsigned long *edi, *ebp;
 unsigned short *esi;
 register unsigned long eax, ebx, ecx, edx;

 edx=0;

 Convert_StartFrame();            /* Start frame, track palettes */
 ScrY = STScreenStartHorizLine;   /* Starting line in ST screen */

// do      /* y-loop */
//  {
/*
NAKED void ConvertMediumRes_640x16Bit_YLoop(void)
{
*/

   eax = STScreenLineOffset[ScrY] + STScreenLeftSkipBytes;  /* Offset for this line + Amount to skip on left hand side */
   edi = (unsigned long *)((char *)pSTScreen + eax);        /* ST format screen 4-plane 16 colours */
   ebp = (unsigned long *)((char *)pSTScreenCopy + eax);    /* Previous ST format screen */
   esi = (unsigned short *)pPCScreenDest;                   /* PC format screen */
/*
    // Get screen addresses, 'edi'-ST screen, 'ebp'-Previous ST screen, 'esi'-PC screen
    mov    eax,[ScrY]
    mov    eax,STScreenLineOffset[eax*4]      // Offset for this line
    add    eax,[STScreenLeftSkipBytes]        // Amount to skip on left hand side
    mov    edi,[pSTScreen]              // ST format screen 4-plane 16 colours
    add    edi,eax
    mov    ebp,[pSTScreenCopy]            // Previous ST format screen
    add    ebp,eax
    mov    esi,[pPCScreenDest]            // PC format screen, byte per pixel 256 colours
*/

   if( AdjustLinePaletteRemap() & 0x00030000 )  /* Change palette table */
     /*???goto Line_ConvertLowRes_640x16Bit*/;
/*
    call  AdjustLinePaletteRemap          // Change palette table, DO NOT corrupt edx,edi,esi or ebp!
    test  eax,0x00030000
    je    Line_ConvertLowRes_640x16Bit      // resolution change(MUST be first)
    jmp    Line_ConvertMediumRes_640x16Bit      // 0 palette same, can do check tests
  }
}
NAKED void Line_ConvertMediumRes_640x16Bit(void)
{
  __asm {
    mov    eax,[STScreenWidthBytes]        // Amount to draw across
    shr    eax,2                  // in 16-pixels(4 bytes)
    mov    [ScrX],eax
x_loop:
#ifdef TEST_SCREEN_UPDATE
    mov    WORD PTR [esi+0],0
    mov    WORD PTR [esi+4],0
    mov    WORD PTR [esi+8],0
    mov    WORD PTR [esi+12],0
    mov    WORD PTR [esi+16],0
    mov    WORD PTR [esi+20],0
    mov    WORD PTR [esi+24],0
    mov    WORD PTR [esi+28],0
#endif  //TEST_SCREEN_UPDATE

    // Do 16 pixels at one time
    mov    ebx,[edi]
    // Full update? or just test changes?
    test  [ScrUpdateFlag],0xe0000000
    jne    copy_word                // Force
    // Does differ?
    cmp    ebx,[ebp]
    je    next_word                // Pixels are same as last frame, so ignore

copy_word:
    mov    [bScreenContentsChanged],TRUE

    // Plot in 'wrong-order', as ebx is 68000 endian
    cmp    [bScrDoubleY],TRUE            // Double on Y?
    je    double_y

    MED_BUILD_PIXELS_0                // Generate 'ecx' as pixels [4,5,6,7]
    PLOT_MED_640_16BIT(8)
    MED_BUILD_PIXELS_1                // Generate 'ecx' as pixels [12,13,14,15]
    PLOT_MED_640_16BIT(24)
    MED_BUILD_PIXELS_2                // Generate 'ecx' as pixels [0,1,2,3]
    PLOT_MED_640_16BIT(0)
    MED_BUILD_PIXELS_3                // Generate 'ecx' as pixels [8,9,10,11]
    PLOT_MED_640_16BIT(16)

    jmp    done_word

double_y:
    push  ebp
    mov    ebp,[PCScreenBytesPerLine]
    MED_BUILD_PIXELS_0                // Generate 'ecx' as pixels [4,5,6,7]
    PLOT_MED_640_16BIT_DOUBLE_Y(8)
    MED_BUILD_PIXELS_1                // Generate 'ecx' as pixels [12,13,14,15]
    PLOT_MED_640_16BIT_DOUBLE_Y(24)
    MED_BUILD_PIXELS_2                // Generate 'ecx' as pixels [0,1,2,3]
    PLOT_MED_640_16BIT_DOUBLE_Y(0)
    MED_BUILD_PIXELS_3                // Generate 'ecx' as pixels [8,9,10,11]
    PLOT_MED_640_16BIT_DOUBLE_Y(16)
    pop    ebp
done_word:

next_word:
    add    esi,16*2                // Next PC pixels
    add    edi,4                  // Next ST pixels
    add    ebp,4                  // Next ST copy pixels

    dec    [ScrX]
    jne    x_loop                  // Loop on X

    mov    eax,[pPCScreenDest]
    add    eax,[PCScreenBytesPerLine]        // Offset to next line
    add    eax,[PCScreenBytesPerLine]
    mov    [pPCScreenDest],eax

    inc    [ScrY]
    mov    eax,[STScreenEndHorizLine]
    cmp    [ScrY],eax
    jne    ConvertMediumRes_640x16Bit_YLoop    // And on Y

    pop    ebx
    pop    esi
    pop    edi
    pop    ebp

    ret
  }
*/
}

