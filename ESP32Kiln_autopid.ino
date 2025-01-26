
#include <PID_v1.h>
#include <PID_AutoTune_v0.h> // https://github.com/t0mpr1c3/Arduino-PID-AutoTune-Library

#define DEFAULT_TEMP_RISE_AFTER_OFF 30.0

double _CALIBRATE_max_temperature;
float measureInterval = 500;
int tuner_id = 5;
double tuner_noise_band = 1;
double tuner_output_step = 1;

float _calP = .5 / DEFAULT_TEMP_RISE_AFTER_OFF;
float _calD = 5.0 / DEFAULT_TEMP_RISE_AFTER_OFF;
float _calI = 4 / DEFAULT_TEMP_RISE_AFTER_OFF;

PID_ATune aTune(&kiln_temp, &pid_out, &set_temp, &now, DIRECT);

void CalibrateInit()
{
    int measureInterval = measureInterval;
    aTune.Cancel();                                // just in case
    aTune.SetNoiseBand(tuner_noise_band);   // noise band +-1*C
    aTune.SetOutputStep(tuner_output_step); // change output +-.5 around initial output
    aTune.SetControlType(tuner_id);
    aTune.SetLookbackSec(measureInterval * 100);
    aTune.SetSampleTime(measureInterval);
    aTune.Runtime(); // initialize autotuner here, as later we give it actual readings
}

void HandleCalibration() {
	if (aTune.Runtime()) {
			_calP = aTune.GetKp();
			_calI = aTune.GetKi();
			_calD = aTune.GetKd();
            DBG dbgLog(LOG_DEBUG,"[PID] Calibration data available: PID = [%f, %f, %f]", _calP, _calI, _calD);
	}

	_CALIBRATE_max_temperature = max(_CALIBRATE_max_temperature, kiln_temp);
}
