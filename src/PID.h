#ifndef PID_H
#define PID_H


#include <vector>

class PID {
public:
  /*
  * Errors
  */
  double p_error;
  double i_error;
  double d_error;

  /*
  * Coefficients
  */ 
  double Kp;
  double Ki;
  double Kd;

  /*
   * Output maximum limit
   */
  double output_max;
  double output_min;

  /*
  * Constructor
  */
  PID();

  /*
  * Destructor.
  */
  virtual ~PID();

  /*
  * Initialize PID.
  */
  void Init(double Kp, double Ki, double Kd, double MaxLimit, double MinLimit);

  /*
  * Update the PID error variables given cross track error.
  */
  void UpdateError(double cte, double dt);

  /*
  * Calculate the total PID error.
  */
  double TotalError();


  /*
   *  Reset PID errors and Gain (only for tuning)
   */
  void restartPID(double Kp, double Ki, double Kd);

  /*
   * Tuning pid using Twiddle
   */
  std::vector<double> TunePIDTwiddle(const double error);

  /*
   * Tuning Params
   */

  std::vector<double> dp = {0.01,0.01,0.001};            	//note: Kp, Kd, Ki
  std::vector<double> p;    								//note: Kp, Kd, Ki
  int pid_i = 0;
  double best_rms_err = 1e3;
  double now_rms_err = 0.0;
  const int pid_run_max = 400;
  int pid_run_n = 0;
  int twiddle_n = 0;
  enum twiddle_state {DP_START=1, DP_UP, DP_DOWN,DOING_BAD,DOING_GOOD};
  twiddle_state dp_state = twiddle_state::DP_START;

};

#endif /* PID_H */
