#ifndef QUATERNION_TO_RPY_H
#define QUATERNION_TO_RPY_H

#include "PlotJuggler/transform_function.h"

class QuaternionToRollPitchYaw : public PJ::TransformFunction
{
public:
  QuaternionToRollPitchYaw();

  const char* name() const override
  {
    return "quaternion_to_RPY";
  }

  void reset() override;

  int numInputs() const override
  {
    return 4;
  }

  int numOutputs() const override
  {
    return 3;
  }

  void setScale(double scale)
  {
    _scale = scale;
  }

  void setWarp(bool wrap)
  {
    _wrap = wrap;
  }

  void calculate() override;

  void calculateNextPoint(size_t index, const std::array<double, 4>& quat,
                          std::array<double, 3>& rpy);

private:
  double _prev_roll = 0;
  double _prev_yaw = 0;
  double _prev_pitch = 0;
  double _roll_offset = 0;
  double _pitch_offset = 0;
  double _yaw_offset = 0;
  double _scale = 1.0;
  bool _wrap = true;
  double _last_timestamp = std::numeric_limits<double>::lowest();
};

#endif  // QUATERNION_TO_RPY_H
