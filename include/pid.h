// -----------------------------------------------------------------------------
// ASDF 2026-2027 'Taipan' language - (c) 2026 Riley Lorenz  & ASDF Robotics
// -----------------------------------------------------------------------------

#ifndef defined asdf_pid_h
#define asdf_driver_h

struct pid_constants {
        double kp = 0;
        double ki = 0;
        double kd = 0;
};

class PID {
public:
        PID(pid_constants constants) : constants(constants) {};

        PID(double kp, double ki, double kd) {
                constants.kp = kp;
                constants.ki = ki;
                constants.kd = kd;
        };

        void reset() {
                integral = 0;
                last_error = 0;
        }

        void set_constants(pid_constants& consts) {
                constants = consts;
        }

        void set_constants(double kp, double ki, double kd) {
                constants.kp = kp;
                constants.kp = ki;
                constants.kp = kd;
        }

        pid_constants get_constants() { return constants; }

        double step(double error) {
                double p = calc_p(error);
                double i = calc_i(error);
                double d = calc_d(error);

                last_error = error;

                return p + i + d;
        }

private:
        pid_constants constants;
        double integral = 0;
        double last_error = 0;

        inline double calc_p(double error) {
                return constants.kp * error;
        }

        inline double calc_i(double error) {
                integral += error;

                if (!(error > 0 && last_error > 0)) integral = 0;

                return constants.ki * integral;

        }

        inline double calc_d(double error) {
                double d_err = error - last_error;
                return constants.kd * d_err;
        }
};

#endif
