
#include <PID_v1.h>
#include <PID_AutoTune_v0.h> // https://github.com/t0mpr1c3/Arduino-PID-AutoTune-Library

#define DEFAULT_TEMP_RISE_AFTER_OFF 30.0
#define CONTROL_HYSTERISIS .01

double _CALIBRATE_max_temperature;
int measureInterval = 500;
int tuner_id = 5;
double tuner_noise_band = 1;
double tuner_output_step;

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
    tuner_output_step = Prefs[PRF_PID_WINDOW].value.uint16 / (2.0 * PID_WINDOW_DIVIDER);
    Enable_EMR();
    aTune.Cancel();                         // just in case
    aTune.SetNoiseBand(tuner_noise_band);   // noise band +-1*C
    aTune.SetOutputStep(tuner_output_step); // half of PID window range
    aTune.SetControlType(tuner_id);
    aTune.SetLookbackSec(30);              // 30 seconds lookback for oscillation detection
    aTune.SetSampleTime(measureInterval);
    windowStartTime = millis();
    aTune.Runtime(); // initialize autotuner here, as later we give it actual readings

    // Open CSV log directly -- Init_log_file() would crash because
    // Program_run_name/desc are NULL when no program is loaded
    if (Prefs[PRF_LOG_WINDOW].value.uint16) {
        char str[33];
        struct tm timeinfo;
        if(CSVFile) CSVFile.close();
        if(getLocalTime(&timeinfo)) strftime(str, 32, "/logs/%y%m%d_%H%M%S.csv", &timeinfo);
        else sprintf(str, "/logs/%d.csv", millis());
        if(CSVFile = SPIFFS.open(str, "w")){
#ifdef ENERGY_MON_PIN
            CSVFile.print(String("Date,Temperature,Housing,Energy"));
#else
            CSVFile.print(String("Date,Temperature,Housing"));
#endif
        }
        Add_log_line();
    }
}

void HandleCalibration(unsigned long now)
{
    static uint16_t cal_log_cnt = 0;

    if (aTune.Runtime())
    {
        Disable_EMR();
        Disable_SSR();
        _calP = aTune.GetKp();
        _calI = aTune.GetKi();
        _calD = aTune.GetKd();

        // Sanity check: if Kp is zero or negative, the tuner likely failed
        if (_calP <= 0.0 || _calI < 0.0 || _calD < 0.0) {
            DBG dbgLog(LOG_DEBUG, "[PID] Calibration FAILED - invalid results: PID = [%f, %f, %f]", _calP, _calI, _calD);
            Program_run_state = PR_ABORTED;
        } else {
            // Cap Kd for slow thermal processes -- relay tuning produces Kd proportional
            // to oscillation period, which is huge for kilns. Kd > Kp causes the derivative
            // term to overpower proportional/integral and stall heating.
            double maxKd = _calP * 0.5;
            if (_calD > maxKd) {
                DBG dbgLog(LOG_DEBUG, "[PID] Capping Kd from %f to %f (0.5 * Kp)", _calD, maxKd);
                _calD = maxKd;
            }
            DBG dbgLog(LOG_DEBUG, "[PID] Calibration data available: PID = [%f, %f, %f]", _calP, _calI, _calD);
            Prefs[PRF_PID_KP].value.vfloat = _calP;
            Prefs[PRF_PID_KI].value.vfloat = _calI;
            Prefs[PRF_PID_KD].value.vfloat = _calD;
            KilnPID.SetTunings(_calP, _calI, _calD);
            Save_prefs();
            Program_run_state = PR_NONE;
        }
        cal_log_cnt = 0;
        Close_log_file();
        return;
    }

    if (Prefs[PRF_LOG_WINDOW].value.uint16) {
        cal_log_cnt++;
        if (cal_log_cnt > Prefs[PRF_LOG_WINDOW].value.uint16) {
            cal_log_cnt = 1;
            Add_log_line();
        }
    }

    handle_pid(now);
    _CALIBRATE_max_temperature = max(_CALIBRATE_max_temperature, kiln_temp);
}

void CalibrateAbort()
{
    aTune.Cancel();
    Program_run_state = PR_ABORTED;
    Disable_EMR();
    Disable_SSR();
    Close_log_file();
    DBG dbgLog(LOG_DEBUG, "[PID] Calibration aborted by user");
}

void handle_pid(unsigned long now)
{
    uint16_t pidWindow = Prefs[PRF_PID_WINDOW].value.uint16;
    if (now - windowStartTime > pidWindow) {
        windowStartTime += pidWindow;
    }
    bool heater = (pid_out * PID_WINDOW_DIVIDER > now - windowStartTime);
    heater ? Enable_SSR() : Disable_SSR();
}