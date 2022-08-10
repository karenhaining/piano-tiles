/*   @file      main.c
 *   @author    Karen Haining
 *   @author    Wayne Lai
 *   @date      10-June-2022
 *   @brief     Piano tiles game
 *   
 * University of Washington
 * ECE/CSE 474, 10-June-2022
 * 
 * Lab 4 Project
 * We implemented our own version of the "piano tiles" game, in which a set of
 * tiles (2x2 blocks) fall down the screen (8x8 matrix) and the player must
 * press the corresponding button before the tile hits the bottom. Each tile is 
 * correlated with a specific note of a song, which is played when the button is
 * pressed. If the tile hits the bottom of the screen before the player presses 
 * the button, or if the player presses the wrong button, the game is over. The player
 * gets one point for each correct button press. This score is displayed on the 
 * 7-segment display. As the game continues, the tiles fall faster and faster. 
 * (The flashing LED from the previous part also keeps going).
 * 
 * Acknowledgements: a decent portion of the code for the 7-segment display was recycled from
 *  our lab 3, and a decent portion of the code for the 8x8 matrix was recycled from our lab 2.
 *  We would also like to acknowledge Rick Astley, who wrote the song "Never Gonna Give You Up,"
 *  as this is the song our game plays. (Song transcribed by Karen Haining).
 * 
 */


#include <Arduino_FreeRTOS.h>
#include <queue.h>

/*** related to blink task ***/
#define LED_PORT    PORTB       ///< the port for the external LED
#define LED_1       1 << PB5    ///< the external LED
#define OFF         0           ///< off state for LED

/*** related to speaker task ***/
// notes by scale degree, in C major
#define NOTE_5      5100        ///< 196 Hz (G3)
#define NOTE_6      4544        ///< 220 Hz (A3)
#define NOTE_7      4056        ///< 247 Hz (B3)
#define NOTE_1      3830        ///< 261 Hz (C4)
#define NOTE_2      3400        ///< 294 Hz (D4)
#define NOTE_3      3038        ///< 329 Hz (E4)
#define NOTE_4      2864        ///< 349 Hz (F4)
#define NOTE_5H     2550        ///< 392 Hz (G4)
#define NOTE_REST   0           ///< rest
int NGGYU[] = {NOTE_5, NOTE_6, NOTE_1, NOTE_6, NOTE_3, NOTE_3, NOTE_2, NOTE_5, NOTE_6, NOTE_1, 
               NOTE_6, NOTE_2, NOTE_2, NOTE_1, NOTE_5, NOTE_6, NOTE_1, NOTE_6, NOTE_1, NOTE_2, 
               NOTE_7, NOTE_6, NOTE_5, NOTE_2, NOTE_1, NOTE_5, NOTE_6, NOTE_1, NOTE_6, NOTE_3, 
               NOTE_3, NOTE_2, NOTE_5, NOTE_6, NOTE_1, NOTE_6, NOTE_5H, NOTE_7, NOTE_1, NOTE_5,
               NOTE_6, NOTE_1, NOTE_6, NOTE_1, NOTE_2, NOTE_7, NOTE_6, NOTE_5, NOTE_2, NOTE_1};   ///< notes of song
#define NGGYU_LENGTH  50                                                                          ///< length of song
int note_index = 0;                                                                               ///< which note of the song is playing

/*** related to button input ***/
#define BUTTON_0    8           ///< pin of button 0
#define BUTTON_1    9           ///< pin of button 1
#define BUTTON_2    10          ///< pin of button 2
#define BUTTON_3    11          ///< pin of button 3
#define NUM_BUTTONS 4           ///< the number of buttons
int BUTTONS[] = {BUTTON_0, BUTTON_1, BUTTON_2, BUTTON_3};                         ///< array of buttons

/*! states that a button can be in */
enum bp_state {
  NOT_PRESSED,  /*!< button is not being pressed */
  INIT_PRESSED, /*!< button has been pressed for the first time */
  CONT_PRESSED  /*!< button is being held */
};

bp_state button_states[4] = {NOT_PRESSED, NOT_PRESSED, NOT_PRESSED, NOT_PRESSED}; ///< array representing the states of the four buttons

/*** related to 7-segment display ***/
#define DIGIT_PORT  PORTK       ///< port that digit select is
#define LINE_PORT   PORTF       ///< port that A-G is
#define DIS_0       0b00111111  ///< displays the digit 0
#define DIS_1       0b00000110  ///< displays the digit 1
#define DIS_2       0b01011011  ///< displays the digit 2
#define DIS_3       0b01001111  ///< displays the digit 3
#define DIS_4       0b01100110  ///< displays the digit 4
#define DIS_5       0b01101101  ///< displays the digit 5
#define DIS_6       0b01111101  ///< displays the digit 6
#define DIS_7       0b00000111  ///< displays the digit 7
#define DIS_8       0b01111111  ///< displays the digit 8
#define DIS_9       0b01100111  ///< displays the digit 9
int DIGITS_LIST[] = {DIS_0, DIS_1, DIS_2, DIS_3, DIS_4, DIS_5, DIS_6, DIS_7, DIS_8, DIS_9}; ///< all possible values for digits
int DIGIT_ARRAY[4] = {0, 0, 0, 0};  ///< digits to be displayed on the 7-segment display
int score = 0;                  ///< player score to be displayed on the 7-segment display

/*** related to 8x8 matrix display and tiles ***/
// defines the bits corresponding to each row and column of the matrix
#define ROW_1 1 << PC7      ///< bit corresponding to row 1 of the matrix
#define ROW_2 1 << PC6      ///< bit corresponding to row 2 of the matrix
#define ROW_3 1 << PC5      ///< bit corresponding to row 3 of the matrix
#define ROW_4 1 << PC4      ///< bit corresponding to row 4 of the matrix
#define ROW_5 1 << PC3      ///< bit corresponding to row 5 of the matrix
#define ROW_6 1 << PC2      ///< bit corresponding to row 6 of the matrix
#define ROW_7 1 << PC1      ///< bit corresponding to row 7 of the matrix
#define ROW_8 1 << PC0      ///< bit corresponding to row 8 of the matrix
#define COL_1 1 << PL7      ///< bit corresponding to col 1 of the matrix
#define COL_2 1 << PL6      ///< bit corresponding to col 2 of the matrix
#define COL_3 1 << PL5      ///< bit corresponding to col 3 of the matrix
#define COL_4 1 << PL4      ///< bit corresponding to col 4 of the matrix
#define COL_5 1 << PL3      ///< bit corresponding to col 5 of the matrix
#define COL_6 1 << PL2      ///< bit corresponding to col 6 of the matrix
#define COL_7 1 << PL1      ///< bit corresponding to col 7 of the matrix
#define COL_8 1 << PL0      ///< bit corresponding to col 8 of the matrix
#define OFF_SCREEN 9        ///< row at which tiles would not be on the screen yet

// note that the rows make up the entirety of PORTC and the columns make up the entirety of PORTL
#define ROW_PORT PORTC                                                  ///< the port that rows are in (should drive low to turn on)
#define COL_PORT PORTL                                                  ///< the port that cols are in (should drive high to turn on)
int ROW[] = {ROW_1, ROW_2, ROW_3, ROW_4, ROW_5, ROW_6, ROW_7, ROW_8};   ///< array representing all the rows of the matrix
int COL[] = {COL_1, COL_2, COL_3, COL_4, COL_5, COL_6, COL_7, COL_8};   ///< array representing all the columns of the matrix

/*! represents a tile */
typedef struct tile {
  int lane;             /*!< which of the four lanes tile is in (0-3 inclusive) */
  int row;              /*!< which row the top of the tile is on */
  boolean is_pressed;   /*!< true iff a button pressed it */
} tile;

tile tile_list[NGGYU_LENGTH];   ///< list of tiles
int bottom_tile = 0;            ///< index of tile_list that the bottom-most tile on the screen is at

int fall_period = 200;          ///< period of tile fall rate

/*** related to scheduler settings and tasks ***/
#define INCLUDE_vTaskSuspend  1       ///< enable task suspension
#define configMAX_PRIORITIES  10      ///< change maximum priorities

TaskHandle_t tileDisplayHandle = 1;   ///< handle for tile display task
TaskHandle_t speakerHandle = 2;       ///< handle for speaker task

// function headers for tasks
void TaskSpeaker(void *pvParameters);
void TaskTileDisplay(void *pvParameters);
void TaskTileFall(void *pvParameters);
void TaskButtonPress(void *pvParameters);
void TaskBlink(void *pvParameters);
void TaskScoreDisplay(void *pvParameters);

/**
 * @brief initializes the tiles for the start of the game
 * 
 * Pseudo-randomly gives each tile a <<tt>>lane</tt> and places it in
 * a <tt>row</tt> from which it will fall. Also initializes <tt>is_pressed</tt>
 * to be false. 
 * 
 * @author Karen Haining
 * @author Wayne Lai
 * 
 */
void initialize_tiles() {
  // for every tile...
  for (int i = 0; i < NGGYU_LENGTH; i++) {
    tile t;
    t.lane = random(0,4);     // pseudo-randomly assign lane
    t.row = 2*i + 7;          // set row position
    t.is_pressed = false;
    tile_list[i] = t;
  }
}

void setup() {
  // wait for serial port to connect. Needed for native USB, on LEONARDO, MICRO, YUN, and other 32u4 based boards.
  while (!Serial) {;} 

  // create tasks for the scheduler
  xTaskCreate(TaskSpeaker,        "Speaker",              128, NULL, 4,   &speakerHandle);
  xTaskCreate(TaskTileDisplay,    "Display tile pattern", 128, NULL, 10,  &tileDisplayHandle);
  xTaskCreate(TaskTileFall,       "Make tiles fall",      128, NULL, 9,   NULL);
  xTaskCreate(TaskBlink,          "Blink LED",            128, NULL, 3,   NULL);
  xTaskCreate(TaskButtonPress,    "Handles button press", 128, NULL, 9,   NULL);
  xTaskCreate(TaskScoreDisplay,   "Displays score",       128, NULL, 10,  NULL);

  // populate the tiles list
  initialize_tiles();

  // allows the hardware to load smoothly
  delay(1000);

  // start scheduler
  vTaskStartScheduler();
}

void loop(){}


/*--------------------------------------------------*/
/*-------------- Task Helper Functions -------------*/
/*--------------------------------------------------*/

/**
 * @brief clears 8x8 matrix
 *
 * Clears the 8x8 matrix of all LEDs.
 * 
 * @author Karen Haining
 * @author Wayne Lai
 */
void clear_board() {
  ROW_PORT = 0b11111111;  // driving rows high turns them off
  COL_PORT = 0;           // driving cols low turns them off
}

/**
 * @brief ends the game
 *
 * Clears the 8x8 matrix of all tiles and stops the speaker sound.
 * Leaves the score displayed. 
 * 
 * @author Karen Haining
 * @author Wayne Lai
 */
void end_game() {
  vTaskSuspend(tileDisplayHandle);  // turn off tile display
  OCR4A = 0;                        // stop speaker sound
  vTaskSuspend(speakerHandle);      // stop speaker task
  clear_board();                    // clear board
}

/**
 * @brief updates state of a button
 * 
 * Updates the state of button at index @p index in <tt>BUTTONS</tt> 
 * to be <tt>INIT_PRESSED</tt>, <tt>CONT_PRESSED</tt>, or 
 * <tt>NOT_PRESSED</tt>.
 *
 * @param index an int, which index the button is in <tt>BUTTONS</tt>
 *
 * @author Karen Haining
 * @author Wayne Lai
 */
void update_button(int index) {
  if (digitalRead(BUTTONS[index])) {      // if button is pressed
    if (button_states[index] == INIT_PRESSED || button_states[index] == CONT_PRESSED) { // if was already pressed
      button_states[index] = CONT_PRESSED;
    } else {                                                                            // if was not already pressed
      button_states[index] = INIT_PRESSED;
    } 
  } else {                                // if button is not pressed
      button_states[index] = NOT_PRESSED;
  }
}

/**
 * @brief updates states of all buttons
 * 
 * Updates the state of all buttons in <tt>BUTTONS</tt> to be 
 * <tt>INIT_PRESSED</tt>, <tt>CONT_PRESSED</tt>, or <tt>NOT_PRESSED</tt>.
 * 
 * @see update_button 
 * 
 * @author Karen Haining
 * @author Wayne Lai
 */
void update_buttons() {
  // update all buttons
  for (int i = 0; i < NUM_BUTTONS; i++) {
    update_button(i);
  }
}

/**
 * @brief displays a digit on the 7-segment display
 * 
 * Displays the digit @p num on the 7-segment display in position @p pos. 
 * 
 * @param num an int, the digit to be set. If @p num > 9 or @p num < 0, the @p pos will turn off
 * @param pos an int, which position to display the digit (0 is the least significant bit, 3 is the most significant bit)
 * 
 * @warning unspecific behavior if @p pos is not between 0 and 3 (inclusive)
 * 
 * @author Karen Haining
 * @author Wayne Lai
 */
void display_num(int num, int pos) {
  // drive low to correct port
  DIGIT_PORT = ~(1 << pos);

  // display digit
  if (num > 9 || num < 0){
    LINE_PORT = OFF;
  } else {
    LINE_PORT = DIGITS_LIST[num];
  }

  vTaskDelay(1);
}

/**
 * @brief updates and displays the digits to be displayed on the 7-segment display
 * 
 * Sets the global digits array <tt>DIGIT_ARRAY</tt> to be the number @p num. Displays
 * this number on the 7-segment display.
 * 
 * @param num an int, the number to be displayed
 * 
 * @note if @p num is more than 4 digits long, will only display the 4 least significant digits
 * @see display_num
 * 
 * @author Karen Haining
 * @author Wayne Lai
 */
void update_num(int num) {
  // calculate individual digits
  DIGIT_ARRAY[0] = num % 10;
  DIGIT_ARRAY[1] = ((num % 100) - DIGIT_ARRAY[0]) / 10;
  DIGIT_ARRAY[2] = ((num % 1000) - (10*DIGIT_ARRAY[1]) - DIGIT_ARRAY[0]) / 100;
  DIGIT_ARRAY[3] = ((num % 10000) - (100*DIGIT_ARRAY[2]) - (10*DIGIT_ARRAY[1]) - DIGIT_ARRAY[0]) / 1000;

  // displays each digit
  for (int i = 0; i < 4; i++) {
    display_num(DIGIT_ARRAY[i], i);
  }
}


/*--------------------------------------------------*/
/*---------------------- Tasks ---------------------*/
/*--------------------------------------------------*/

/**
 * @brief task that causes LED to blink
 *
 * Blinks an off-board LED for 100 ms on, 200 ms off.
 * 
 * @param pvParameters void pointer to parameters for the task
 * 
 * @author Karen Haining
 * @author Wayne Lai
 */
void TaskBlink(void *pvParameters) {
  // initialize digital pin 13 as an output (also is the pin for the built-in LED)
  pinMode(LED_BUILTIN, OUTPUT);

  for (;;) {
    // turn on LED for 100 ms
    digitalWrite(LED_BUILTIN, HIGH);
    vTaskDelay( 100 / portTICK_PERIOD_MS );

    // turn off LED for 200 ms
    digitalWrite(LED_BUILTIN, LOW);
    vTaskDelay( 200 / portTICK_PERIOD_MS );
  }
}

/**
 * @brief task that handles button pressing
 *
 * Updates the state of the buttons. If the correct button
 * is pressed, the score updates and the game continues. If not,
 * the game ends. (Also, if the tile hits the bottom and no
 * button is being pressed, the game ends).
 *
 * @param pvParameters void pointer to parameters for the task
 * 
 * @author Karen Haining
 * @author Wayne Lai
 */
void TaskButtonPress(void *pvParameters) {

  // configure corresponding pins as inputs
  pinMode(BUTTON_0, INPUT);
  pinMode(BUTTON_1, INPUT);
  pinMode(BUTTON_2, INPUT);
  pinMode(BUTTON_3, INPUT);

  for (;;) {

    // update states of buttons
    update_buttons();

    // determine which button is currently being pressed
    int pressed_button = -1;
    for (int i = 0; i < NUM_BUTTONS; i++) {
      if (button_states[i] == INIT_PRESSED || button_states[i] == CONT_PRESSED) {
        pressed_button = i;
      }
    }

    vTaskDelay(1);

    // figure out relationship of the bottom tile and the button being pressed
    bool hit_bottom_no_press = tile_list[bottom_tile].row < 0 && pressed_button == -1;
    bool hit_bottom_wrong = tile_list[bottom_tile].row < 0 && pressed_button != tile_list[bottom_tile].lane;
    bool hit_wrong_tile = pressed_button != -1 && pressed_button != tile_list[bottom_tile].lane && button_states[pressed_button] == INIT_PRESSED;

    if (hit_bottom_wrong || hit_wrong_tile || hit_bottom_no_press) {      // if the user made one of these mistakes... game over
      end_game();
    } else if (pressed_button == tile_list[bottom_tile].lane && button_states[pressed_button] == INIT_PRESSED) {  // else, game continues

      // update relevant information
      tile_list[bottom_tile].is_pressed = true;
      note_index++;
      score++;

      // put the bottom tile to now be at the top of the falling tiles
      tile_list[bottom_tile].lane = random(0,4);
      tile_list[bottom_tile].row = 2*(NGGYU_LENGTH-1) + 7;
      tile_list[bottom_tile].is_pressed = false;

      // the next tile is now the bottom tile
      bottom_tile++;
    }
  }
}


/**
 * @brief task regarding the playing of the speaker
 *
 * Plays the note at index <tt>note_index</tt> in <tt>NGGYU</tt> on the speaker.
 *
 * @param pvParameters void pointer to parameters for the task
 * 
 * @author Karen Haining
 * @author Wayne Lai
 */
void TaskSpeaker(void *pvParameters) {

  // setup timer
  PRR1 = 0;                 // enables timer (see pg. 56 of data sheet)
  TCCR4B = 1 << CS41;       // set prescale to 8 (see pg. 156-157)
  TCCR4B |= 1 << WGM42;     // set to CTC (see pg. 145, 154-156)
  TCCR4A = 1 << COM4A0;     // toggle
  DDRH = 1 << DDH3;         // enable hardware output (p. 89, 99)
  
  for (;;) {
    // set the note value and play the note
    OCR4A = NGGYU[note_index];
  }
}

/**
 * @brief task related to the tiles being displayed on the 8x8 matrix
 *
 * Displays the correct tiles on the 8x8 matrix.
 *
 * @param pvParameters void pointer to parameters for the task
 * 
 * @author Karen Haining
 * @author Wayne Lai
 */
void TaskTileDisplay(void *pvParameters) {

  // enable the output for the matrix
  DDRL = 0b11111111;
  DDRC = 0b11111111;

  for (;;) {

    clear_board();

    // set display for tiles
    for (int i = bottom_tile; i < NGGYU_LENGTH && tile_list[i].row <= OFF_SCREEN; i++) {    // for every tile that is on the screen...
      tile t = tile_list[i];
      
      if (!t.is_pressed) {      // if the tile has not been pressed yet...
        if (t.row == OFF_SCREEN - 1) {    // if the tile is fully in the screen
          ROW_PORT = ~(ROW[2*t.lane] | ROW[2*t.lane+1]);
          COL_PORT = COL[t.row - 1];
        } else if (t.row == 0) {          // if the tile is at the very top of the screen and the bottom half is displayed
          ROW_PORT = ~(ROW[2*t.lane] | ROW[2*t.lane+1]);
          COL_PORT = COL[t.row]; 
        } else if (t.row < 8) {           // if the tile is at the very bottom of the screen and the top half is displayed
          ROW_PORT = ~(ROW[2*t.lane] | ROW[2*t.lane+1]);
          COL_PORT = COL[t.row] | COL[t.row-1];
        }
      }
      vTaskDelay(1);
    }
  }  
}

/**
 * @brief task that makes the tiles fall
 * 
 * Calculates the position of all the tiles. Decreases the <tt>fall_periond</tt>
 * every time the function funs.
 *
 * @param pvParameters void pointer to parameters for the task
 * 
 * @author Karen Haining
 * @author Wayne Lai
 */
void TaskTileFall(void *pvParameters) {

  for (;;) {
    
    // reset bottom tile and note index if necessary
    if (bottom_tile >= NGGYU_LENGTH) bottom_tile = 0;
    if (note_index >= NGGYU_LENGTH) note_index = 0;
    
    // makes the tiles fall for the next time they will be displayed
    for (int i = 0; i < NGGYU_LENGTH; i++) {      
      tile_list[i].row--;

      // reset the tile if necessary (bottom_tile will be reset in TaskButtonPress)
      if (tile_list[i].row < 0 && i != bottom_tile) {
        tile_list[i].row = 2*49 + 7;
        tile_list[i].lane = random(0,4);
        tile_list[i].is_pressed = false;
      }
    }

    // give tile time to stay in one position, but shorten this time every round  
    vTaskDelay(fall_period / portTICK_PERIOD_MS);
    fall_period--;
  }
}

/**
 * @brief task related to the 7-segment score display
 * 
 * Displays the player's score on the 7-segment display.
 *
 * @param pvParameters void pointer to parameters for the task
 * 
 * @see update_num
 * 
 * @author Karen Haining
 * @author Wayne Lai
 */
void TaskScoreDisplay(void *pvParameters) {

  // enable output
  DDRF = 0b11111111;    // positions A-G
  DDRK = 0b00001111;    // digit select
  
  for (;;) {
    // display score
    update_num(score);
  }
}
