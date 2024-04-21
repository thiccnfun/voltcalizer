#pragma once

#ifndef AudioAnalysis_h
#define AudioAnalysis_h

#include "Arduino.h"

// arduinoFFT V2
// See the develop branch on GitHub for the latest info and speedups.
// https://github.com/kosme/arduinoFFT/tree/develop
// if you are going for speed over percision uncomment the lines below.
//#define FFT_SPEED_OVER_PRECISION
//#define FFT_SQRT_APPROXIMATION

#include <arduinoFFT.h>

#ifndef SAMPLE_SIZE
#define SAMPLE_SIZE 1024
#endif
#ifndef BAND_SIZE
#define BAND_SIZE 8
#endif

class AudioAnalysis
{
public:
  enum falloff_type
  {
    NO_FALLOFF,
    LINEAR_FALLOFF,
    ACCELERATE_FALLOFF,
    EXPONENTIAL_FALLOFF,
  };

  AudioAnalysis();
  /* FFT Functions */
  void computeFFT(int32_t *samples, int sampleSize, int sampleRate); // calculates FFT on sample data
  float *getReal();                                                  // gets the Real values after FFT calculation
  float *getImaginary();                                             // gets the imaginary values after FFT calculation

  /* Band Frequency Functions */
  void setNoiseFloor(float noiseFloor);                                // threshold before sounds are registered
  void computeFrequencies(uint8_t bandSize = BAND_SIZE);               // converts FFT data into frequency bands
  void normalize(bool normalize = true, float min = 0, float max = 1); // normalize all values and constrain to min/max.

  void autoLevel(falloff_type falloffType = ACCELERATE_FALLOFF, float falloffRate = 0.01, float min = 10, float max = -1); // auto ballance normalized values to ambient noise levels.
                                                                                                                            // min and max are based on pre-normalized values.
  void setEqualizerLevels(float low = 1, float mid = 1, float high = 1 );   // adjust the frequency levels for a given range - low, medium and high.
                                                                            // 0.5 = 50%, 1.0 = 100%, 1.5 = 150%  the raw value etc.
  void setEqualizerLevels(float *bandEq);                                   // full control over each bands eq value.
  float *getEqualizerLevels(); // gets the last bandEq levels

  bool
  isNormalize();      // is normalize enabled
  bool isAutoLevel(); // is auto level enabled
  bool isClipping();  // is values exceding max

  void bandPeakFalloff(falloff_type falloffType = ACCELERATE_FALLOFF, float falloffRate = 0.05); // set the falloff type and rate for band peaks.
  void vuPeakFalloff(falloff_type falloffType = ACCELERATE_FALLOFF, float falloffRate = 0.05);   // set the falloff type and rate for volume unit peak.

  float *getBands(); // gets the last bands calculated from computeFrequencies()
  float *getPeaks(); // gets the last peaks calculated from computeFrequencies()

  float getBand(uint8_t index); // gets the value at bands index
  float getBandAvg();           // average value across all bands
  float getBandMax();           // max value across all bands
  int getBandMaxIndex();        // index of the highest value band
  int getBandMinIndex();        // index of the lowest value band

  float getPeak(uint8_t index); // gets the value at peaks index
  float getPeakAvg();           // average value across all peaks
  float getPeakMax();           // max value across all peaks
  int getPeakMaxIndex();        // index of the highest value peak
  int getPeakMinIndex();        // index of the lowest value peak

  /* Volume Unit Functions */
  float getVolumeUnit();        // gets the last volume unit calculated from computeFrequencies()
  float getVolumeUnitPeak();    // gets the last volume unit peak calculated from computeFrequencies()
  float getVolumeUnitMax();     // value of the highest value volume unit
  float getVolumeUnitPeakMax(); // value of the highest value volume unit

protected:
  /* Library Settings */
  bool _isAutoLevel = false;
  bool _isClipping = false;
  float _autoMin = 10; // lowest raw value the autoLevel will fall to before stopping. -1 = will auto level down to 0.
  float _autoMax = -1; // highest raw value the autoLevel will rise to before clipping. -1 = will not have any clipping.

  bool _isNormalize = false;
  float _normalMin = 0;
  float _normalMax = 1;

  falloff_type _bandPeakFalloffType = ACCELERATE_FALLOFF;
  float _bandPeakFalloffRate = 0.05;
  falloff_type _vuPeakFalloffType = ACCELERATE_FALLOFF;
  float _vuPeakFalloffRate = 0.05;
  falloff_type _autoLevelFalloffType = ACCELERATE_FALLOFF;
  float _autoLevelFalloffRate = 0.01;

  float calculateFalloff(falloff_type falloffType, float falloffRate, float currentRate);
  template <class X>
  X mapAndClip(X x, X in_min, X in_max, X out_min, X out_max);

  /* FFT Variables */
  int32_t *_samples;
  int _sampleSize;
  int _sampleRate;
  float _real[SAMPLE_SIZE];
  float _imag[SAMPLE_SIZE];
  float _weighingFactors[SAMPLE_SIZE];

  /* Band Frequency Variables */
  float _noiseFloor = 0;
  int _bandSize = BAND_SIZE;
  float _bands[BAND_SIZE];
  float _peaks[BAND_SIZE];
  float _peakFallRate[BAND_SIZE];
  float _peaksNorms[BAND_SIZE];
  float _bandsNorms[BAND_SIZE];
  float _bandEq[BAND_SIZE];

  float _bandAvg;
  float _peakAvg;
  int8_t _bandMinIndex;
  int8_t _bandMaxIndex;
  int8_t _peakMinIndex;
  int8_t _peakMaxIndex;
  float _bandMin;
  float _bandMax; // used for normalization calculation
  float _peakMin;
  float _autoLevelPeakMax; // used for normalization calculation
  // float _peakMinFalloffRate;
  float _autoLevelPeakMaxFalloffRate; // used for auto level calculation

  /* Volume Unit Variables */
  float _vu;
  float _vuPeak;
  float _vuPeakFallRate;
  float _vuMin;
  float _vuMax; // used for normalization calculation
  float _vuPeakMin;
  float _autoLevelVuPeakMax; // used for normalization calculation
  // float _vuPeakMinFalloffRate;
  float _autoLevelMaxFalloffRate; // used for auto level calculation
  ArduinoFFT<float> *_FFT = nullptr;
};

#endif // AudioAnalysis_H