#include <uWS/uWS.h>
#include <iostream>
#include "json.hpp"
#include "PID.h"
#include <cmath>
#include <vector>
#include <chrono> // clock
typedef std::chrono::high_resolution_clock Clock;


// PID twiddle tune setting
// change pidTuning to true if you want to run twiddle pid search
const bool pidTuning = false;
int n_try_tuning = 0;
double eps_tuning = 0.0;
const double Kp_tw = 0.05;
const double Kd_tw = 0.05;
const double Ki_tw = 0.001;

// pid steering
const double Kp_s = 0.12;
const double Kd_s = 0.05;
const double Ki_s = 0.002;


// pid throttle
const double Kp_t = 0.100;
const double Kd_t = 0.100;
const double Ki_t = 0.001;

// max output limit
const double max_steering =  1.0;   // max output steering (radian)
const double min_steering = -1.0;   // min
const double max_throttle =  0.3;   // max output throttle (mph): acceleration
const double min_throttle =  0.0;   // min (no brake, just lift off the throttle)

// running speed
enum speed_mph {SPEED_1=15, SPEED_2=20, SPEED_3=30};
double run_speed = static_cast<double>(speed_mph::SPEED_1);


// for convenience
using json = nlohmann::json;

// For converting back and forth between radians and degrees.
constexpr double pi() { return M_PI; }
double deg2rad(double x) { return x * pi() / 180; }
double rad2deg(double x) { return x * 180 / pi(); }
double normalise_angle(double angle) { return atan2(sin(angle),cos(angle)); }

// Checks if the SocketIO event has JSON data.
// If there is data the JSON object in string format will be returned,
// else the empty string "" will be returned.
std::string hasData(std::string s) {
  auto found_null = s.find("null");
  auto b1 = s.find_first_of("[");
  auto b2 = s.find_last_of("]");
  if (found_null != std::string::npos) {
    return "";
  }
  else if (b1 != std::string::npos && b2 != std::string::npos) {
    return s.substr(b1, b2 - b1 + 1);
  }
  return "";
}

int main()
{
  uWS::Hub h;
  // Initialize the pid variable.
  PID pid_steering;
  pid_steering.Init(Kp_s, Ki_s, Kd_s, max_steering, min_steering);

  PID pid_speed;
  pid_speed.Init(Kp_t, Ki_t, Kd_t, max_throttle, min_throttle);

  // loop count
  static int counter = 0;

  // system timer
  auto new_msg = Clock::now();
  auto old_msg = Clock::now();

  h.onMessage([&pid_steering, &pid_speed, &new_msg, &old_msg](uWS::WebSocket<uWS::SERVER> ws, char *data, size_t length, uWS::OpCode opCode) {
    // "42" at the start of the message means there's a websocket message event.
    // The 4 signifies a websocket message
    // The 2 signifies a websocket event
    if (length && length > 2 && data[0] == '4' && data[1] == '2')
    {
      auto s = hasData(std::string(data).substr(0, length));
      if (s != "") 
      {
        auto j = json::parse(s);
        std::string event = j[0].get<std::string>();
        if (event == "telemetry") 
        {
          // j[1] is the data JSON object
          double cte = std::stod(j[1]["cte"].get<std::string>());
          double speed = std::stod(j[1]["speed"].get<std::string>());
          double angle = std::stod(j[1]["steering_angle"].get<std::string>());

          // init pid variables
          double steer_value;
          double throttle;

          new_msg = Clock::now();
          double dt = (std::chrono::duration_cast<std::chrono::milliseconds>(new_msg - old_msg).count())/1000.0;
          old_msg = new_msg;
          std::cout << "dt: " << dt << std::endl; 
          
          // pid running time step
          if(dt > 0.01)
          {  
	
			  /* -------------*/
			  /* Steering PID */
			  /* -------------*/
			  // update pid error
			  pid_steering.UpdateError(cte, dt);
			  // compute steering output
			  steer_value = pid_steering.TotalError();
			  // limit angle to e.g. [-1,1] radian about 60 degrees
			  if(steer_value > max_steering){
				steer_value = max_steering;
			  }
			  else if(steer_value < min_steering){
				steer_value = min_steering;
			  }

			  //std::cout << "run loop : " << counter << std::endl;
			  counter++;


			  /* --------------*/
			  /*    Speed PID  */
			  /* --------------*/
			  // 1- set target speed cruise to X mph- use pid to maintain output throttle
			  //    note: without this, the speed may go beyond 30 mph, problem during cornering
			  //    simply set low target speed and control it with throttle pid
			  //    this method is easy to work with
			  //    if cte > 0.3, it is time to reduce speed
			  double set_speed;                                			// mph
			  counter < 200?set_speed = 15.0:set_speed = run_speed; 	// start slow (let pid to settle a bit) or using cte to check
			  const double speed_err = speed - set_speed;      			// speed error
			  // update pid error
			  pid_speed.UpdateError(speed_err, dt);
			  // compute steering output
			  throttle = pid_speed.TotalError();
			  // limit throttle to 0.35 max gas input (higher the faster to get up to speed but overshot may happen)
			  if(throttle > max_throttle){
				throttle = max_throttle;
			  }
			  else if(throttle < min_throttle){
				  throttle = min_throttle;
			  }

			  // about to swing at high speed error- no more speed please
			  if(fabs(cte) > 0.3 && speed > 30.00) {
				  throttle = 0.0;
			  }
			  
              //
			  // PID Tuning Twiddle:
			  //
			  if(pidTuning){
				 std::vector<double> kpid;
				 eps_tuning = pid_steering.dp[0]+pid_steering.dp[1]+pid_steering.dp[2];
				 // set how many times to run e.g. 10 tries or until dp doesn't change any more
				 if(pid_steering.twiddle_n < 10 and eps_tuning > 0.00001){
					kpid = pid_steering.TunePIDTwiddle(cte);
				 }
				 else
				 {
				    std::cout << "Done tuning Kp,Kd,Ki =  " << std::endl;
					std::cout << pid_steering.Kp  << std::endl;
					std::cout << pid_steering.Kd  << std::endl;
					std::cout << pid_steering.Ki  << std::endl;
				 }

		      }// end pid tuning

              // DEBUG- speed
              //std::cout << "throttle: " << throttle << " speed: " << speed << std::endl;

              // DEBUG- steering
              std::cout << "CTE: " << cte << " Current steering: " << angle << " Cmd Steering: " << steer_value << std::endl;

              json msgJson;
              msgJson["steering_angle"] = steer_value;
              msgJson["throttle"] = throttle;
              auto msg = "42[\"steer\"," + msgJson.dump() + "]";

              // DEBUD- msg
              //std::cout << msg << std::endl;
              ws.send(msg.data(), msg.length(), uWS::OpCode::TEXT);
              

	 	    
          } // if dt > 0
          
        }// end if message telemetry
      
      } // end if message " "
      else {
        // Manual driving
        std::string msg = "42[\"manual\",{}]";
        ws.send(msg.data(), msg.length(), uWS::OpCode::TEXT);
      }// end else message manual
      
    }
  });// end on message

  // We don't need this since we're not using HTTP but if it's removed the program
  // doesn't compile :-(
  h.onHttpRequest([](uWS::HttpResponse *res, uWS::HttpRequest req, char *data, size_t, size_t) {
    const std::string s = "<h1>Hello world!</h1>";
    if (req.getUrl().valueLength == 1)
    {
      res->end(s.data(), s.length());
    }
    else
    {
      // i guess this should be done more gracefully?
      res->end(nullptr, 0);
    }
  });

  h.onConnection([&h](uWS::WebSocket<uWS::SERVER> ws, uWS::HttpRequest req) {
    std::cout << "Connected!!!" << std::endl;
  });

  h.onDisconnection([&h](uWS::WebSocket<uWS::SERVER> ws, int code, char *message, size_t length) {
    ws.close();
    std::cout << "Disconnected" << std::endl;
  });

  int port = 4567;
  if (h.listen(port))
  {
    std::cout << "Listening to port " << port << std::endl;
  }
  else
  {
    std::cerr << "Failed to listen to port" << std::endl;
    return -1;
  }
  h.run();
}
