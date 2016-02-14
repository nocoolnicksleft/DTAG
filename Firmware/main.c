


#include <16F877.h>
// #device ICD=TRUE
#device WRITE_EEPROM=ASYNC
#fuses HS,NOWDT,NOLVP,NOPROTECT,NOBROWNOUT

#use delay(clock=20000000)

#use fast_io(A)
#use fast_io(B)

#use rs232(stream=terminal, baud=57600, xmit=PIN_C6, rcv=PIN_C7, ERRORS)

#include <string.h>
#include <stdlib.h>

#define LED1 PIN_A0
#define LED2 PIN_A1

#define SW1 PIN_A2
#define SW2 PIN_A3
#define SW3 PIN_A4

#define BLANK PIN_B5
#define CLOCK PIN_B2
#define DATA PIN_B1
#define STROBE PIN_B4

#define TEXTBUFFER_LENGTH 30
#define TEXTBUFFER_LAST 29


/**********************************************************
/ Serial Input Buffer
/**********************************************************/

#define SERIAL_BUFFER_LENGTH 32
char serialbuffer[SERIAL_BUFFER_LENGTH];
unsigned int8 serialbufferptr = 0;
int1 serialreceived = 0;
int1 doecho = 1;
   
#INT_RDA
void serial_isr() {
   BYTE c;
   c = toupper(fgetc(terminal));
   if (doecho) fputc(c,terminal);
   if (!serialreceived) {
    if (c == 13)  {
     serialbuffer[serialbufferptr] = 0;
     serialreceived = 1;
     serialbufferptr = 0;
    } else {
     serialbuffer[serialbufferptr] = c;
     if (serialbufferptr < SERIAL_BUFFER_LENGTH) serialbufferptr++;
    }
   }
}

char ackdata[30] = "";

void command_ack(int1 error)
{
  if (error) {
   fprintf(terminal,"ER %S\n",ackdata);
  } else {
   fprintf(terminal,"OK %S\n",ackdata);
  }
  ackdata[0] = 0;
}

/**********************************************************
/ Config / EEPROM
/**********************************************************/

#define MYMAGICNUMBER 49

#define EFFECT_NONE 0
#define EFFECT_ROLL_RIGHT 1
#define EFFECT_ROLL_LEFT 2
#define EFFECT_WALK_RIGHT 3
#define EFFECT_WALK_LEFT 4

struct config_struct {
 unsigned int8 magicnumber;
 unsigned int1 state_led1;
 unsigned int1 state_led2;
 unsigned int1 autoscroll;
 unsigned int8 active_storage;
 unsigned int8 scroll_delay;
 unsigned int8 effect;
 char textstorage[3][TEXTBUFFER_LENGTH];
} config;

void write_eeprom_block(unsigned int16 address, unsigned int8 size, unsigned int8 *ptr)
{
  unsigned int8 i;
  output_high(BLANK);
  for (i=0;i<size;i++) {
   write_eeprom(address+i,*(ptr+i));
//   fprintf(terminal,"WR %u to %lu\n\r",*(ptr+i), address+i);
  }  
  output_low(BLANK);
}

void read_eeprom_block(unsigned int16 address, unsigned int8 size, unsigned int8 *ptr)
{
  unsigned int8 i;
  for (i=0;i<size;i++) {
   *(ptr+i) = read_eeprom(address+i);
//   fprintf(terminal,"RD %u from %lu\n\r",*(ptr+i), address+i);
  }  
}

void read_config()
{
  read_eeprom_block(1,sizeof(config_struct),&config);
  if (config.magicnumber != MYMAGICNUMBER) {
   //fprintf(terminal,"Init config, size %u\n\r",sizeof(config_struct));
   memset(&config,0,sizeof(config_struct));
   config.magicnumber = MYMAGICNUMBER;
   config.state_led1 = 0;
   config.state_led2 = 0;
   config.autoscroll = 1;
   config.active_storage = 0;
   config.scroll_delay = 13;
   config.effect = EFFECT_NONE;
   strcpy(config.textstorage[0],"DICKTIER");
   strcpy(config.textstorage[1],"HARALD DAS LUSTIGE ERDFERKEL");
   strcpy(config.textstorage[2],"DINGDONG 2000");
   write_eeprom_block(1,sizeof(config_struct),&config);
  }
}

void write_config()
{
   write_eeprom_block(1,sizeof(config_struct),&config);
}



/**********************************************************
/ VFD
/**********************************************************/

#define GRIDS 11
#define ANODES 17

unsigned int16 vfd_bitbuffer[GRIDS];

// 11G
#define PLAY_FWD 0x1 
#define PLAY_REV 0x2 
//10G
#define DOLBY 0x1
//9G
#define DOLBY_C 0x1
#define DOLBY_B 0x2
//7G
#define S11 0x1
#define S12 0x2
#define CO 0x1
//6G
#define S9 0x1
#define S10 0x2
//5G
#define S7 0x1
#define S8 0x2
//4G
#define S5 0x1
#define S6 0x2
//3G
#define S3 0x1
#define S4 0x2
//2G
#define S1 0x1
#define S2 0x2
//1G
#define DCC P11
#define AUTOREVERSE_FWD P12
#define AUTOREVERSE_REV P13
#define AUTOREVERSE_BRK_OPEN P14
#define AUTOREVERSE_BRK_CLOSE P15
#define ANALOG P16
#define DIGITAL 0x1

#define P3  (int16)0b0000000000000001
#define P4  (int16)0b0000000000000010
#define P5  (int16)0b0000000000000100
#define P6  (int16)0b0000000000001000
#define P7  (int16)0b0000000000010000
#define P8  (int16)0b0000000000100000
#define P9  (int16)0b0000000001000000
#define P10 (int16)0b0000000010000000
#define P11 (int16)0b0000000100000000
#define P12 (int16)0b0000001000000000
#define P13 (int16)0b0000010000000000
#define P14 (int16)0b0000100000000000
#define P15 (int16)0b0001000000000000
#define P16 (int16)0b0010000000000000
#define P17 (int16)0b0100000000000000

#define SEG_a P3
#define SEG_b P4
#define SEG_c P14
#define SEG_d P16
#define SEG_e P15
#define SEG_f P5
#define SEG_g P10
#define SEG_h P8
#define SEG_j P7
#define SEG_k P6
#define SEG_m P9
#define SEG_n P11
#define SEG_p P12
#define SEG_r P13

int16 encode_ascii(char code)
{ 
  switch (code)  {
    case '0': return (int16)(SEG_a | SEG_b | SEG_c | SEG_d | SEG_e | SEG_f | SEG_k | SEG_r);
    case '1': return (int16)(SEG_b | SEG_c);
    case '2': return (int16)(SEG_a | SEG_b | SEG_d | SEG_e | SEG_g | SEG_m);
    case '3': return (int16)(SEG_a | SEG_b | SEG_c | SEG_d | SEG_m);
    case '4': return (int16)(SEG_b | SEG_c | SEG_f | SEG_g | SEG_m);
    case '5': return (int16)(SEG_a | SEG_c | SEG_d | SEG_f | SEG_g | SEG_m);
    case '6': return (int16)(SEG_a | SEG_c | SEG_d | SEG_e | SEG_f | SEG_g | SEG_m);
    case '7': return (int16)(SEG_a | SEG_b | SEG_c);
    case '8': return (int16)(SEG_a | SEG_b | SEG_c | SEG_d | SEG_e | SEG_f | SEG_g | SEG_m);
    case '9': return (int16)(SEG_a | SEG_b | SEG_c | SEG_d | SEG_f | SEG_g | SEG_m);
    case 'A': return (int16)(SEG_a | SEG_b | SEG_c | SEG_e | SEG_f | SEG_g | SEG_m);
    case 'B': return (int16)(SEG_a | SEG_b | SEG_c | SEG_d | SEG_j | SEG_p | SEG_m);
    case 'C': return (int16)(SEG_a | SEG_d | SEG_e | SEG_f);
    case 'D': return (int16)(SEG_a | SEG_b | SEG_c | SEG_d | SEG_j | SEG_p);
    case 'E': return (int16)(SEG_a | SEG_d | SEG_e | SEG_f | SEG_g | SEG_m);
    case 'F': return (int16)(SEG_a | SEG_e | SEG_f | SEG_g);
    case 'G': return (int16)(SEG_a | SEG_c | SEG_d | SEG_e | SEG_f | SEG_m);
    case 'H': return (int16)(SEG_b | SEG_c | SEG_e | SEG_f | SEG_g | SEG_m);
    case 'I': return (int16)(SEG_j | SEG_p);
    case 'J': return (int16)(SEG_b | SEG_c | SEG_d | SEG_e);
    case 'K': return (int16)(SEG_e | SEG_f | SEG_g | SEG_k | SEG_n);
    case 'L': return (int16)(SEG_d | SEG_e | SEG_f);
    case 'M': return (int16)(SEG_b | SEG_c | SEG_e | SEG_f | SEG_h | SEG_k);
    case 'N': return (int16)(SEG_b | SEG_c | SEG_e | SEG_f | SEG_h | SEG_n);
    case 'O': return (int16)(SEG_a | SEG_b | SEG_c | SEG_d | SEG_e | SEG_f);
    case 'P': return (int16)(SEG_a | SEG_b | SEG_e | SEG_f | SEG_g | SEG_m);
    case 'Q': return (int16)(SEG_a | SEG_b | SEG_c | SEG_d | SEG_e | SEG_f | SEG_n);
    case 'R': return (int16)(SEG_a | SEG_b | SEG_e | SEG_f | SEG_g | SEG_m | SEG_n);
    case 'S': return (int16)(SEG_a | SEG_c | SEG_d | SEG_f | SEG_g | SEG_m);
    case 'T': return (int16)(SEG_a | SEG_j | SEG_p);
    case 'U': return (int16)(SEG_b | SEG_c | SEG_d | SEG_e | SEG_f);
    case 'V': return (int16)(SEG_e | SEG_f | SEG_k | SEG_r);
    case 'W': return (int16)(SEG_b | SEG_c | SEG_e | SEG_f | SEG_n | SEG_r);
    case 'X': return (int16)(SEG_h | SEG_k | SEG_n | SEG_r);
    case 'Y': return (int16)(SEG_h | SEG_k | SEG_p);
    case 'Z': return (int16)(SEG_a | SEG_d | SEG_k | SEG_r);
    case '+': return (int16)(SEG_g | SEG_j | SEG_m | SEG_p);
    case '-': return (int16)(SEG_g | SEG_m);
    case '*': return (int16)(SEG_g | SEG_h | SEG_j | SEG_k | SEG_m | SEG_n | SEG_p | SEG_r);
    case '_': return (int16)(SEG_d);
    case '/': return (int16)(SEG_k | SEG_r);
    case ',': return (int16)(SEG_r);
    case '\'': return (int16)(SEG_j);
    case '$': return (int16)(SEG_a | SEG_c | SEG_d | SEG_f | SEG_g | SEG_m | SEG_j | SEG_p);
  }
  return 0x0;
}

void vfd_pushstring(char *str[])
{
  int i;
  int1 end = 0;
  for (i = 0; i < 10; i++) {
    if (str[i] == 0) end = 1;
    if (end)  vfd_bitbuffer[10-i] = 0;
    else vfd_bitbuffer[10-i] = encode_ascii(str[i]);
  }
}

void vfd_scroll(int1 right,char nextchar)
{
 int i;
 for (i = 10; i > 1; i--) {
  vfd_bitbuffer[i] = vfd_bitbuffer[i-1];
 }
 vfd_bitbuffer[1] = encode_ascii(nextchar);
}

void vfd_shift_in(int1 parm)
{
  output_bit(DATA,parm);
  output_high(CLOCK);
  output_low(CLOCK);
}

void vfd_strobe(void)
{
  output_high(STROBE);
  output_low(STROBE);
}


void vfd_output()
{
  static unsigned int8 vfd_gate = 0;
  unsigned int8 i;
  unsigned int16 buf;

  for (i = 0; i < 11; i++) 
  {
    if ( i == vfd_gate) vfd_shift_in(1);
    else vfd_shift_in(0);
  }

  // P1
  vfd_shift_in(0);
  // P2
  vfd_shift_in(0);

  buf = vfd_bitbuffer[vfd_gate];

  // CHARACTER BITS P3 -> P16
  for (i = 0; i < 14; i++) 
  {
    vfd_shift_in( buf & 0x1 );
    buf = buf >> 1;
  }

  // P17
  vfd_shift_in(0);

  // LAST 2 BITS UNUSED
  vfd_shift_in(0);
  vfd_shift_in(0);

  vfd_strobe();

  if (vfd_gate == 10) vfd_gate = 0;
  else  vfd_gate++;
	
}

/**********************************************************
/ FUNCTIONS
/**********************************************************/
unsigned int1 overflow = 0;
unsigned int8 scrollcounter = 0;
unsigned int1 scrollnow = 0;
unsigned int8 scrollptr = 0;
unsigned int8 keystate[3] = {0,0,0};

#int_timer0
void RTCC_ISR()
{
  vfd_output();
  set_timer0(0);
}

#int_timer1
void TIMER1_ISR()
{
  if (--scrollcounter == 0) {
   scrollnow = 1;
   scrollcounter = config.scroll_delay;
  }
}


void output_leds()
{
  output_bit(LED1,config.state_led1);
  output_bit(LED2,config.state_led2);
}

void display_current_buffer()
{
  if (strlen(config.textstorage[config.active_storage]) > 10) overflow = 1;
  else overflow = 0;
  scrollcounter = config.scroll_delay;
  scrollptr = 10;
  vfd_pushstring(config.textstorage[config.active_storage]);
}

int1 checkrange(unsigned char val, unsigned char maxval)
{
  if ( ((val - '0') >= 0) && ( ((val - '0') <= (maxval - '0')) )) return 0;
  return 1;
}

/**********************************************************
/ MAIN
/**********************************************************/

void main(void)
{
 // char cchar = 0;

  int8 command;
  int8 subcommand;

  set_tris_a(0b00011100);
  set_tris_b(0b00000000);

  output_a(0xb00000011);

  output_high(BLANK);

  setup_timer_0(RTCC_INTERNAL | RTCC_DIV_32);
  setup_timer_1(T1_INTERNAL | T1_DIV_BY_1);

  enable_interrupts(INT_TIMER0);
  enable_interrupts(INT_TIMER1);
  enable_interrupts(INT_RDA);
  enable_interrupts(GLOBAL);

  read_config();

  output_leds();

  display_current_buffer();

  vfd_bitbuffer[0] = 0;

  output_low(BLANK);

  for (;;) 
  {

   if (!input(SW1)) {
    fprintf(terminal,"OKT1\n");
    config.active_storage = 0;
    write_config();     
    display_current_buffer();
   }

   if (!input(SW2)) {
    fprintf(terminal,"OKT2\n");
    config.active_storage = 1;
    write_config();     
    display_current_buffer();
   }

   if (!input(SW3)) {
    fprintf(terminal,"OKT3\n");
    config.active_storage = 2;
    write_config();     
    display_current_buffer();
   }

   if (serialreceived)
   {
     serialreceived = 0;
     command = (serialbuffer[0]);
     subcommand = (serialbuffer[1] - '0');

     if (command == 'L') { // LXY X=0=A0 X=1=A1 Y=0=AUS Y=1=EIN
      if (checkrange(serialbuffer[1],'2') || checkrange(serialbuffer[2],'1')) command_ack(1);
      else {
        if (serialbuffer[1] == '1') config.state_led1 = (serialbuffer[2]-'0');
        else if (serialbuffer[1] == '2') config.state_led2 = (serialbuffer[2]-'0');
        sprintf(ackdata,"L%u%c",subcommand,serialbuffer[2]);
        output_leds();
        write_config();     
        command_ack(0);
       }
     } else if (command == 'T') { // MAXIMAL 10 ZEICHEN DIREKT ANZEIGEN
       vfd_pushstring(serialbuffer+1);
       serialbuffer[11] = 0;
       sprintf(ackdata,"T%s",serialbuffer+1);
       overflow = 0;
       command_ack(0);
     } else if (command == 'Z') { // OXXY XX=ZEICHEN-ID Y=0=AUS Y=1=EIN
       command_ack(1);

     } else if (command == 'M') { // MAXIMAL 30 ZEICHEN IN GEWÄHLTEN FLASH-SPEICHER ABLEGEN
       if (subcommand < 3) {
        if (serialbuffer[2] != 0) {
         strncpy(config.textstorage[subcommand],serialbuffer+2,TEXTBUFFER_LAST);
         write_config();     
         if (subcommand == config.active_storage) display_current_buffer();
        }
        sprintf(ackdata,"M%u%s",subcommand,config.textstorage[subcommand]);
        command_ack(0);
       } else command_ack(1);
     } else if (command == 'R') { // FULL RESET
       config.magicnumber = 0;
       write_config();
       command_ack(0);
       reset_cpu();
     } else if (command == 'S') { // SX 0=SPEICHERTEXT 0 (DEFAULT) 
                                          //    1=SPEICHERTEXT 1
                                          //    2=SPEICHERTEXT 2
                                          //    3=AUS/BLANK
       if (subcommand < 5) {
        sprintf(ackdata,"S%u",subcommand);
        if (subcommand < 3) {
         config.active_storage = subcommand;
         write_config();
         display_current_buffer();
         command_ack(0);
       } else if (subcommand == 3) {
         output_high(BLANK);
         command_ack(0);
        }
       } else command_ack(1);

     } else if (command == 'O') { // OXYY X=OPTION YY=WERT
                                          // OPTION 1 AUTOSCROLL WERT 00/01 AUS/EIN
                                          // OPTION 2 SCROLLDELAY WERT 01-99 (MS*10)
                                          // OPTION 3 EFFECT 00-04
                                          // OPTION 4 ECHO 00/01 AUS/EIN
                                          
       if (subcommand < 5) {
        if (subcommand == 1) {
         if (serialbuffer[3] == '0') config.autoscroll = 0;
         else if (serialbuffer[3] == '1') config.autoscroll = 1;
        } else if (subcommand == 2) {
         config.scroll_delay = atoi(serialbuffer+2);
        } else if (subcommand == 3) {
         config.effect = atoi(serialbuffer+2);
        } else if (subcommand == 4) {
         if (serialbuffer[3] == '0') doecho  = 0;
         else if (serialbuffer[3] == '1') doecho = 1;
        }
        sprintf(ackdata,"O%u%c%c",subcommand,serialbuffer[2],serialbuffer[3]);
        write_config();
        command_ack(0);
       } else command_ack(1);
     } else if (command == 'P') { // PX SHIFT IN CHARACTER X
         vfd_scroll(config.effect,serialbuffer[1]);
         command_ack(0);
     } else if (command == 'V') { // PRINT VERSION
      strcpy(ackdata,"DICKTIERANZEIGEGERAET V1.0");
      command_ack(0);

     } 
     else 
      command_ack(1);  // KOMMANDO UNBEKANNT

   }

   if (scrollnow) 
   {
     scrollnow = 0;
     if (overflow && config.autoscroll) 
     {
       vfd_scroll(config.effect,config.textstorage[config.active_storage][scrollptr]);
       if (scrollptr == strlen(config.textstorage[config.active_storage])) scrollptr = 0;
       else scrollptr++;
     }
   }


/*

   vfd_pushbuffer(cchar);
   if (cchar == TEXTLENGTH -10 + 2) cchar = 0;
   else cchar++;
*/

  }
}
