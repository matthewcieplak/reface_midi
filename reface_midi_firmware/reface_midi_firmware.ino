#include <MIDI.h>
#include <elapsedMillis.h>
#include <EEPROM.h>
/*
YAMAHA REFACE CS MIDI PATCH STORAGE
Copyright Matthew Cieplak 2022
*/

const int MIDI_CHANNEL = 1; //what channel to transmit/recieve CC messages on
const int LED_PIN = 13; //onboard LED
const int ALT_LED_PIN = 12; //external LED
const int SWITCH_PIN = 0; //digital pin used for bank switch
const int PATCH_SIZE_BYTES = 24; //number of bytes allocated to each patch (17 minimum, plus some room for extra parameters added later
                                 //Atmega168p has 512 bytes of EEPROM, so for 16 patches max size is 32 bytes
                                 //Atmega328p has 2kB of EEPROM, so for 16 patches max size is 128 bytes
const int SAVE_TIMEOUT = 2000; //hold button for 2 seconds to save
const int DEBOUNCE_TIMEOUT = 20; //milliseconds of input debouncing on button presses
const int num_parameters = 17;

const int CC_numbers[17] = {
  17, //Effect type
  18, //Effect depth
  19, //Effect rate
  79, //EG sustain level
  75, //EG decay time
  72, //EG release time
  83, //EG FEG-AEG balance
  73, //EG attack time
  71, //Filter resonance
  74, //Filter cutoff
  20, //Portamento
  76, //LFO speed
  77, //LFO depth
  78, //LFO assign
  80, //Oscillator type
  82, //Oscillator mod
  81 //Oscillator texture
};



const int CC_default[17] = {
  127, //Effect type off
  0,   //Effect depth 0
  0,   //Effect rate 0
  120, //EG sustain level
  10,  //EG decay time
  0,   //EG release time
  64,  //EG FEG-AEG balance
  0,   //EG attack time
  10,  //Filter resonance
  100, //Filter cutoff
  0,   //Portamento
  64,  //LFO speed
  0,   //LFO depth
  64,  //LFO assign
  0,  //Oscillator type
  10,  //Oscillator mod
  10   //Oscillator texture
};

const int CC_indices[17];
int CC_values[17];

elapsedMillis button_timer;


const int button_inputs[8] = {2, 3, 4, 5, 6, 7, 8, 9}; //pin numbers for digital button inputs
const int switch_pin = 10;
bool button_states[8] = {false, false, false, false, false, false, false, false};
int button_pressed = -1;
bool bank_switch = false;
bool saved = true;




MIDI_CREATE_DEFAULT_INSTANCE();

void CCInput(byte channel, byte cc_num, byte value) {

  //store received parameters in active RAM
  for (int i = 0; i < num_parameters; i++) {
    if (cc_num == CC_numbers[i]) {
      CC_values[i] = value;
      digitalWrite(LED_PIN, (value % 2 == 0) ? HIGH : LOW);

      break;
    }
  }
}

void noteOn(byte channel, byte velocity, byte note) {
  digitalWrite(LED_PIN, HIGH);
}

void noteOff(byte channel, byte velocity, byte note) {
  digitalWrite(LED_PIN, LOW);
}


void setup() {
  pinMode(LED_PIN, OUTPUT);
  pinMode(ALT_LED_PIN, OUTPUT);
  pinMode(switch_pin, INPUT_PULLUP);
  digitalWrite(LED_PIN, HIGH);

  int i;
  for (i = 0; i < 8; i++) {
    pinMode(button_inputs[i], INPUT_PULLUP);
  }

  for (i = 0; i < num_parameters; i++){
    CC_values[i] = CC_default[i];
  }

  blink();

  MIDI.begin(MIDI_CHANNEL_OMNI);
  MIDI.turnThruOff();
  MIDI.setHandleControlChange(CCInput); // This command tells the MIDI Library
  MIDI.setHandleNoteOn(noteOn);
  MIDI.setHandleNoteOff(noteOff);
}

void storePatchToMemory(int patch_num){
   for (int i = 0; i < num_parameters; i++) {
     EEPROM.update(i + (patch_num * PATCH_SIZE_BYTES), CC_values[i]);
   }
}

void readPatchFromMemory(int patch_num){
   for (int i = 0; i < num_parameters; i++) {
     CC_values[i] = EEPROM.read(i + (patch_num * PATCH_SIZE_BYTES));
   }
}

void sendPatchToKeyboard(){
   for (int i = 0; i < num_parameters; i++) {
     MIDI.sendControlChange(CC_numbers[i], CC_values[i], MIDI_CHANNEL);
  }
}

void blink(){
  digitalWrite(ALT_LED_PIN, HIGH); delay(125);
  digitalWrite(ALT_LED_PIN, LOW); delay(125);
  digitalWrite(ALT_LED_PIN, HIGH);
}

void readButtons(){
  if (button_pressed > -1 && button_timer > SAVE_TIMEOUT && !saved) { //on holding for 3s, save patch to memory and blink 3x
    readBankSwitch();
    storePatchToMemory(button_pressed + (bank_switch ? 8 : 0));
    button_pressed = -1;
    blink();
    blink();
    blink();
    saved = true;
  }


  int i;
  for (i = 0; i < 8; i++) {
    if (digitalRead(button_inputs[i]) == LOW) { //LOW = button is pressed
      if (button_states[i] == false) {
        onButtonPressed(i);
      } else if (button_pressed != i && button_timer > DEBOUNCE_TIMEOUT) { //after debounce, set button status and wait
        button_pressed = i;
      }
    } else if (button_states[i] == true)  {
      onButtonReleased(i);
    }
  }
}

void onButtonPressed(int i) {
  button_states[i] = true;
  button_timer = 0;

  digitalWrite(ALT_LED_PIN, LOW);
  digitalWrite(LED_PIN, LOW);
}

void onButtonReleased(int i) {
  //on release, send patch to keyboard and blink
  button_states[i] = false;
  if (button_pressed == i && !saved) { //if just finished saving, skip sending patch back to kbd
    readBankSwitch();
    readPatchFromMemory(button_pressed + (bank_switch ? 8 : 0));
    sendPatchToKeyboard();
    blink();
  }

  //reset to default status
  saved = false;
  button_pressed = -1;
  digitalWrite(ALT_LED_PIN, HIGH);
  digitalWrite(LED_PIN, HIGH);
}

void readBankSwitch(){
  if (digitalRead(switch_pin) == LOW) {
    bank_switch = true;
  } else {
    bank_switch = false;
  }
}

void loop() {
  MIDI.read();
  readButtons();
}
