#include "AudioAnalysis.h"

AudioAnalysis::AudioAnalysis()
{
  // set default eq levels;
  for (int i = 0; i < _bandSize; i++)
  {
    _bandEq[i] = 1.0;
  }
}

void AudioAnalysis::computeFFT(int32_t *samples, int sampleSize, int sampleRate)
{
  _samples = samples;
  if (_FFT == nullptr || _sampleSize != sampleSize || _sampleRate != sampleRate)
  {
    _sampleSize = sampleSize;
    _sampleRate = sampleRate;
    _FFT = new ArduinoFFT<float>(_real, _imag, _sampleSize, _sampleRate, _weighingFactors);
  }

  // prep samples for analysis
  for (int i = 0; i < _sampleSize; i++)
  {
    _real[i] = samples[i];
    _imag[i] = 0;
  }

  _FFT->dcRemoval();
  _FFT->windowing(FFTWindow::Hamming, FFTDirection::Forward, false); /* Weigh data (compensated) */
  _FFT->compute(FFTDirection::Forward);                              /* Compute FFT */
  _FFT->complexToMagnitude();                                        /* Compute magnitudes */
}

float *AudioAnalysis::getReal()
{
  return _real;
}

float *AudioAnalysis::getImaginary()
{
  return _imag;
}

void AudioAnalysis::setNoiseFloor(float noiseFloor)
{
  _noiseFloor = noiseFloor;
}

float getPoint(float n1, float n2, float percent)
{
  float diff = n2 - n1;

  return n1 + (diff * percent);
}

void AudioAnalysis::setEqualizerLevels(float low, float mid, float high)
{
  float xa, ya, xb, yb, x, y;
  // low curve
  float x1 = 0;
  float lowSize = _bandSize / 4;
  float y1 = low;
  float x2 = lowSize / 2;
  float y2 = low;
  float x3 = lowSize;
  float y3 = (low + mid)/2.0;
  for (int i = x1; i < lowSize; i++)
  {
    float p = (float)i / (float)lowSize;
    //xa = getPoint(x1, x2, p);
    ya = getPoint(y1, y2, p);
    //xb = getPoint(x2, x3, p);
    yb = getPoint(y2, y3, p);

    //x = getPoint(xa, xb, p);
    y = getPoint(ya, yb, p);

    _bandEq[i] = y;
  }

  // mid curve
  x1 = lowSize;
  float midSize = (_bandSize-lowSize) / 2;
  y1 = y3;
  x2 = x1 + midSize / 2;
  y2 = mid;
  x3 = x1 + midSize;
  y3 = (mid + high) / 2.0;
  for (int i = x1; i < x1+midSize; i++)
  {
    float p = (float)(i - x1) / (float)midSize;
    // xa = getPoint(x1, x2, p);
    ya = getPoint(y1, y2, p);
    // xb = getPoint(x2, x3, p);
    yb = getPoint(y2, y3, p);

    // x = getPoint(xa, xb, p);
    y = getPoint(ya, yb, p);

    _bandEq[i] = y;
  }

  // high curve
  x1 = lowSize + midSize;
  float highSize = midSize;
  y1 = y3;
  x2 = x1 + highSize / 2;
  y2 = high;
  x3 = x1 + highSize;
  y3 = high;
  for (int i = x1; i < x1+highSize; i++)
  {
    float p = (float)(i - x1) / (float)highSize;
    // xa = getPoint(x1, x2, p);
    ya = getPoint(y1, y2, p);
    // xb = getPoint(x2, x3, p);
    yb = getPoint(y2, y3, p);

    // x = getPoint(xa, xb, p);
    y = getPoint(ya, yb, p);

    _bandEq[i] = y;
  }
}

void AudioAnalysis::setEqualizerLevels(float *bandEq)
{
  // blind copy of eq percentages
  for(int i = 0; i < _bandSize; i++) {
    _bandEq[i] = bandEq[i];
  }
}

float *AudioAnalysis::getEqualizerLevels()
{
  return _bandEq;
}

void AudioAnalysis::computeFrequencies(uint8_t bandSize)
{
  // TODO: use maths to calculate these offset values. Inputs being _sampleSize and _bandSize output being similar exponential curve below.
  // band offsets helpers based on 1024 samples
  const static uint16_t _2frequencyOffsets[2] = {24, 359};
  const static uint16_t _4frequencyOffsets[4] = {6, 18, 72, 287};
  const static uint16_t _8frequencyOffsets[8] = {2, 4, 6, 12, 25, 47, 92, 195};
  const static uint16_t _16frequencyOffsets[16] = {1, 1, 2, 2, 2, 4, 5, 7, 11, 14, 19, 28, 38, 54, 75, 120}; // initial 
  // 32 and 64 frequency offsets are low end biased because of int math... the first 4 and 8 buckets should be 0.5f but we cant do that here.
  const static uint16_t _32frequencyOffsets[32] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 5, 5, 7, 7, 8, 8, 14, 14, 19, 19, 27, 27, 37, 37, 60, 60};
  const static uint16_t _64frequencyOffsets[64] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 7, 7, 7, 7, 8, 8, 8, 8, 13, 13, 13, 13, 18, 18, 18, 18, 30, 30, 30, 30}; // low end biased because of int
  const uint16_t *_frequencyOffsets;
try_frequency_offsets_again:
  switch (bandSize)
  {
  case 2:
    _frequencyOffsets = _2frequencyOffsets;
    break;
  case 4:
    _frequencyOffsets = _4frequencyOffsets;
    break;
  case 8:
    _frequencyOffsets = _8frequencyOffsets;
    break;
  case 16:
    _frequencyOffsets = _16frequencyOffsets;
    break;
  case 32:
    _frequencyOffsets = _32frequencyOffsets;
    break;
  case 64:
    _frequencyOffsets = _64frequencyOffsets;
    break;
  default:
    bandSize = BAND_SIZE;
    goto try_frequency_offsets_again;
  }
  _bandSize = bandSize;
  _isClipping = false;
  // for normalize falloff rates
  if (_isAutoLevel)
  {
    if (_autoLevelPeakMax > _autoMin)
    {
      _autoLevelPeakMaxFalloffRate = calculateFalloff(_autoLevelFalloffType, _autoLevelFalloffRate, _autoLevelPeakMaxFalloffRate);
      _autoLevelPeakMax -= _autoLevelPeakMaxFalloffRate;
    }
    if (_autoLevelVuPeakMax > _autoMin * 1.5)
    {
      _autoLevelMaxFalloffRate = calculateFalloff(_autoLevelFalloffType, _autoLevelFalloffRate, _autoLevelMaxFalloffRate);
      _autoLevelVuPeakMax -= _autoLevelMaxFalloffRate;
    }
  }
  _vu = 0;
  _bandMax = 0;
  _bandAvg = 0;
  _peakAvg = 0;
  _bandMaxIndex = -1;
  _bandMinIndex = -1;
  _peakMaxIndex = -1;
  _peakMinIndex = -1;
  int offset = 2; // first two values are noise
  for (int i = 0; i < _bandSize; i++)
  {
    _bands[i] = 0;
    // handle band peaks fall off
    _peakFallRate[i] = calculateFalloff(_bandPeakFalloffType, _bandPeakFalloffRate, _peakFallRate[i]);
    if (_peaks[i] - _peakFallRate[i] <= _bands[i])
    {
      _peaks[i] = _bands[i];
    }
    else
    {
      _peaks[i] -= _peakFallRate[i]; // fall off rate
    }
    for (int j = 0; j < _frequencyOffsets[i]; j++)
    {
      // scale down factor to prevent overflow
      int rv = (_real[offset + j] / (0xFFFF * 0xFF));
      int iv = (_imag[offset + j] / (0xFFFF * 0xFF));
      // some smoothing with imaginary numbers.
      rv = sqrt(rv * rv + iv * iv);
      // add eq offsets
      rv = rv * _bandEq[i];
      // combine band amplitudes for current band segment
      _bands[i] += rv;
      _vu += rv;
    }
    offset += _frequencyOffsets[i];

    // remove noise
    if (_bands[i] < _noiseFloor)
    {
      _bands[i] = 0;
    }

    if (_bands[i] > _peaks[i])
    {
      _peakFallRate[i] = 0;
      _peaks[i] = _bands[i];
    }

    // handle min/max band
    if (_bands[i] > _bandMax && _bands[i] > _noiseFloor)
    {
      _bandMax = _bands[i];
      _bandMaxIndex = i;
    }
    if (_bands[i] < _bandMin)
    {
      _bandMin = _bands[i];
      _bandMinIndex = i;
    }
    // handle min/max peak
    if (_peaks[i] > _autoLevelPeakMax)
    {
      _autoLevelPeakMax = _peaks[i];
      if (_isAutoLevel && _autoMax != -1 && _peaks[i] > _autoMax)
      {
        _isClipping = true;
        _autoLevelPeakMax = _autoMax;
      }
      _peakMaxIndex = i;
      _autoLevelPeakMaxFalloffRate = 0;
    }
    if (_peaks[i] < _peakMin && _peaks[i] > _noiseFloor)
    {
      _peakMin = _peaks[i];
      _peakMinIndex = i;
    }

    // handle band average
    _bandAvg += _bands[i];
    _peakAvg += _peaks[i];
  } // end bands
  // handle band average
  _bandAvg = _bandAvg / _bandSize;
  _peakAvg = _peakAvg / _bandSize;

  // handle vu peak fall off
  _vu = _vu / 8.0; // get it closer to the band peak values
  _vuPeakFallRate = calculateFalloff(_vuPeakFalloffType, _vuPeakFalloffRate, _vuPeakFallRate);
  _vuPeak -= _vuPeakFallRate;
  if (_vu > _vuPeak)
  {
    _vuPeakFallRate = 0;
    _vuPeak = _vu;
  }
  if (_vu > _vuMax)
  {
    _vuMax = _vu;
  }
  if (_vu < _vuMin)
  {
    _vuMin = _vu;
  }
  if (_vuPeak > _autoLevelVuPeakMax)
  {
    _autoLevelVuPeakMax = _vuPeak;
    if (_isAutoLevel && _autoMax != -1 && _vuPeak > _autoMax)
    {
      _isClipping = true;
      _autoLevelVuPeakMax = _autoMax;
    }
    _autoLevelMaxFalloffRate = 0;
  }
  if (_vuPeak < _vuPeakMin)
  {
    _vuPeakMin = _vuPeak;
  }
}

template <class X>
X AudioAnalysis::mapAndClip(X x, X in_min, X in_max, X out_min, X out_max)
{
  if (_isAutoLevel && _autoMax != -1 && x > _autoMax)
  {
    // clip the value to max
    x = _autoMax;
  }
  else if (x > in_max)
  {
    // value is clipping
    x = in_max;
  }
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void AudioAnalysis::normalize(bool normalize, float min, float max)
{
  _isNormalize = normalize;
  _normalMin = min;
  _normalMax = max;
}
void AudioAnalysis::bandPeakFalloff(falloff_type falloffType, float falloffRate)
{
  _bandPeakFalloffType = falloffType;
  _bandPeakFalloffRate = falloffRate;
}

void AudioAnalysis::vuPeakFalloff(falloff_type falloffType, float falloffRate)
{
  _vuPeakFalloffType = falloffType;
  _vuPeakFalloffRate = falloffRate;
}

float AudioAnalysis::calculateFalloff(falloff_type falloffType, float falloffRate, float currentRate)
{
  switch (falloffType)
  {
  case LINEAR_FALLOFF:
    return falloffRate;
  case ACCELERATE_FALLOFF:
    return currentRate + falloffRate;
  case EXPONENTIAL_FALLOFF:
    if (currentRate == 0)
    {
      currentRate = falloffRate;
    }
    return currentRate + currentRate;
  case NO_FALLOFF:
  default:
    return 0;
  }
}

void AudioAnalysis::autoLevel(falloff_type falloffType, float falloffRate, float min, float max)
{
  _isAutoLevel = falloffType != NO_FALLOFF;
  _autoLevelFalloffType = falloffType;
  _autoLevelFalloffRate = falloffRate;
  _autoMin = min;
  _autoMax = max;
}

bool AudioAnalysis::isNormalize()
{
  return _isNormalize;
}

bool AudioAnalysis::isAutoLevel()
{
  return _isAutoLevel;
}

bool AudioAnalysis::isClipping()
{
  return _isClipping;
}

float *AudioAnalysis::getBands()
{
  if (_isNormalize)
  {
    for (int i = 0; i < _bandSize; i++)
    {
      _bandsNorms[i] = mapAndClip(_bands[i], 0.0f, _autoLevelPeakMax, _normalMin, _normalMax);
    }
    return _bandsNorms;
  }
  return _bands;
}

float AudioAnalysis::getBand(uint8_t index)
{
  if (index >= _bandSize || index < 0)
  {
    return 0;
  }
  if (_isNormalize)
  {
    return mapAndClip(_bands[index], 0.0f, _autoLevelPeakMax, _normalMin, _normalMax);
  }
  return _bands[index];
}

float AudioAnalysis::getBandAvg()
{
  if (_isNormalize)
  {
    return mapAndClip(_bandAvg, 0.0f, _autoLevelPeakMax, _normalMin, _normalMax);
  }
  return _bandAvg;
}

float AudioAnalysis::getBandMax()
{
  return getBand(getBandMaxIndex());
}

int AudioAnalysis::getBandMaxIndex()
{
  return _bandMaxIndex;
}

int AudioAnalysis::getBandMinIndex()
{
  return _bandMinIndex;
}

float *AudioAnalysis::getPeaks()
{
  if (_isNormalize)
  {
    for (int i = 0; i < _bandSize; i++)
    {
      _peaksNorms[i] = mapAndClip(_peaks[i], 0.0f, _autoLevelPeakMax, _normalMin, _normalMax);
    }
    return _peaksNorms;
  }
  return _peaks;
}

float AudioAnalysis::getPeak(uint8_t index)
{
  if (index >= _bandSize || index < 0)
  {
    return 0;
  }
  if (_isNormalize)
  {
    return mapAndClip(_peaks[index], 0.0f, _autoLevelPeakMax, _normalMin, _normalMax);
  }
  return _peaks[index];
}

float AudioAnalysis::getPeakAvg()
{
  if (_isNormalize)
  {
    return mapAndClip(_peakAvg, 0.0f, _autoLevelPeakMax, _normalMin, _normalMax);
  }
  return _peakAvg;
}

float AudioAnalysis::getPeakMax()
{
  return getPeak(getPeakMaxIndex());
}

int AudioAnalysis::getPeakMaxIndex()
{
  return _peakMaxIndex;
}

int AudioAnalysis::getPeakMinIndex()
{
  return _peakMinIndex;
}

float AudioAnalysis::getVolumeUnit()
{
  if (_isNormalize)
  {
    return mapAndClip(_vu, 0.0f, _autoLevelVuPeakMax, _normalMin, _normalMax);
  }
  return _vu;
}

float AudioAnalysis::getVolumeUnitPeak()
{
  if (_isNormalize)
  {
    return mapAndClip(_vuPeak, 0.0f, _autoLevelVuPeakMax, _normalMin, _normalMax);
  }
  return _vuPeak;
}

float AudioAnalysis::getVolumeUnitMax()
{
  if (_isNormalize)
  {
    return mapAndClip(_vuMax, 0.0f, _autoLevelVuPeakMax, _normalMin, _normalMax);
  }
  return _vuMax;
}

float AudioAnalysis::getVolumeUnitPeakMax()
{
  if (_isNormalize)
  {
    return _normalMax;
  }
  return _autoLevelVuPeakMax;
}