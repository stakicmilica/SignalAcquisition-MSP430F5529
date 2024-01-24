#include <msp430.h> 
#include <stdint.h>

/**
 * main.c
 */

#define TIMER_PERIOD        (163)  /* ~5ms (4.97ms)  32768/1000*5 = 163   32768-1sec -->  x-4.97ms */

#define CONV_PERIOD        (204)  /* 50ms */


volatile uint8_t conversion_flag = 0;

volatile uint8_t min_flag = 0;
volatile uint8_t max_flag = 0;
volatile uint8_t average_flag = 0;
volatile uint8_t end_flag = 0;

volatile uint8_t min = 0xff;
volatile uint8_t max = 0;
volatile uint16_t average = 0;

volatile uint8_t disp1 = 0;
volatile uint8_t disp2 = 0;
volatile uint8_t current_digit =0;

volatile uint8_t counter = 0;
volatile uint16_t sum = 0;
volatile uint8_t data = 0;

#pragma DATA_SECTION (MyRamData, ".MyRamSpace")
char MyRamData[200];

int main(void)
{
	WDTCTL = WDTPW | WDTHOLD;	// stop watchdog timer
	
    // configure button S1 for starting conversion
    P1REN |= BIT4;              // enable pull up/down
    P1OUT |= BIT4;              // set pull up
    P1DIR &= ~BIT4;             // configure P1.4 as in
    P1IES |= BIT4;              // interrupt on falling edge
    P1IFG &= ~BIT4;             // clear flag
    P1IE  |= BIT4;              // enable interrupt

	// configure button S2(pin p1.5) for min value
	P1REN |= BIT5;              // enable pull up/down
	P1OUT |= BIT5;              // set pull up
	P1DIR &= ~BIT5;             // configure P1.5 as in
	P1IES |= BIT5;              // interrupt on falling edge
	P1IFG &= ~BIT5;             // clear flag
	P1IE  |= BIT5;              // enable interrupt

	// configure button S3(pin 1.1) for max value
	P1REN |= BIT1;              // enable pull up/down
	P1OUT |= BIT1;              // set pull up
	P1DIR &= ~BIT1;             // configure P1.1 as in
	P1IES |= BIT1;              // interrupt on falling edge
	P1IFG &= ~BIT1;             // clear flag
	P1IE  |= BIT1;              // enable interrupt

	// configure button S3(pin 1.1) for average value
	P2REN |= BIT1;              // enable pull up/down
	P2OUT |= BIT1;              // set pull up
	P2DIR &= ~BIT1;             // configure P2.1 as in
	P2IES |= BIT1;              // interrupt on falling edge
	P2IFG &= ~BIT1;             // clear flag
	P2IE  |= BIT1;              // enable interrupt

    // sevenseg 1
    P7DIR |= BIT0;              // set P7.0 as out (SEL1)
    P7OUT |= BIT0;              // disable display 1
    // sevenseg 2
    P6DIR |= BIT4;              // set P6.4 as out (SEL2)
    P6OUT |= BIT4;              // disable display 2

    // a,b,c,d,e,f,g
    P2DIR |= 0x48;              // configure P2.3 and P2.6 as out
    P3DIR |= BIT7;                     // configure P3.7 as out
    P4DIR |= 0x09;              // configure P4.0 and P4.3 as out
    P8DIR |= 0x06;              // configure P8.1 and P8.2 as out

    //init TA2 - display mux
    TA2CCR0 = TIMER_PERIOD;     // set timer period in CCR0 register
    TA2CCTL0 = CCIE;            // enable interrupt for TA1CCR0
    TA2CTL = TASSEL__ACLK | MC__UP;     //clock select and up mode

    // LED1 init
    P2OUT &= ~BIT4;             // set P2.4 to '0'
    P2DIR |= BIT4;              // configure P2.4 as out

    /* initialize Timer A1  - for debouncing S1 */
    TA1CCR0 = TIMER_PERIOD;     // debounce period
    TA1CCTL0 |= CCIE;            // enable CCR0 interrupt
    TA1CTL |= TASSEL__ACLK;

    /* ADC12_A channel A0 init */
    P6SEL |= BIT0;              // input pin P6.0 - pot2
                                // set pin P6.0 as alternate function - A0 analog

    ADC12CTL0 &= ~ADC12ENC;     // disable ADC before configuring

    ADC12CTL0 |= ADC12ON;
    ADC12CTL1 |= ADC12SHS_1 + ADC12SSEL_3 + ADC12CONSEQ_2;      // start address ADC12MEM0, timer starts conv
                                                                // single channel continuous conversion, SMCLK clock

    ADC12CTL2 &= ~ADC12RES_2;   // 8 bit conversion result

    ADC12MCTL0 |= ADC12INCH_0;    //reference AVCC and AVSS, channel A0
    ADC12CTL0 |= ADC12ENC;        // enable ADC12

    ADC12IE |= ADC12IE0;        // enable interrupt when MEM0 is written



    /* initialize Timer A0 for ADC12 trigger */
    TA0CCR0 = (CONV_PERIOD/2  - 1);// f_OUT = 20 Hz, f_ACLK = 32768 Hz ID=8 => T_OUT = 32768/8/20 = 204.8 ~ 204
                                   // TOGGLE outmod => T_OUT = 2 * T_CCR0 =>
                                   // T_CCR0 = T_OUT/2 = 204/2 = 102 (TA0CCR0 = T_CCR0 - 1)

    TA0CCTL1 |= OUTMOD_4;      // toggle mode
    TA0CTL |= TASSEL__ACLK + ID__8;

    // init RAM
    //RCCTL0 = RCKEY;
    RCCTL0 = 0x5A00;

    __enable_interrupt();
    while(1)
    {
        if(conversion_flag)
        {
            TA0CTL |= MC__UP;  // start timer for triggering ADC12

            /* reseting values before conversion */
            conversion_flag = 0;
            min = 0xff;
            max = 0;
            sum = 0;
            counter = 0;
            average = 0;
        }
        if(end_flag)
        {
            //ADC12CTL0 &= ~ADC12ENC;
            TA0CTL &= ~(MC0 | MC1);      // stop and clear timer
            P2OUT &= ~ BIT4;             // set P2.4 to '0' - led off


            static uint8_t cnt = 0;
            for(cnt=0; cnt<0xC8; cnt++)
            {
                if(MyRamData[cnt] < min)
                min = MyRamData[cnt];

                if(MyRamData[cnt] > max)
                max = MyRamData[cnt];
            }
            average = sum/200 ;
            end_flag = 0;
        }
        if(min_flag)
        {
             uint8_t nr = min;
             uint8_t tmp;
             static volatile uint8_t digits[2] = {0};

             for (tmp = 0; tmp < 2; tmp++)
             {
                  digits[tmp] = nr % 16;
                  nr /= 16;
             }

                  disp2 = digits[0];   // cifra jedinica
                  disp1 = digits[1];   //cifra desetica
                  min_flag = 0;
        }
        if(max_flag)
        {
            uint8_t nr = max;
            uint8_t tmp;
            static volatile uint8_t digits[2] = {0};

            for (tmp = 0; tmp < 2; tmp++)
            {
                  digits[tmp] = nr % 16;
                  nr /= 16;
            }
            disp2 = digits[0];   // cifra jedinica
            disp1 = digits[1];   //cifra desetica
            max_flag = 0;
        }

        if(average_flag)
        {
             uint8_t nr = average;
             uint8_t tmp;
             static volatile uint8_t digits[2] = {0};

             for (tmp = 0; tmp < 2; tmp++)
             {
                  digits[tmp] = nr % 16;
                  nr /= 16;
             }
             disp2 = digits[0];   // cifra jedinica
             disp1 = digits[1];   //cifra desetica
             average_flag = 0;
       }

    }

}

void __attribute__ ((interrupt(PORT1_VECTOR))) P1ISR (void)
{
    if ((P1IFG & BIT4) != 0)        // check if P1.4 flag is set
    {
        /* start timer for debouncing */
        TA1CTL |= MC__UP;

        P1IFG &= ~BIT4;             // clear P1.4 flag
        P1IE &= ~BIT4;              // disable P1.4 interrupt
    }

    if ((P1IFG & BIT5) != 0)        // check if P1.4 flag is set
       {
           /* start timer for debouncing */
        TA1CTL |= MC__UP;

        P1IFG &= ~BIT5;             // clear P1.5 flag
        P1IE &= ~BIT5;              // disable P1.5 interrupt
       }
    if ((P1IFG & BIT1) != 0)        // check if P1.1 flag is set
        {
            /* start timer for debouncing */
        TA1CTL |= MC__UP;

        P1IFG &= ~BIT1;             // clear P1.1 flag
        P1IE &= ~BIT1;              // disable P1.1 interrupt
        }
}
void __attribute__ ((interrupt(PORT2_VECTOR))) P2ISR (void)
{
    if ((P2IFG & BIT1) != 0)        // check if P2.1 flag is set
    {
        /* start timer for debouncing */
        TA1CTL |= MC__UP;

        P2IFG &= ~BIT1;             // clear P2.1 flag
        P2IE &= ~BIT1;              // disable P2.1 interrupt
    }
}
void __attribute__ ((interrupt(TIMER1_A0_VECTOR))) CCR0ISR (void)
{
    if ((P1IN & BIT4) == 0) // check if button is still pressed
    {
        conversion_flag = 1;  // indicator to start timer TA0 which triggers ADC12 --> conversion starts
    }
    if ((P1IN & BIT5) == 0) // check if button is still pressed
    {
        min_flag = 1;
    }

    if ((P1IN & BIT1) == 0) // check if button is still pressed
    {
        max_flag = 1;
    }

    if ((P2IN & BIT1) == 0) // check if button is still pressed
    {
        average_flag = 1;
    }
    TA1CTL &= ~(MC0 | MC1); // stop and clear timer
    TA1CTL |= TACLR;

    P1IFG &= ~BIT4;         // clear P1.4 flag
    P1IE |= BIT4;           // enable P1.4 interrupt

    P1IFG &= ~BIT5;         // clear P1.5 flag
    P1IE |= BIT5;           // enable P1.5 interrupt

    P1IFG &= ~BIT1;         // clear P1.1 flag
    P1IE |= BIT1;           // enable P1.1 interrupt

    P2IFG &= ~BIT1;         // clear P2.1 flag
    P2IE |= BIT1;           // enable P2.1 interrupt
    return;
}
void __attribute__ ((interrupt(ADC12_VECTOR))) ADC12ISR (void)
{
    switch (ADC12IV)
    {
    case ADC12IV_ADC12IFG0:

       P2OUT |= BIT4;             // set P2.4 to '1' - led on
       data = ADC12MEM0;

       MyRamData[counter] = data;
       sum = sum + data;
       counter ++;
       if(counter == 0xC8)
           {
               end_flag = 1;
           }
        break;
    default:
        break;
    }
}
