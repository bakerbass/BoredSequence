#include "daisy_field.h"
#include "daisysp.h"

using namespace daisy;
using namespace daisysp;

DaisyField hw;

// Sequencer parameters
int NUM_STEPS = 8;
const int MAX_STEPS = 16;
float stepValues[MAX_STEPS];
bool  gateValues[MAX_STEPS]; // rows A and B  
int currentStep = 0;
float tempo = 120.0f; // BPM
uint32_t lastStepTime = 0;
uint32_t stepInterval = 500; // milliseconds

enum SeqMode {e_Single, e_Dual};
enum KnobMode {e_SeqControls, e_StepVals};
SeqMode seqMode = e_Dual;
KnobMode knobMode = e_StepVals;

// Clock/trigger state
bool gateHigh = false;
uint32_t gateStartTime = 0;
const uint32_t gateDuration = 50; // Gate duration in ms

void UpdateLeds(float *knob_vals, bool *btn_vals)
{
    // Map to the 8 knob LEDs
    size_t knob_leds[] = {
        DaisyField::LED_KNOB_1,
        DaisyField::LED_KNOB_2,
        DaisyField::LED_KNOB_3,
        DaisyField::LED_KNOB_4,
        DaisyField::LED_KNOB_5,
        DaisyField::LED_KNOB_6,
        DaisyField::LED_KNOB_7,
        DaisyField::LED_KNOB_8,
    };
    size_t keyboard_leds[] = {
        DaisyField::LED_KEY_A1,
        DaisyField::LED_KEY_A2,
        DaisyField::LED_KEY_A3,
        DaisyField::LED_KEY_A4,
        DaisyField::LED_KEY_A5,
        DaisyField::LED_KEY_A6,
        DaisyField::LED_KEY_A7,
        DaisyField::LED_KEY_A8,
        DaisyField::LED_KEY_B1,
        DaisyField::LED_KEY_B2,
        DaisyField::LED_KEY_B3,
        DaisyField::LED_KEY_B4,
        DaisyField::LED_KEY_B5,
        DaisyField::LED_KEY_B6,
        DaisyField::LED_KEY_B7,
        DaisyField::LED_KEY_B8,
    };
    // Light up LEDs - current step bright, others show their values dimly
    for(int i = 0; i < NUM_STEPS; i++)
    {
        float brightness;
        if(i == currentStep && gateHigh)
        {
            brightness = 1.0f; // Bright for active step with gate
        }
        else if(i == currentStep)
        {
            brightness = 0.5f; // Medium for current step without gate
        }
        else
        {
            brightness = knob_vals[i] * 0.2f; // Dim based on step value
        }
        hw.led_driver.SetLed(knob_leds[i], brightness);
        switch(seqMode){
            case e_Dual: 
                hw.led_driver.SetLed(keyboard_leds[i], std::min(brightness + 1.f * btn_vals[i], 1.f));
                hw.led_driver.SetLed(keyboard_leds[i + NUM_STEPS], std::min(brightness + 1.f * btn_vals[i + NUM_STEPS], 1.f));
                break;
            case e_Single: 
                hw.led_driver.SetLed(keyboard_leds[i], std::min(brightness + 1.f * btn_vals[i], 1.f));
                break;
            default:
                break;
        }
    }
    hw.led_driver.SwapBuffersAndTransmit();
}
void UpdateDisplay(){
    hw.display.Fill(false);

    char cstr[16];
    switch(knobMode){
        case e_StepVals:
            sprintf(cstr, "Step Mode");
            break;
        case e_SeqControls:
            sprintf(cstr, "Control Mode");
            break;
        default:
            break;
    }
    // sprintf(cstr, "1-2-3-4-5-6-7-8");
    hw.display.SetCursor(0, 10);
    hw.display.WriteString(cstr, Font_7x10, true);

    sprintf(cstr, "BPM: %d", int(tempo));
    hw.display.SetCursor(0, 22);
    hw.display.WriteString(cstr, Font_7x10, true);

    sprintf(cstr, "Steps: %d", NUM_STEPS);
    hw.display.SetCursor(0, 34);
    hw.display.WriteString(cstr, Font_7x10, true);
    hw.display.Update();
}
int main(void)
{
    float sr;
    hw.Init();
    sr = hw.AudioSampleRate();
    
    // Initialize step values to default (evenly spaced)
    for(int i = 0; i < NUM_STEPS; i++)
    {
        stepValues[i] = (float)i / (float)(NUM_STEPS - 1);
    }
    
    hw.StartAdc();
    lastStepTime = System::GetNow();

    // Main loop
    while(1)
    {
        hw.ProcessAnalogControls();
        hw.ProcessDigitalControls();
        
        uint32_t now = System::GetNow();
        
        if(hw.GetSwitch(DaisyField::SW_1)->RisingEdge()){
            knobMode = e_StepVals;
        }
        else if(hw.GetSwitch(DaisyField::SW_2)->RisingEdge()){
            knobMode = e_SeqControls;
        }
        // Read all 8 knobs for step values and 16 gate values
        switch(knobMode) {
            case e_StepVals:
                for(int i = 0; i < NUM_STEPS; i++) {
                    stepValues[i] = hw.knob[i].Process();
                }
                break;
            case e_SeqControls:
                NUM_STEPS = std::round(16 * hw.knob[0].Process());
                if(NUM_STEPS > 8)
                    seqMode = e_Single;
                tempo = std::round(50 + 300 * hw.knob[1].Process());
                break;
            default:
                break;
        }

        for(int i = 0; i < NUM_STEPS * 2; i++) {
            bool buttonPressed = hw.KeyboardRisingEdge(i);
            if(buttonPressed)
                gateValues[i] = !gateValues[i];
        }
        
        // Calculate step interval from tempo (BPM to milliseconds)
        stepInterval = (60000 / tempo);
        UpdateDisplay();
        // Check if it's time to advance to the next step
        if(now - lastStepTime >= stepInterval) {
            lastStepTime = now;
            
            // Advance to next step
            currentStep = (currentStep + 1) % NUM_STEPS;
            
            // Set gate high
            gateHigh = true;
            gateStartTime = now;
            
            // Output the current step value to CV Out 1
            // CV outputs on Field are via the seed's DAC
            float cvValue = stepValues[currentStep];
            hw.seed.dac.WriteValue(DacHandle::Channel::ONE, (uint16_t)(cvValue * 4095.0f));
        }
        
        // Handle gate duration
        if(gateHigh && (now - gateStartTime >= gateDuration)) {
            gateHigh = false;
        }
        
        UpdateLeds(stepValues, gateValues);
        System::Delay(1);
    }
}
