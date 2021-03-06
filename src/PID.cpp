#include "PID.h"
#include <iostream>
using namespace std;

/*
* TODO: Complete the PID class.
*/

PID::PID() {}

PID::~PID() {}

void PID::Init(double Kp, double Ki, double Kd, double MaxLimit, double MinLimit) {
  PID::Kp = Kp;
  PID::Ki = Ki;
  PID::Kd = Kd;
  PID::d_error = 0.0;
  PID::i_error = 0.0;
  PID::p_error = 0.0;
  PID::output_max = MaxLimit;
  PID::output_min = MinLimit;

  // init pid tuning only
  PID::p = {PID::Kp, PID::Kd, PID::Ki};    //note: Kp, Kd, Ki
}

void PID::UpdateError(double cte, double dt) {
  d_error = (cte - p_error)/dt;
  i_error += cte;
  p_error = cte;
}

double PID::TotalError() {
  /*
   *  PID integral windup - clamping output maximum/minimum
   *  Normal pid approach works without this
   */

  double output = -Kp * p_error  - Kd * d_error - Ki* i_error;

  if (output > output_max){
    i_error -= output - output_max;
    output = output_max;
  }
  else if (output < output_min){
    i_error += output_min - output;
    output = output_min;
  }

  return output;
}

void PID::restartPID(double Kp, double Ki, double Kd) {
  Init(Kp,Ki,Kd, output_max, output_min);
  now_rms_err = 0.0;
  std::cout << "Set new PID " << Kp << " " << Ki << " " << Kd << " " << std::endl;
}

std::vector<double> PID::TunePIDTwiddle(const double err){

  if(pid_run_n < pid_run_max){
      // keep running on this current P,I,D to check rms error on this pid
      // record total mean error so far, let it settle a bit
	  if(pid_run_n > 0.25*pid_run_max)
	    now_rms_err += (err*err);
	  pid_run_n++;
  }
  else{


	 pid_run_n = 0;
	 // compute average rms
	 now_rms_err /= ( 0.75*(double)(pid_run_max) );
	 twiddle_state how_am_i_doing = DOING_BAD;
	 if(now_rms_err < best_rms_err){
	   best_rms_err = now_rms_err;
	   how_am_i_doing = DOING_GOOD;
	 }

	 std::cout << "Running Twiddle... " << twiddle_n << " rms result  " << now_rms_err << std::endl;


     // time to try another P,I,D
	 // state engine: check what we did previously
	 // state start: then dp up
	 // state dp[i] up: if good, then in the next try dp[i] up again, else dp down
	 // state dp[i] down: if good, then in the next try dp[i] down again, else dp stay
	 // dp[i] stay: make dp smaller and start messing the next gain e.g. i+1

	 // Sequential logic: start->go up(restart?)->go down -> restart
	 switch(static_cast<int>(dp_state)){
	   // at start of tuning
	   case DP_START:
		   // try fiddling with the current i gain
		   p[pid_i] += dp[pid_i];
		   dp_state = DP_UP;
		   restartPID(p[0],p[2],p[1]);
		   break;

       // was at dp up
	   case DP_UP:
		   if(how_am_i_doing == DOING_GOOD){
		     // doing good with dp up- we are done with this gain for now
		     dp[pid_i] *= 1.1;
		     // skip to the next gain and restart
		     dp_state = DP_START;
		     pid_i = (pid_i+1)%3;

		   }
		   else{
			 // doing bad- try other way
			 dp_state = DP_DOWN;
			 p[pid_i] -= 2*dp[pid_i];
			 restartPID(p[0],p[2],p[1]);
		   }
		   break;

       // was at dp down
	   case DP_DOWN:
		   if(how_am_i_doing == DOING_GOOD){
			 // doing good with dp down- we are done with this gain for now
			 // increase dp and skip to the next gain and restart
			 dp[pid_i] *= 1.1;

		   }
		   else{
			 // when we reach here that means nothing we try was good
			 // so go back to where we were and make this step smaller next time we try on this  gain
			 p[pid_i] += dp[pid_i];
			 dp[pid_i] *= 0.9;

		   }

		   dp_state = DP_START;
		   pid_i = (pid_i+1)%3;
		   break;

	 }

     // keep track a number twiddle tuning run
	 if(pid_i == 0)
	 {
	   twiddle_n++;
	 }

     // Debug
	 std::cout << "new esp dp " << dp[0] << " " << dp[2] << " " << dp[1] << " " << std::endl;

  }

  return p;
}
