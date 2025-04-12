#include "LCD_DISCO_F429ZI.h"
#include "mbed.h"

// Define the LCD object
LCD_DISCO_F429ZI LCD;

// Debounce delay (in milliseconds)
constexpr int DEBOUNCE_DELAY_MS = 200;

// Timeout objects for each button debounce
Timeout debounce_user_button;
Timeout debounce_external_button;

// Flags to indicate if the buttons are currently in debounce state
volatile bool user_button_debouncing = false;
volatile bool external_button_debouncing = false;

// Callback for user button debounce: resets the debounce flag
void debounce_user_button_callback() {
    user_button_debouncing = false;
}

// Callback for external button debounce: resets the debounce flag
void debounce_external_button_callback() {
    external_button_debouncing = false;
}

// Define the LED and buttons
DigitalOut green_led(LED1);           // Green LED
InterruptIn user_button(BUTTON1);       // User button
InterruptIn external_button(PA_6, PullUp); // External button

// Define timers and random generator objects
Timer reaction_timer;  // Reaction time timer
Timeout random_delay;  // Random delay timeout
Ticker led_ticker;     // LED blinking ticker

// Define application states
enum AppState {
    PREGAME,  // Pre-game state
    START,    // Game start state
    GAME,     // Game state
    RESULT,   // Result state
    CHEATING  // Cheating state
};

AppState state = PREGAME;  // Initial state is pre-game

// Global variables
uint32_t reaction_time = 0;  // Reaction time measured in milliseconds
uint32_t fastest_time = UINT32_MAX;  // Fastest reaction time recorded
bool led_state = false;  // Current LED state (on/off)

// Function declarations
void start_pregame(); // into the prestate
void start_game(); // starting
void on_random_delay_end(); // after sleep's behavior
void show_result();
void reset_game();
void toggle_led();
void on_user_button_press();
void on_external_button_press();
void detect_cheating(); // Function to detect cheating

int main() {
    // Initialize the LCD
    LCD.SetFont(&Font20);
    LCD.SetTextColor(LCD_COLOR_DARKBLUE);
    LCD.Clear(LCD_COLOR_WHITE);

    // Initialize button interrupts
    user_button.fall(&on_user_button_press);      // Trigger when user button is pressed (falling edge)
    external_button.fall(&on_external_button_press); // Trigger when external button is pressed (falling edge)
    srand(time(NULL)); // Initialize random seed with the system time

    // Start in the pre-game state
    start_pregame();

    while (1) {
        switch (state) {
            case PREGAME:
                // Pre-game state: LED is blinking
                break;
            case START:
                // Game start state: waiting for the random delay to expire
                break;
            case GAME:
                // Game state: waiting for the user to press the button
                break;
            case RESULT:
                // Result state: displaying reaction time
                break;
            case CHEATING:
                // Cheating state: display "Cheating" message
                break;
        }
    }
}

// Pre-game state function
void start_pregame() {
    state = PREGAME;
    green_led = 0;  // Turn off LED
    led_ticker.attach(callback(toggle_led), 100ms);  // Blink LED at 10Hz (every 100ms)
    LCD.Clear(LCD_COLOR_WHITE);
    LCD.DisplayStringAt(0, 40, (uint8_t *)"Press to start", CENTER_MODE);
}

// Game start state function
void start_game() {
    state = START;
    green_led = 0;  // Turn off LED
    led_ticker.detach();  // Stop LED blinking

    // Generate a random delay between 1 and 5 seconds
    int delay = rand() % 4000 + 1000;
    random_delay.attach(callback(on_random_delay_end), std::chrono::milliseconds(delay));

    // Update LCD display with waiting message and delay info
    LCD.Clear(LCD_COLOR_WHITE);
    LCD.DisplayStringAt(0, 40, (uint8_t *)"Wait...", CENTER_MODE);
    /* char delay_info[20];
    sprintf(delay_info, "Delay: %d ms", delay);
    LCD.DisplayStringAt(0, 60, (uint8_t *)delay_info, CENTER_MODE); */
}

// Function called when the random delay expires
void on_random_delay_end() {
    state = GAME;
    green_led = 1;  // Turn on LED
    reaction_timer.reset();  // Reset reaction timer
    reaction_timer.start();  // Start reaction timer
    LCD.Clear(LCD_COLOR_WHITE); // Clear LCD
    LCD.DisplayStringAt(0, 40, (uint8_t *)"GO!", CENTER_MODE);
    LCD.DisplayStringAt(0, 80, (uint8_t *)"Press button now!", CENTER_MODE);
    // This is where cheating detection would be checked (if the button was pressed too early)
}

// Function to show the result state (display reaction time)
void show_result() {
    reaction_timer.stop();  // Stop the reaction timer
    // Record the reaction time (from when the timer started until button press)
    reaction_time = std::chrono::duration_cast<std::chrono::milliseconds>(reaction_timer.elapsed_time()).count(); 
    // Update fastest reaction time if the current one is lower
    if (reaction_time < fastest_time) {
        fastest_time = reaction_time;
    }

    // Display the result on the LCD
    state = RESULT;
    LCD.Clear(LCD_COLOR_WHITE);
    char text[50];
    sprintf(text, "current: %u ms", reaction_time);
    LCD.DisplayStringAt(0, 40, (uint8_t *)text, CENTER_MODE);
    sprintf(text, "Fastest: %u ms", fastest_time);
    LCD.DisplayStringAt(0, 60, (uint8_t *)text, CENTER_MODE);
}

// Function to reset the game state
void reset_game() {
    state = PREGAME;
    fastest_time = UINT32_MAX;
    reaction_time = 0;
    green_led = 0;
    
    // Stop all hardware timers
    reaction_timer.stop();
    random_delay.detach();
    led_ticker.detach();
    
    // Immediately update display and return to pre-game state
    LCD.Clear(LCD_COLOR_WHITE);
    start_pregame();
}

// Function to toggle the LED state (used by the ticker)
void toggle_led() {
    led_state = !led_state;
    green_led = led_state;
}

// Interrupt handler for user button press
void on_user_button_press() {
    if (user_button_debouncing) {
        // If the button is currently debouncing, ignore this press
        return;
    }
    
    // Set debounce flag to indicate the button is in debounce period
    user_button_debouncing = true;
    
    // Start the debounce timer: after DEBOUNCE_DELAY_MS, the callback resets the debounce flag
    debounce_user_button.attach(&debounce_user_button_callback, std::chrono::milliseconds(DEBOUNCE_DELAY_MS));
    
    // Determine action based on the current state
    switch (state) {
        case PREGAME:
            start_game();  // Enter game start state
            break;
        case START:
            detect_cheating(); // Check for cheating
            break;
        case GAME:
            show_result();  // Transition to result state
            break;
        case RESULT:
            start_pregame();  // Return to pre-game state
            break;
        case CHEATING:
            start_pregame();  // Return to pre-game state
            break;
    }
}

// Interrupt handler for external button press
void on_external_button_press() {
    reset_game();  // Reset the game
}

// Function to detect cheating
void detect_cheating() {
    state = CHEATING;  // Force the state to CHEATING
    LCD.Clear(LCD_COLOR_WHITE);
    LCD.DisplayStringAt(0, 40, (uint8_t *)"Cheating!", CENTER_MODE);
    green_led = 0;
    
    // Stop the current LED blinking
    led_ticker.detach();
    // Re-attach the ticker to blink the LED at a slower rate (every 500ms)
    led_ticker.attach(callback(toggle_led), 500ms);
    
    reaction_timer.stop(); // Stop the reaction timer
    random_delay.detach();  // Stop the random delay timer
}
