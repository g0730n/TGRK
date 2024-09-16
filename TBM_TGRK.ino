#include <avr/sleep.h>
#include <avr/wdt.h>
#include <EEPROM.h>

//MENU MODE MACROS
#define MENU_MODE 0  //main menu mode
#define BYTE_MODE 1  //byte mode, step through program bytes
#define BIT_MODE 2   //bit mode, for editing a byte
#define RUN_MODE 3   //run mode (3 options)
#define LOAD_MODE 4  //load program from eeprom
#define SAVE_MODE 5  //save program to eeprom

//FLAG MACROS FOR FLAG BITARRAY [f]
#define DEL_FLAG 1
#define BTN_FLAG 2
#define LS0_FLAG 3
#define LS1_FLAG 4
#define INT_FLAG 5

//DEEPSLEEP MODES
#define DS_WDT 0
#define DS_WDT_B_INTERRUPT 1
#define DS_NO_WDT_B_INTERRUPT 2

//BUTTON MACROS
#define B_RUN 5
#define B_SAVE 4
#define B_TOGGLE 3
#define B_RIGHT 2
#define B_LEFT 1

//LED MACROS
#define LED0 0
#define LED1 1
#define UPIN 2
#define MAX 127

//INTERPRETER KEYWORD MACROS
#define EOP -128  //end of program code
#define SEP -1    //seperator code
#define FUNC -2

//MATH MACROS
#define ADD -3
#define SUB -4
#define MUL -5
#define DIV -6
#define AND -7
#define OR -8
#define RND -9

//CONDITIONAL MACROS
#define CG -11
#define CL -12
#define CE -13
#define CNE -14
#define CGE -15
#define CLE -16

//LOOP MACRO
#define LP -96

//LOOP REGISTER MACROS
#define LR1 -95
#define LR2 -94
#define LR3 -93
#define LR4 -92
#define LR5 -91
#define LR6 -90
#define LR7 -89
#define LR8 -88

//LOOP FUNCTION MACRO
#define LPF -87

//REGISTER MACROS
#define R1 -127
#define R2 -126
#define R3 -125
#define R4 -124
#define R5 -123
#define R6 -122
#define R7 -121
#define R8 -120

//EEPROM STORAGE MACROS
#define ER -119
#define EW -118

//FUNCTION MACROS
#define F1 -63
#define F2 -62
#define F3 -61
#define F4 -60
#define F5 -59
#define F6 -58
#define F7 -57
#define F8 -56

//PIN OUTPUT MACROS
#define PDOH -25
#define PDOL -26

//PIN IN MACROS
#define PDIU -27
#define PDID -28

//PIN ADC MACROS
#define PAIX -29
#define PAXX -30

//LED MACROS
#define L0L -48
#define L0H -47
#define L1L -46
#define L1H -45

//SLEEP MACROS
#define S1 -111
#define S2 -110
#define S3 -109
#define S4 -108
#define S5 -107
#define S6 -106
#define S7 -105
#define S8 -104

//DEEPSLEEP MACROS
#define D0 -80
#define D1 -79
#define D2 -78
#define D3 -77
#define D4 -76
#define D5 -75
#define D6 -74
#define D7 -73
#define D8 -72

//BUTTON INPUT MACROS
#define BG -44
#define BV -43

//BLINK BYTE MACRO
#define BB -33

//variable for storing button press
uint8_t btn = 0;  //(0-5)

//variable for storing interpreter button press
int8_t br = 0;

//MAIN PROGRAM ARRAY
int8_t prog[MAX];
//general purpose registers for storing integers 0-127
uint8_t r[8];  //(0-7)
//function registers for storing function addresses 0-127
uint8_t f[8];  //(0-7)
//function return addresses 0-127
uint8_t ret_addr[8];  //(0-7)


//Timing Variables
const uint8_t buttonInterval = 100;   //button check interval
const uint16_t vccAdcInterval = 2000;  //VCC check interval

unsigned long delayInterval = 0;
unsigned long nowTime = millis();
unsigned long delayTime = 0;
unsigned long curButtonTime = 0;
unsigned long curVccAdcTime = 0;
unsigned long bgTime = 0;

//function return position ret_addr[ret_pos] (ret_pos = 0-7)
uint8_t ret_pos = 0;
//current register addr r[reg] (reg = 0-7)
uint8_t reg = 0;
//temporary register to store data (temp = 0-127, -1 means it has been reset)
int8_t temp = -1;

//position in program prog[0-127]
int8_t pos = 0;
//bit position in byte
int8_t bpos = 0;  //(0-7)

//program mode (0-5)
uint8_t mode = 0;
//menu position for navigating options (0-3)
uint8_t mpos = 0;

//flag variable (bitarray)
int8_t flg = 0;

//pseudo random number variable
int8_t random_number = 0;

//declare vcc_check_interval()
void vcc_check_interval();  //periodically check vcc voltage
//declare check_button()
void check_button();

//GET SELECTED BIT FROM BYTE
int8_t get_bit(int8_t num, int8_t bit_s) {
  return (num >> (7 - bit_s)) & 1;
}

//FUNCTION TO GENERATE PSEUDO RANDOM NUMBERS 0-127
void set_random(int8_t time_seed){
  random_number <<= 1;
  random_number ^= get_bit(time_seed,7);
  random_number &= 127;
}

//TOGGLE SELECTED BIT IN BYTE
void toggle_bit(int8_t x) {
  prog[pos] ^= (1 << (7 - x));
}

//SET FLAG BIT
void toggle_flag_bit(int8_t flag) {
  flg ^= (1 << (7 - flag));
}

//WATCHDOG INTERRUPT
ISR(WDT_vect) {
  wdt_disable();
}  // disable watchdog

//PCINT3 INTERRUPT
ISR(PCINT0_vect) {
  //if PB3 is LOW
  if (digitalRead(3) == LOW) {
    //set the interrupt flag
    toggle_flag_bit(INT_FLAG);
    //clear the interrupt from pin 3
    PCMSK &= ~_BV(PCINT3);
    if(!get_bit(flg, BTN_FLAG)){ //if button pressed flag isn't set,
      toggle_flag_bit(BTN_FLAG); //set button pressed flag so we don't kill running interpreter
    }
  }
}

//FUNCTION TO SET ALL PINS INPUT/LOW FOR POWER SAVING
void pins_down(uint8_t sleep_mode) {
  //set all gpio as INPUT
  for (uint8_t i = 0; i <= 5; i++) {
    pinMode(i, INPUT);
    //if we aren't enabling RUN button interrupt
    //set all pins LOW, otherwise don't pulldown PB3
    if (sleep_mode != DS_WDT_B_INTERRUPT && sleep_mode != DS_NO_WDT_B_INTERRUPT && i != 3) {
      digitalWrite(i, LOW);
    }
  }
}

//DEEP SLEEP POWER SAVE FUNCTION
void long_sleep(uint8_t sleep_mode) {
  //set pins for sleep
  pins_down(sleep_mode);
  //disable ADC
  ADCSRA = 0;
  //clear reset bit
  MCUSR = 0;
  //if we are sleeping indefinitively
  //dont enable watchdog timer
  if (sleep_mode != DS_NO_WDT_B_INTERRUPT) {
    //set WDT for 1 second
    WDTCR = bit(WDCE) | bit(WDE);
    WDTCR = bit(WDIE) | bit(WDP2) | bit(WDP1);
    wdt_reset();
  }
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  noInterrupts();
  sleep_enable();

  //if we want to enable RUN button wakeup
  if (sleep_mode == DS_WDT_B_INTERRUPT || sleep_mode == DS_NO_WDT_B_INTERRUPT) {
    //Attatch interrupt to PB3
    PCMSK = (1 << PCINT3);  // set pin change mask to PCINT3
    GIMSK = (1 << PCIE);    // enable pin change interrupt
  }
  interrupts();
  sleep_cpu();
  sleep_disable();
}

void reset_led_pins() {
  pinMode(LED0, OUTPUT);
  pinMode(LED1, OUTPUT);
}

//FUNCTION TO RESET LED PINS AND ENABLE ADC AFTER DEEPSLEEP
void long_sleep_wake() {
  //set LED pins to output
  reset_led_pins();
  //enable ADC
  ADCSRA |= 1 << ADEN;
}

//LED CONTROL
void led_ctrl(int8_t led1, int8_t led0) {
  digitalWrite(LED1, led1);  //set LED1 state
  digitalWrite(LED0, led0);  //set LED0 state
  if (led0 != get_bit(flg, LS0_FLAG)) {
    toggle_flag_bit(LS0_FLAG);  //store LED0 last state
  }
  if (led1 != get_bit(flg, LS1_FLAG)) {
    toggle_flag_bit(LS1_FLAG);  //store LED1 last state
  }
}

//MENU LED CONTROL
void menu_led() {
  if (mpos == 0) {
    led_ctrl(LOW, LOW);
  } else if (mpos == 1) {
    led_ctrl(HIGH, LOW);
  } else if (mpos == 2) {
    led_ctrl(LOW, HIGH);
  } else {
    led_ctrl(HIGH, HIGH);
  }
}

void do_delay(unsigned long amount) {
  nowTime = millis();   //get current time
  delayTime = nowTime;  //set our timer
  while (nowTime - delayTime <= amount) {
    nowTime = millis();  //increment current time
  }
  delayTime = 0;
}

//TOGGLE LED
void toggle_led(int8_t led) {
  led_ctrl(LOW, LOW);
  do_delay(100);
  led_ctrl(led == 1 ? HIGH : LOW, led == 1 ? LOW : HIGH);
}

//BLINK AN LED
void blink(int8_t led) {
  toggle_led(led);
  do_delay(400);
  led_ctrl(LOW, LOW);
  do_delay(400);
}

//BLINK BOTH LED's X AMOUNT OF TIMES
void blink_both(uint8_t x) {
  for (uint8_t i = 0; i < x; i++) {
    led_ctrl(HIGH, HIGH);
    do_delay(200);
    led_ctrl(LOW, LOW);
    do_delay(200);
  }
}

//BLINK OUT THE BITS OF A BYTE
void blink_byte(int8_t num) {
  for (int8_t i = 0; i < 8; i++) { blink(get_bit(num, i)); }
}

//BLINK OUT BINARY ERROR CODE
void blink_error(uint8_t error) {
  while (1) {
    blink_byte(error);
    do_delay(3000);
  }
}

float read_voltage(uint8_t mode) {

  float raw = 0.0;
  uint8_t pin;

  if (mode == 2) {
    pinMode(UPIN, INPUT);
    analogReference(DEFAULT);
    pin = A1;
  } else {
    analogReference(INTERNAL1V1);
    pin = A2;
  }
  do_delay(10);

  for (uint8_t x = 0; x < 10; x++) {  //get 10 readings
    if (x == 0) {
      raw = analogRead(pin);
      raw = 0.0;  //discard first reading
    }
    raw += analogRead(pin);
  }

  raw /= 9;  //get average of last 9 readings

  do_delay(10);

  if (pin == A2) {
    analogReference(DEFAULT);  //reset analogReference
  }

  return raw * 5.4 / 1023.0;
}

//READ ADC3, VCC VOLTAGE
int8_t process_voltage(uint8_t mode) {

  int8_t val = (int8_t)(read_voltage(mode) * 10);  //5.4*10 = 54

  if (mode == 1) {  //if we are blinking volts
    uint8_t dec = 0;
    uint8_t rem = 0;

    if (val > 0) {
      dec = val / 10;
      if (val - dec > 0) {
        rem = (val - (dec * 10));
      }
    }
    for (int8_t i = 0; i < dec; i++) { blink(LED1); }
    for (int8_t i = 0; i < rem; i++) { blink(LED0); }
  }
  //otherwise return 2 digit voltage code (5.4v = 54, 2.4v = 24)
  return val;
}

//Put device in deepsleep if VCC is < 2v
void check_voltage() {
  while (read_voltage(0) < 2.0) {        //if its low, pwr_down until its above 2v
    for (uint8_t i = 0; i < 120; i++) {  //sleep 2 min
      long_sleep(DS_WDT);
    }
    long_sleep_wake();  //wakeup
  }
  do_delay(10);
}

//PROCESS BUTTON PRESS ADC VALUE
uint8_t button() {
  uint16_t adcv = analogRead(A3);
  uint8_t b = 0;
  if (adcv < 20) b = B_RUN;
  else if (adcv <= 570) b = B_SAVE;
  else if (adcv <= 780) b = B_TOGGLE;
  else if (adcv <= 880) b = B_RIGHT;
  else if (adcv <= 950) b = B_LEFT;
  return b;
}

//INITIALIZE LED PINS
void setup() {
  reset_led_pins();
}

//FUNCTION TO DISPLAY BYTE TYPE
void byte_mode_led(int8_t x) {
  if (prog[x] < 0) { toggle_led(1); }  //if it's a keyword LED1
  else {
    toggle_led(0);
  }
}

//CHECK IF WE HAVE REACHED PROGRAM END
uint8_t check_prog_end(int8_t i) {
  uint8_t flag = 0;
  if (prog[i] == EOP || (prog[i] == prog[i + 1] && prog[i] == prog[i + 2] && (prog[i] == 0 || prog[i] == SEP))) {
    flag = 1;
  }
  return flag;
}

//FUNCTION TO BLINK ENTIRE PROGRAM FAST
void blink_prog() {
  for (int8_t i = 0; i < MAX; i++) {  //loop through program
    byte_mode_led(i);                 //flash each byte
    do_delay(100);
    //if we read more than 3 zero's in a row, 3 ones's in a row, or EOP keyword (-128) Stop program read
    if (check_prog_end(i)) {
      break;
    }
  }
  menu_led();  //reset menu LED
}

//SWITCH TO MENU MODE, RESET LED's
void goto_menu() {
  mode = MENU_MODE;
  menu_led();
}

//BYTE MODE, VIEW PROGRAM BYTES
void byte_mode(uint8_t b) {
  if (b == B_LEFT && pos > 0) {
    pos -= 1;
    byte_mode_led(pos);
  } else if (b == B_RIGHT && pos < MAX) {
    pos += 1;
    byte_mode_led(pos);
  } else if (b == B_TOGGLE) {
    mode = BIT_MODE;
    bpos = 0;
    toggle_led(get_bit(prog[pos], bpos));
  } else if (b == B_SAVE) {
    goto_menu();  //exit to main menu
  } else if (b == B_RUN) {
    blink_byte(prog[pos]);
    toggle_led(get_bit(prog[pos], 0));
  }
}

//BIT MODE, EDIT BITS IN BYTE
void bit_mode(uint8_t b) {
  if (b == B_LEFT && bpos > 0) {
    bpos -= 1;
    toggle_led(get_bit(prog[pos], bpos));
  } else if (b == B_RIGHT && bpos < 7) {
    bpos += 1;
    toggle_led(get_bit(prog[pos], bpos));
  } else if (b == B_TOGGLE) {
    toggle_bit(bpos);
    toggle_led(get_bit(prog[pos], bpos));
  } else if (b == B_SAVE) {
    led_ctrl(LOW, LOW);
    bpos = 0;
    mode = BYTE_MODE;
    byte_mode_led(pos);
  } else if (b == B_RUN) {
    blink_byte(prog[pos]);
    do_delay(400);
    toggle_led(get_bit(prog[pos], bpos));
  }
}

//CHECK IF KEYWORD IS INPUT/ADC PIN
uint8_t is_pin(int8_t addr) {
  uint8_t flag = 0;
  if (prog[addr] >= PAXX && prog[addr] <= PDIU) { flag = 1; }
  return flag;
}

//CHECK IF IS AN EEPROM KEYWORD
uint8_t is_eeprom(int8_t addr) {
  uint8_t flag = 0;
  if (prog[addr] == EW || prog[addr] == ER) { flag = 1; }
  return flag;
}

//CHECK IF IS A REGISTER KEYWORD
uint8_t is_reg(int8_t addr) {
  uint8_t flag = 0;
  if (prog[addr] >= R1 && prog[addr] <= R8) { flag = 1; }
  return flag;
}

//CHECK IF IS A MATH OPERATOR
uint8_t is_math(int8_t addr) {
  uint8_t flag = 0;
  if (prog[addr] >= DIV && prog[addr] <= ADD) { flag = 1; }
  return flag;
}

//FUNCTION TO CHECK IF BYTE IS A NUMBER
int8_t is_num(int8_t addr) {
  uint8_t flag = 0;
  if (prog[addr] > SEP) { flag = 1; }
  return flag;
}

//FUNCTION TO GET VALUE STORED IN REGISTER
int8_t get_reg_val(int8_t addr) {
  return r[prog[addr] + 127];
}

int8_t get_pin_val(int8_t addr) {
  if (prog[addr] == PAXX) {
    return process_voltage(0);
  } else if (prog[addr] == PDIU) {
    pinMode(UPIN, INPUT_PULLUP);
    do_delay(100);
    return (int8_t)digitalRead(UPIN);
  } else if (prog[addr] == PDID) {
    pinMode(UPIN, INPUT);
    digitalWrite(UPIN, LOW);
    return (int8_t)digitalRead(UPIN);
  } else if (prog[addr] == PAIX) {
    //ANALOG IN ON PB2
    return process_voltage(2);
  } else {
    return -1;
  }
}

int8_t get_eeprom_val(int8_t addr) {
  int8_t val = 0;
  if (prog[addr] == ER) {
    uint16_t eeAddress = 385 + (prog[addr + 1]);  //set the address
    if(eeAddress < 385 || eeAddress > 502){ blink_error(addr); } //if we our outside of our address range
    val = EEPROM.read(eeAddress);         //read eeprom byte
  }
  return val;
}

//FUNCTION TO GET VALUE OF BYTE (REGISTER OR INTEGER)
int8_t get_token_value(int8_t addr) {
  int8_t num = 0;
  if (is_reg(addr)) {         //check if is a register
    num = get_reg_val(addr);  //load from reg
  } else if (is_num(addr)) {  //check if num
    num = prog[addr];
  } else if (is_pin(addr)) {
    num = get_pin_val(addr);
  } else if (prog[addr] == ER) {
    num = get_eeprom_val(addr);
  } else if (prog[addr] == RND) { //check if random number
    set_random(millis());
    num = random_number;
  } else if (prog[addr] == BV){
    num = br;
  }
  return num;  //return the number
}

//FUNCTION TO PARSE CONDITIONAL CHECKS
int8_t check_con(int8_t c, int8_t num1, int8_t num2) {
  int8_t flag = 0;                           //set flag to false
  if (c == CG && num1 > num2) { flag = 1; }  //CG n1 n2
  else if (c == CL && num1 < num2) { flag = 1; } //CL n1 n2
  else if (c == CE && num1 == num2) { flag = 1; }  //CE n1 n2
  else if (c == CNE && num1 != num2) { flag = 1; } //CN n1 n2
  else if (c == CGE && num1 >= num2) { flag = 1; }  //CGE n1 n2
  else if (c == CLE && num1 <= num2) { flag = 1; }  //CLE n1 n2
  return flag;
}

int8_t do_condition(int8_t x) {
  //if conditional is true evaluate expression
  // check_con(C,n1,n2)
  if ( is_eeprom(prog[x + 1]) || is_eeprom(prog[x + 2])){
    blink_error(x); //We cant check conditions on EEPROM addresses (maybe someday :)
  }
  if (check_con(prog[x], get_token_value(x + 1), get_token_value(x + 2))) {
    x += 3;  //skip compared numbers
    //check if there is another condition
    //if it's an AND ( && ), this condition was true, we will let
    //parser check for next condition
    //if its an OR ( || ) or AND then we just skip keyword, and next condition will decide outcome
    if (prog[x] == AND || prog[x] == OR) { x++; }
  } else {
    //if first conditional not true, check for second expression and or skip
    x += 3;
    if (prog[x] == OR) {  //if symbol is an OR and first condition was not true
      //skip the OR, parser will check next condition
      x++;
    } else {  //otherwise skip the expression
      while (prog[x] != SEP && prog[x] != FUNC && x < MAX) { x++; }
    }
  }
  return x;
}

//FUNCTION TO DO MATH ON NUMBERS
int8_t do_math(int8_t regv, int8_t opaddr, int8_t numaddr) {
  int16_t val = 0;
  if (prog[opaddr] == ADD) { val = regv + numaddr; }  //do addition
  else if (prog[opaddr] == SUB) {
    val = regv - numaddr;
  }                                                        //do subtraction
  else if (prog[opaddr] == MUL) { val = regv * numaddr; }  //do multiplication
  else if (prog[opaddr] == DIV) {                          //do division
    if (regv == 0) { val = regv; }                         //if try to divide by zero, set result to 0
    else {
      val = regv / numaddr;
    }  //otherwize, do division
  }
  //if our number is less than zero, set it to zero
  if (val < 0) { val = 0; }
  //if our number is greater than int8_t MAX, set it to 127
  else if (val > 126) {
    val = 127;
  }

  return (int8_t)val;
}

//PROCESS SAVING EEPROM BYTES
int8_t do_eeprom_save(int8_t x) {
  x++; //skip the ES keyword
  uint16_t eeAddress = 385 + (prog[x]);  //set the address

  if(eeAddress < 385 || eeAddress > 502){ blink_error(x); } //if we our outside of our address range

  int8_t val = EEPROM.read(eeAddress);         //read eeprom byte
  x++; //skip the address

  if (is_math(x)) {  // EW + 21 / 3
    //do the math. Check if there are more operations

    if (val < 0) { val = 0; }  //if for some reason it's negative set to zero

    while (is_math(x)) {  //check for multiple math operations
      val = do_math(val, x, get_token_value(x + 1));
      x += 2;  //skip the operator and number
    }
    EEPROM.update(eeAddress, val);
  } else {
    //otherwise we are assigning value to this eeprom address register
    val = get_token_value(x);  //store byte
    x++;
    if (is_math(x)) {       // EW 21*12/4
      while (is_math(x)) {  //check for multiple math operations
        val = do_math(val, x, get_token_value(x + 1));
        x += 2;  //skip the operator and number
      }
    } else {
      EEPROM.update(eeAddress, val);  //save the byte
    }
  }

  return x;
}

//PROCESS REGISTER
int8_t do_register(int8_t x) {
  //reg = -124 - (-127 - 1)
  reg = prog[x] - R1;  //load the register address
  x++;
  if (is_math(x)) {       //if we are doing math
    while (is_math(x)) {  //check for multiple math operations
      r[reg] = do_math(r[reg], x, get_token_value(x + 1));
      if ( is_eeprom(prog[ x + 1])){
        x += 3; // if eeprom: skip operator, EW, and address
      }
      else{
        x += 2;  //skip the operator and number
      }
    }
  } else {
    //otherwise we are assigning value to this register
    r[reg] = get_token_value(x);
    x++;                    //skip the register or number
    if (is_math(x)) {       // check if math
      while (is_math(x)) {  //check for multiple math operations
        r[reg] = do_math(r[reg], x, get_token_value(x + 1));
        if ( is_eeprom(prog[ x + 1])){
          x += 3; // if eeprom: skip operator, EW, and address
        }
        else{
          x += 2;  //skip the operator and number
        }
      }
    }
  }
  return x;
}

//PROCESS SYMBOL
int8_t do_symbol(int8_t x) {
  if (prog[x] == SEP) { x++; }          //its a seperator, skip it
  else if (prog[x] == FUNC) {           //its a function block
    if (ret_pos > 0) { ret_pos -= 1; }  //decrement return position
    x = ret_addr[ret_pos];              //return to return address
  } else {
    blink_error(x);
  }
  return x;
}

//PROCESS FUNCTION
int8_t do_function(int8_t x) {
  int8_t f_addr = prog[x] - F1 + 1;  //get function address
  if (prog[x + 1] == FUNC) {         //check if next symbol is a block symbol
    f[f_addr] = x + 2;               //if it is, save the function begin address
    x += 2;
    //skip the function block
    while (prog[x] != FUNC) { x++; }
    x++;                        //skip the tailing #
  } else {                      //otherwise we are jumping to a function
    ret_addr[ret_pos] = x + 1;  //save return address
    //if we haven't reached max function nests
    if (ret_pos < 7) {
      ret_pos++;      //increment return index
      x = f[f_addr];  //jump to function address
    } else {
      blink_error(ret_pos);
    }  //otherwise raise error condition
  }
  return x;
}

//PROCESS SLEEP KEYWORD
int8_t do_sleep(int8_t x) {
  nowTime = millis();
  if (!get_bit(flg, DEL_FLAG)) {
    delayTime = nowTime;
    toggle_flag_bit(DEL_FLAG);
    delayInterval = (prog[x] - S1 + 1) * 1000;
  } else if (nowTime - delayTime >= delayInterval && get_bit(flg, DEL_FLAG)) {
    delayTime = nowTime;
    toggle_flag_bit(DEL_FLAG);
    x++;
  }
  return x;
}

//PROCESS LOOP KEYWORDS
int8_t do_loop(int8_t x) {
  if (prog[x] == LP) {  //LOOP function or expression block
    while (prog[x] != FUNC && prog[x] != SEP && x > SEP) { x--; }
  } 
  else if (prog[x] == LPF) { // LOOP FUNCTION ONLY
    while (prog[x] != FUNC && x > SEP) { x--; }
  } else {  //loop x amount of rx times
    //if loop is starting, set temp register to selected register value
    if (temp < 0) { temp = r[prog[x] - LR1]; }
    temp--;  //decrement temp value
    //if we are still counting
    if (temp > 0) {
      while (prog[x] != SEP && prog[x] != FUNC && x > SEP) { x--; }
    } else {       //otherwise reset temp and move to next token
      temp = SEP;  //reset temp
      x++;         //next token
    }
  }
  x++;
  return x;
}

int8_t do_deepsleep(int8_t x) {
  int16_t sleep_time = (prog[x] - D0) * 60;  //sleep keyword 1-8s*60 = 1-8min
  if (sleep_time == 0) {                     //if keyword is D0 (-80)
    long_sleep(DS_NO_WDT_B_INTERRUPT);       //sleep forever
    long_sleep_wake();                       //reset pins and ADC
  } else {
    for (int16_t i = 0; i < sleep_time; i++) {
      long_sleep(DS_WDT_B_INTERRUPT);  //go into PWR_DOWN_MODE
      long_sleep_wake();               //reset pins and ADC
      if (get_bit(flg, INT_FLAG)) {
        toggle_flag_bit(INT_FLAG);
        i = sleep_time;
      }
    }
  }
  x++;
  return x;
}

//PROCESS GPIO CONTROLS
int8_t do_gpio(int8_t x) {
  int8_t value = -1;
  if (prog[x] == PDOH) {
    pinMode(UPIN, OUTPUT);
    digitalWrite(UPIN, HIGH);
    value = HIGH;
  } else if (prog[x] == PDOL) {
    pinMode(UPIN, OUTPUT);
    digitalWrite(UPIN, LOW);
    value = LOW;
  }
  return value;
}

//PROCESS LED KEYWORDS
int8_t do_led(int8_t x) {
  if (prog[x] == L0H) {
    led_ctrl(get_bit(flg, LS1_FLAG), HIGH);
  } else if (prog[x] == L0L) {
    led_ctrl(get_bit(flg, LS1_FLAG), LOW);
  } else if (prog[x] == L1H) {
    led_ctrl(HIGH, get_bit(flg, LS0_FLAG));
  } else if (prog[x] == L1L) {
    led_ctrl(LOW, get_bit(flg, LS0_FLAG));
  }
  x++;
  return x;
}

//PROCESS BUTTONS (INTERPRETER)
int8_t do_buttons(int8_t x) {
  unsigned long timeout = 5000;
  unsigned long curTime = millis();
  bgTime = millis();
  br = 0;
  
  while (curTime - bgTime <= timeout){
    curTime = millis();
    check_button();
    if (btn > 0 && btn < B_RUN){
      br = (int8_t)btn;
      break;
    }
  }

  btn = 0;
  x++;
  return x;
}

//PARSE KEYWORDS
int8_t parse_op(int8_t x) {

  if (prog[x] >= OR && prog[x] <= SEP) {  //its a symbol
    x = do_symbol(x);
  } else if (prog[x] >= CLE && prog[x] <= CG) {  //its a CONDITIONAL
    x = do_condition(x);
  } else if (is_reg(x)) {  //its a REGISTER
    x = do_register(x);
  } else if (prog[x] >= F1 && prog[x] <= F8) {  //its a FUNCTION register
    x = do_function(x);
  } else if (prog[x] >= S1 && prog[x] <= S8) {  //its a SLEEP code
    x = do_sleep(x);
  } else if (prog[x] >= LP && prog[x] <= LPF) {  //its a loop keyword
    x = do_loop(x);
  } else if (prog[x] >= D0 && prog[x] <= D8) {  //deepsleep
    x = do_deepsleep(x);
  } else if (prog[x] == PDOH || prog[x] == PDOL) {  //pin control
    if (do_gpio(x) < 0) {
      blink_error(x);
    }
    x++;
  } else if (prog[x] >= L0L && prog[x] <= L1H) {  //LED control
    x = do_led(x);
  } else if (prog[x] == BG) {  //read buttons
    x = do_buttons(x);
  } else if (prog[x] == EW) {  //EEPROM ACCESS
    x = do_eeprom_save(x);
  } else if (prog[x] == BB) {
    x++;
    blink_byte(get_token_value(x));
    if( prog[x] == ER ){
      x++; //skip eeprom address
    }
    x++;
  }
  return x;
}

//PROGRAM INTERPRETER FUNCTION
void interpret() {
  int8_t x = 0;  //reset program read position
  ret_pos = 0;   //reset function return position
  //reset interpreter variables
  for (uint8_t i = 0; i < 8; i++){
    r[i] = 0;
    f[i] = 0;
    ret_addr[i] = 0;
  }
  led_ctrl(LOW, LOW);  //reset LED's
  do_delay(400);       //delay to prevent button interference

  //read program until we reach end
  while (x > SEP && x < MAX) {
    vcc_check_interval();  //periodically check vcc voltage
    check_button();        //check for button press
    //if RUN button pressed, stop program
    if (get_bit(flg, BTN_FLAG) && btn == B_RUN) {
      btn = 0;
      break;
    }
    //it's a keyword, parse it
    else if (prog[x] < 0) {
      if (prog[x] == EOP) break;
      x = parse_op(x);
    }
    //if program end, stop program
    else if (check_prog_end(x)) {
      break;
    }
    //if none of the above it's an error!
    else {
      if (prog[x] != EOP) { blink_error(x); }
      break;
    }
  }
  goto_menu();  //exit to main menu
}

//FUNCTION FOR LOADING AND SAVING TO EEPROM
void eeprom_ctrl(uint8_t slot, uint8_t mode) {
  uint16_t addr;        //address for saving or loading to
  blink_both(3);        //blink both LED x times
  if (slot < 3) {       //saving to slot 0-2
    addr = slot * 128;  //slot offset
    for (uint8_t i = 0; i < MAX; i++) {
      if (mode == SAVE_MODE) {  //if we are saving
        if (check_prog_end(i)) {
          EEPROM.update(addr + i, EOP);
          break;
        } else {
          EEPROM.update(addr + i, prog[i]);
        }
      } else if (mode == LOAD_MODE) {
        prog[i] = EEPROM.read(addr + i);
        if (prog[i] == EOP) {
          break;
        }
      } else {
        blink_error(-127);
      }
    }
    blink_both(3);
  }
}

//FUNCTION TO NAVIGATE MENUS WITH < and > BUTTONS
uint8_t menu_nav(uint8_t b) {
  uint8_t max_pos = (mode == LOAD_MODE || mode == SAVE_MODE) ? 2 : 3;
  uint8_t flag = 0;

  if (b == B_RIGHT) {
    mpos = (mpos < max_pos) ? (mpos + 1) : 0;
    flag = 1;
  } else if (b == B_LEFT) {
    mpos = (mpos == 0) ? max_pos : (mpos - 1);
    flag = 1;
  }

  menu_led();
  return flag;
}

//TRANSMIT PROGRAM OVER SERIAL
void send_serial() {

  uint8_t bp = 0;

  Serial.flush();
  do_delay(400);

  while (bp < MAX && prog[bp] != EOP) {
    if (prog[bp] == 0 && prog[bp + 1] == 0) {
      break;
    } else {
      Serial.write(prog[bp]);
      bp++;
    }
  }
  Serial.flush();
  Serial.write(EOP);
}

// LOAD PROGRAM FROM SERIAL
void read_serial() {
  uint8_t done = 0;
  uint8_t count = 0;
  int8_t new_byte = 0;

  while (!done) {

    if (Serial.available() > 0) {

      new_byte = Serial.read();
      prog[count] = new_byte;

      if (count == MAX || new_byte == EOP) {
        done = 1;
        break;
      }
      else {
        count++;
      }
    }
  }
  delay(400);
  send_serial();
}

uint8_t serial_waiting() {
  uint8_t waiting = 1;
  uint8_t mode = 0;
  uint8_t count = 0;

  while (waiting) {

    check_button();
    if (get_bit(flg, BTN_FLAG) && btn > 0) {  //if we have a press
      if (btn == B_SAVE) { waiting = 0; }
      btn = 0;
    }

    if (!Serial.available() && count < 10) {
      Serial.flush();
      Serial.write(255);
      count ++;
    }

    if (Serial.available() > 0) {

      uint8_t in_byte = Serial.read();

      if(in_byte == 1){
        mode = 1;
        while (Serial.available() > 0);
        waiting = 0;
      }
      else if(in_byte == 2){
        mode = 2;
        while (Serial.available() > 0);
        waiting = 0;
      }
    }
  }
  return mode;
}

//LAUNCHER FOR PROGRAMS AND MODES
void run_prog(int8_t selection) {
  if (selection == 0) {
    if (mode == MENU_MODE) {
      mode = RUN_MODE;
      mpos = 3;
      menu_led();
    } else if (mode == RUN_MODE) {
      blink_prog();
    }
  } else if (selection == 1) {
    if (mode == MENU_MODE) {
      led_ctrl(LOW, LOW);
      mode = BYTE_MODE;
      byte_mode_led(pos);
    } else if (mode == RUN_MODE) {
      led_ctrl(LOW, LOW);
      process_voltage(1);
    }
  } else if (selection == 2) {
    if (mode == MENU_MODE) {
      mode = LOAD_MODE;
    } else if (mode == RUN_MODE) {
      blink_both(1);
      led_ctrl(LOW, LOW);
      do_delay(400);

      Serial.begin(2400);
      uint8_t status = serial_waiting();
      if(status == 1){
        send_serial();
      }
      else if(status == 2){
        read_serial();
      }
      Serial.end();

      reset_led_pins();
      blink_both(2);
    }
  } else if (selection == 3) {
    if (mode == MENU_MODE) {
      mode = SAVE_MODE;
    } else if (mode == RUN_MODE) {
      //GAME?
    }
  } else {
    blink_error(2);
  }
}

//MODE FOR SAVING PROGRAM TO EEPROM
void save_mode(uint8_t b) {
  if (!menu_nav(b)) {  //if its not < or > key, check button
    if (b == B_TOGGLE) {
      eeprom_ctrl(mpos, SAVE_MODE);
      mode = MENU_MODE;
      menu_led();
    } else if (b == B_SAVE) {
      goto_menu();  // go back
    }
  }
}

//MODE FOR LOADING PROGRAM FROM EEPROM
void load_mode(uint8_t b) {
  if (!menu_nav(b)) {  //if its not < or > key, check button
    if (b == B_TOGGLE) {
      eeprom_ctrl(mpos, LOAD_MODE);  //load selected program from eeprom
      goto_menu();                   //exit to main menu
    } else if (b == B_SAVE) {
      goto_menu();  //go back
    }
  }
}

//PROGRAM RUN MODE
void run_mode(uint8_t b) {
  if (!menu_nav(b)) {  //if its not < or > key, check button
    if (b == B_SAVE) {
      goto_menu();  //go back
    } else if (b == B_RUN) {
      run_prog(mpos);  //run selected program
    }
  }
}

//MAIN MENU MODE
void menu_mode(uint8_t b) {
  if (!menu_nav(b)) {  //if its not < or > key, check button
    if (b == B_TOGGLE) {
      run_prog(mpos);
    } else if (b == B_SAVE) {
      blink_prog();
    } else if (b == B_RUN) {
      interpret();
    }
  }
}

//PASSES BUTTON PRESSES ONTO CURRENT MODE MENU
void mode_select(uint8_t b) {
  if (mode == MENU_MODE) {
    menu_mode(b);
  } else if (mode == BYTE_MODE) {
    byte_mode(b);
  } else if (mode == BIT_MODE) {
    bit_mode(b);
  } else if (mode == RUN_MODE) {
    run_mode(b);
  } else if (mode == SAVE_MODE) {
    save_mode(b);
  } else if (mode == LOAD_MODE) {
    load_mode(b);
  } else {
    blink_error(1);
  }
}

//FUNCTION TO CHECK VCC VOLTAGE EVERY 20s
void vcc_check_interval() {
  nowTime = millis();                               //get time
  if (nowTime - curVccAdcTime >= vccAdcInterval) {  //if we have reached our count
    check_voltage();                                //check voltage
    curVccAdcTime = nowTime;                        //reset counter
  }
}

//function for debouncing and checking button presses
void check_button() {
  nowTime = millis();  //get time
  uint8_t b = 0;       //button variable
  //if debounce time has passed check button
  if (nowTime - curButtonTime >= buttonInterval) {
    
    b = button();  //check for button press

    if (b > 0 && !get_bit(flg, BTN_FLAG)) {  //if a button is pressed
      toggle_flag_bit(BTN_FLAG);
      btn = b;
      set_random(nowTime); //reset our random number
    } else if (b == 0 && get_bit(flg, BTN_FLAG)) {  //if flag is set and
      toggle_flag_bit(BTN_FLAG);                    //if no button is pressed clear flag
      btn = 0;
    }
    curButtonTime = nowTime;  //reset debounce timer
  }
}

//MAIN LOOP
void loop() {
  vcc_check_interval();                     //check vcc voltage
  check_button();                           //get button press
  if (get_bit(flg, BTN_FLAG) && btn > 0) {  //if we have a press
    mode_select(btn);                       //send off button press
    btn = 0;                                //clear button
  }
}