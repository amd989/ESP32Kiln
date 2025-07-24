
#include <PID_v1.h>
#include <PID_AutoTune_v0.h> // https://github.com/t0mpr1c3/Arduino-PID-AutoTune-Library

#define DEFAULT_TEMP_RISE_AFTER_OFF 30.0
#define CONTROL_HYSTERISIS .01

double _CALIBRATE_max_temperature;
int measureInterval = 500;
int tuner_id = 5;
double tuner_noise_band = 1;
double tuner_output_step = 1;

float _calP = .5 / DEFAULT_TEMP_RISE_AFTER_OFF;
float _calI = 4 / DEFAULT_TEMP_RISE_AFTER_OFF;
float _calD = 5.0 / DEFAULT_TEMP_RISE_AFTER_OFF;

PID_ATune aTune(&kiln_temp, &pid_out, &set_temp, &now, DIRECT);

void CalibrateInit()
{
    Program_run_state = PR_CALIBRATE;    
    measureInterval = Prefs[PRF_PID_MEASURE_INTERVAL].value.uint16;
    tuner_id = Prefs[PRF_PID_ALGORITHM].value.uint8;
    set_temp = 300;
    Enable_EMR();
    aTune.Cancel();                         // just in case
    aTune.SetNoiseBand(tuner_noise_band);   // noise band +-1*C
    aTune.SetOutputStep(tuner_output_step); // change output +-.5 around initial output
    aTune.SetControlType(tuner_id);
    aTune.SetLookbackSec(30);              // 30 seconds lookback for oscillation detection
    aTune.SetSampleTime(measureInterval);
    windowStartTime = millis();
    aTune.Runtime(); // initialize autotuner here, as later we give it actual readings
}

void HandleCalibration(unsigned long now)
{
    if (aTune.Runtime())
    {
        Program_run_state = PR_NONE; // end calibration
        Disable_EMR();
        Disable_SSR();
        _calP = aTune.GetKp();
        _calI = aTune.GetKi();
        _calD = aTune.GetKd();
        DBG dbgLog(LOG_DEBUG, "[PID] Calibration data available: PID = [%f, %f, %f]", _calP, _calI, _calD);
        Prefs[PRF_PID_KP].value.vfloat = _calP;
        Prefs[PRF_PID_KI].value.vfloat = _calI;
        Prefs[PRF_PID_KD].value.vfloat = _calD;
        KilnPID.SetTunings(_calP, _calI, _calD);
        Save_prefs();
    }

    handle_pid(now);
    _CALIBRATE_max_temperature = max(_CALIBRATE_max_temperature, kiln_temp);
}

void handle_pid(unsigned long now)
{
    bool heater = (now - windowStartTime < (measureInterval * pid_out) && pid_out > CONTROL_HYSTERISIS) ||
                  (now - windowStartTime >= (measureInterval * pid_out) && pid_out > 1.0 - CONTROL_HYSTERISIS);
    heater ? Enable_SSR() : Disable_SSR();
}