;-------------------------------------------------------------------------------
            .cdecls C,LIST,"msp430.h"       ; Include device header file


;-------------------------------------------------------------------------------
			.ref	WriteLed				; reference the extern function
			.ref	disp1					; reference the global variable
			.ref	disp2					; reference the global variable

;-------------------------------------------------------------------------------
			.text
; Interrupt handler for TA2CCR0 CCIFG
CCR0ISR:	mov.w	#0000, R12				; clear R12
			cmp.b 	#1, R8					; check which one display should be shown now
			jz	  	segm1_ON				; if R8 = 1 go to segm1 turning on
		; turning segm2 on (if R8 = 2)
			mov.b	&disp2, R12				; pass disp2 trought R12 to WriteLed
			bis.b	#BIT0, &P7OUT			; turn off segm1
			call	#WriteLed				; set up a, b, c, ...
			bic.b	#BIT4, &P6OUT			; turn on segm2
			mov.w	#0001h, R8				; next time would be going to segm1 turning on
			jmp		endISR					; excape from routine
		; turning segm1 on (if R8 = 1)
segm1_ON:	mov.b	&disp1, R12				; pass disp1 trought R12 to WriteLed
			bis.b	#BIT4, &P6OUT			; turn off segm2
			call	#WriteLed				; set up a, b, c, ...
			bic.b	#BIT0, &P7OUT			; turn on segm1
			mov.w	#0002, R8				; next time would be going to segm2 turning on
endISR:		reti


;-------------------------------------------------------------------------------
; Interrupt Vectors
;-------------------------------------------------------------------------------
            .sect   ".int44"                ; MSP430 TIMER2_A0 Vector
            .short  CCR0ISR
