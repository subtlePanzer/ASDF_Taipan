// -----------------------------------------------------------------------------
// ASDF 2027 'Taipan' language - (c) 2026 Riley Lorenz  & ASDF Robotics
// -----------------------------------------------------------------------------

#include "main.h"

extern "C" {
#include "api.h"
#include "auton.h"
#include "dead_reckoning.h"
#include "driver.h"
#include "hardware.h"
#include "object.h"
#include "tp_main.h"
#include "vm.h"
};

// #include "IPositionable.hpp"
// #include "navigator.hpp"
#include "navigator.hpp"
#include "odom.hpp"
#include <fstream>
#include <functional>

tracking_wheel lat_wheel = tracking_wheel(odom_perp, 25.4f, -46.97f); // TODO: check offsets
tracking_wheel para_wheel = tracking_wheel(odom_para, 25.4f, 15.0f);

straight_odom_2_wheel odom_sys(lat_wheel, para_wheel);

#ifdef USE_TAIPAN
run_script_params* params;
#endif

std::vector<vec2> odom_buffer;

static void flush_odom_to_sd() {
#ifdef SAVE_ODOM_TRACK
        if (pros::usd::is_installed() && !odom_buffer.empty()) {
                FILE* odom_log_file = fopen("/usd/odom.log", "w");

                if (odom_log_file != nullptr) {
                        for (const auto& pt : odom_buffer) {
                                std::string line = std::format("{:.2f}, {:.2f}\n", pt.x, pt.y);
                                fputs(line.c_str(), odom_log_file);
                        }
                        fclose(odom_log_file);
                }

                // odom_buffer.clear();
        }
#endif
}

static void odom_task_wrapper() {
#ifdef SAVE_ODOM_TRACK
        std::fstream file_strm;
#endif
        pros::Controller mainctrl(CONTROLLER_MASTER);
        pros::IMU imui(imu_port);

        do {
                heading.store(imui.get_heading());
                odom_sys.calc_position();

                vec2 pos = odom_sys.get_position();

                curr_x.store(pos.x);
                curr_y.store(pos.y);
                // printf("Position: %f %f\n", pos.x, pos.y);
                // printf("Position: %f %f\n", pos.x, pos.y);
                mainctrl.print(0, 0, "POS: %.2f %.2f", pos.x, pos.y);
                // mainctrl.print(0, 0, "HD: %.2f", heading.load());
#ifdef SAVE_ODOM_TRACK
                odom_buffer.push_back(pos);

                if (mainctrl.get_digital_new_press(pros::E_CONTROLLER_DIGITAL_X)) {
                        flush_odom_to_sd();
                }
#endif

                pros::delay(5);
        } while (1);
}

void initialize() {
#ifdef USE_INI_LOADER
        // Pull ini files
        get_config(); // TODO: Cache values to files
#endif

        // robot
        init_hardware();

#ifdef USE_TAIPAN
        // Taipan
        std::atomic_init(&abort_auton, false);
        std::atomic_init(&vm_cleanup_done, false);

        params = new run_script_params();
        params->func = init_tp();
        params->interrupt = &abort_auton;
        params->vm_cleanup_atomic = &vm_cleanup_done;

#endif
        calibrate_imu();

        lat_wheel.init_tracking_wheel();
        para_wheel.init_tracking_wheel();

        pros::Task odom_tracker_task(odom_task_wrapper, "ODOM TRACKER");
        printf("x: %f y: %f\n", odom_sys.get_position().x, odom_sys.get_position().y);
        // exit(0);

        pros::screen::print(TEXT_MEDIUM, 0, "Initialised");
        pros::delay(300);
        pros::screen::erase();
}

void disabled() {

}

void competition_initialize() {

}

#ifdef USE_TAIPAN
void delay_thrd(void* main_task_handle) {
        pros::task_t main = (pros::task_t)main_task_handle;

        int millis_start = pros::millis();
        while (pros::millis() - millis_start < 5'000 && !abort_auton.load()) {
                pros::delay(25);
        }

        printf("5 second mark.\n");

        millis_start = pros::millis();
        while (pros::millis() - millis_start < 5'000 && !abort_auton.load()) {
                pros::delay(25);
        }

        printf("10 second mark.\n");

        millis_start = pros::millis();
        while (pros::millis() - millis_start < 4'950 && !abort_auton.load()) {
                pros::delay(25);
        } // leave time for cleanup

        printf("15 second mark.\n");
        printf("AUTON TIME PERIOD FINISHED.\n");
        printf("Script aborted.\n");
        pros::c::task_notify(main);
}
#endif

#include "auton.h"

void autonomous() {
        pros::screen::print(pros::E_TEXT_MEDIUM, 0, "AUTON");

        printf("Tasks: %d\n", pros::Task::get_count());

        pros::Task dead_reckoning_task(dead_reckoning_position_tracker, "AUTON DEAD RECKON / POS TRACKER");
#ifdef USE_TAIPAN
        if (params->func != NULL) {
                printf("Running... \n");
                abort_auton.store(false);

                pros::task_t main_task = pros::c::task_get_current();

                params->main_handle = &main_task;

                printf("Delegating to runtime\n");
                pros::Task taipan_runtime_task(run_script, params, "TAIPAN RUNTIME");
                pros::Task taipan_runtime_aborter_task(delay_thrd, (void*)main_task, "TAIPAN RUNTIME ABORTER");
                pros::c::task_notify_take(true, TIMEOUT_MAX);

                abort_auton.store(true);

                while (!vm_cleanup_done.load()) {
                        pros::delay(10); // wait until vm is ready for completion
                }

                if (!atomic_load(&abort_auton))
                        printf("Script finished.\n");
        } else {
                printf("No script found or compilation failed. Please review error logs.\n");
#endif

                // p2p point2point_nav;
                // point2point_nav.position_source = &odom_sys;

                // point2point_nav.set_target(vec2(100, 100));
                // pros::Task p2p_task([&point2point_nav]() {point2point_nav.navigate_thread();}, "P2P");


                // while (!point2point_nav.check_is_finished()) {
                //         vec2 pos = odom_sys.get_position();
                //         printf("%f %f\n", pos.x, pos.y);

                //         pros::delay(10);
                // }

                // p2p_task.join();


                // LL P2P: Initial setup

                init_claw();
                toggle_claw();

                struct p2p_params ip2p_params;
                struct within_range_data lat_range_data;
                initialise_within_range_data(&lat_range_data);
                lat_range_data.range = 1.0f; // if within 10 mms for 100 msec  //TODO:
                lat_range_data.time = 100.0f; //TODO:

                ip2p_params.lat_exit_range_data = &lat_range_data;
                ip2p_params.exit_range = 2.0; //TODO:

                ip2p_params.max_lat_speed = 0.8; //TODO:
                ip2p_params.min_lat_speed = 0.05; //TODO:
                ip2p_params.max_ang_speed = 0.7; //TODO:
                ip2p_params.min_ang_speed = 0.02; //TODO:

                struct pid_data lat_pid;
                struct pid_data ang_pid;

                initialise_pid_data(&lat_pid);
                initialise_pid_data(&ang_pid);

                lat_pid.kp = 4.0; //TODO:
                lat_pid.ki = 0.0; //TODO:
                lat_pid.kd = 1.5; //TODO:
                lat_pid.anti_windup_range = 10.0f; //TODO:

                ang_pid.kp = 1.8; //TODO:
                ang_pid.ki = 0.0; //TODO:
                ang_pid.kd = 0.5; //TODO:
                ang_pid.anti_windup_range = 15.0f; //TODO:

                ip2p_params.lat_pid = &lat_pid;
                ip2p_params.ang_pid = &ang_pid;

                ip2p_params.lat_slew = .5f; //TODO:
                ip2p_params.ang_slew = .3f; //TODO:

                ip2p_params.dt = 10.0f; // millis

                struct turn_params auton_turn_params;

                struct within_range_data turn_range_data;
                initialise_within_range_data(&turn_range_data);
                turn_range_data.range = 1.5f;
                turn_range_data.time = 100.0f;

                auton_turn_params.turn_exit_range_data = &turn_range_data;
                auton_turn_params.exit_range = 2.0;
                auton_turn_params.max_speed = 0.7;
                auton_turn_params.min_speed = 0.0;

                struct pid_data turn_pid;
                initialise_pid_data(&turn_pid);
                turn_pid.kp = 3.2;
                turn_pid.ki = 0.0;
                turn_pid.kd = 0.4;
                turn_pid.anti_windup_range = 20.0f;

                auton_turn_params.turn_pid = &turn_pid;

                auton_turn_params.slew = 0.4f;
                auton_turn_params.dt = 10.0f;

                set_motors_brake();
                // toggle_claw();

                // LL P2P: Set these per movement!
                struct auton_command_data ip2p_command;
                ip2p_command.timeout_millis = 1200.0f;
                ip2p_command.reverse = false;

                // Do the P2P
                improved_point_to_point(vec2(0, 300), ip2p_command, ip2p_params);

                // deploy matchloader
                temp_c_spin_motor(motor_lift_a, -110);
                temp_c_spin_motor(motor_lift_b, -110);
                pros::delay(1400);
                temp_c_spin_motor(motor_lift_a, 100);
                temp_c_spin_motor(motor_lift_b, 100);
                pros::delay(1200);
                temp_c_spin_motor(motor_lift_a, 0);
                temp_c_spin_motor(motor_lift_b, 0);

                pros::delay(600);

                // Toggle
                ip2p_command.timeout_millis = 800.0f;
                ip2p_command.reverse = true;
                improved_point_to_point(vec2(0, -120), ip2p_command, ip2p_params);
                ip2p_command.reverse = false;
                improved_point_to_point(vec2(0, 150), ip2p_command, ip2p_params);
                ip2p_command.reverse = true;
                improved_point_to_point(vec2(0, -120), ip2p_command, ip2p_params);

                ip2p_command.reverse = false;
                improved_point_to_point(vec2(0, 200), ip2p_command, ip2p_params);


                // // // Score preload
                // ip2p_command.timeout_millis = 5000.0f;

                // struct auton_turn_target turn_target;
                // turn_target.as.heading = -90.0f;
                // turn_target.is_position = false;
                // turn_target.lock_side = LOCKED_NONE;
                // struct auton_command_data turn_cmd;
                // turn_cmd.reverse = false;
                // turn_cmd.timeout_millis = 1000.0f; 

                // improved_auton_turn(turn_target, turn_cmd, auton_turn_params);

                // temp_c_spin_motor(motor_lift_a, -100);
                // temp_c_spin_motor(motor_lift_b, -100);
                // pros::delay(900);
                // temp_c_brake_motor(motor_lift_a);
                // temp_c_brake_motor(motor_lift_b);

                // ip2p_command.timeout_millis = 1500.0f;
                // improved_point_to_point(vec2(-280, 200), ip2p_command, ip2p_params);

                // pros::delay(300);
                // temp_c_spin_motor(motor_lift_a, 80);
                // temp_c_spin_motor(motor_lift_b, 80);
                // pros::delay(400);
                // temp_c_brake_motor(motor_lift_a);
                // temp_c_brake_motor(motor_lift_b);

                // toggle_claw();

                // ip2p_command.reverse = true;
                // improved_point_to_point(vec2(100, 200), ip2p_command, ip2p_params);

                // ip2p_command.reverse = false;

                // turn_target.as.heading = 230.0f;

                // improved_auton_turn(turn_target, turn_cmd, auton_turn_params);
                // ip2p_command.timeout_millis = 2500.0f;
                // improved_point_to_point(vec2(-300, 600), ip2p_command, ip2p_params);

                // toggle_claw();

                // turn_target.as.heading = -45.0f;
                // turn_target.is_position = false;
                // turn_target.lock_side = LOCKED_NONE;
                // turn_cmd.reverse = false;
                // turn_cmd.timeout_millis = 1200.0f; 
                // improved_auton_turn(turn_target, turn_cmd, auton_turn_params);

#ifdef EMERGENCY
                auton_point_to_point(vec2(0, 150), false);

                // deploy matchloader
                temp_c_spin_motor(motor_lift_a, -100);
                temp_c_spin_motor(motor_lift_b, -100);
                pros::delay(1200);
                temp_c_spin_motor(motor_lift_a, -30);
                temp_c_spin_motor(motor_lift_b, -30);
                pros::delay(600);
                temp_c_spin_motor(motor_lift_a, 0);
                temp_c_spin_motor(motor_lift_b, 0);

                pros::delay(800);
                temp_spin_dt(-100, -100);
                pros::delay(300);
                temp_spin_dt(0, 0);
                auton_point_to_point(vec2(0, 150), false);
                temp_spin_dt(-100, -100);
                pros::delay(300);
                temp_spin_dt(0, 0);

                auton_point_to_point(vec2(0, 500), false);
                auton_point_to_point(vec2(1000, 0), false);
#endif // Emergency 

                // Testing
                                // auton_point_to_point(vec2(-550, 1380), false);
                                // auton_point_to_point(vec2(-550, 1750), false);
                                // auton_point_to_point(vec2(0, 0), false);
                                // exit(0);





// #ifndef short_auto
//                 temp_spin_dt(80, 30);
// #else
//                 temp_spin_dt(0, 100);
// #endif
//                 pros::delay(850);
//                 temp_spin_dt(60, 60);
//                 pros::delay(800);
//                 temp_spin_dt(0, 0);
//                 temp_c_spin_motor(motor_lift_a, 60);
//                 temp_c_spin_motor(motor_lift_b, 60);
//                 pros::delay(1100);
//                 temp_c_spin_motor(motor_lift_a, 0);
//                 temp_c_spin_motor(motor_lift_b, 0);
//                 temp_spin_dt(-100, -100);
//                 pros::delay(50);
//                 temp_spin_dt(0, 0);
//                 toggle_claw();


//                 pros::delay(500);
//                 temp_spin_dt(-100, -100);
//                 pros::delay(500);
//                 temp_spin_dt(0, 0);
// #ifndef short_auto
//                 temp_spin_dt(-100, 100);
//                 // turn towards pin+cup stack
//                 pros::delay(250);
//                 temp_spin_dt(60, 60);
//                 pros::delay(1400);
//                 temp_spin_dt(40, 40);
//                 pros::delay(250);
//                 temp_spin_dt(0, 0);
//                 toggle_claw();
//                 pros::delay(200);
//                 temp_c_spin_motor(motor_lift_a, -100);
//                 temp_c_spin_motor(motor_lift_b, -100);
//                 temp_spin_dt(100, 100);
//                 pros::delay(80);
//                 // pros::delay(300);
//                 temp_spin_dt(100, -100);
//                 pros::delay(650);
//                 temp_spin_dt(100, 100);

//                 pros::delay(1100);
//                 temp_spin_dt(0, 0);
//                 temp_c_spin_motor(motor_lift_a, 60);
//                 temp_c_spin_motor(motor_lift_b, 60);
//                 pros::delay(1300);
//                 toggle_claw();
//                 pros::delay(200);
// #endif
// #ifdef SKILLS
//                 temp_spin_dt(-100, -100);
//                 pros::delay(200);
//                 temp_spin_dt(-100, 100);
//                 pros::delay(700);
//                 temp_spin_dt(100, 100);
//                 pros::delay(1000);
// #endif

                // temp_c_spin_motor(motor_lift_a, 60);
                // temp_c_spin_motor(motor_lift_b, 60);
                // pros::delay(1000);
                // temp_spin_dt(120, 120);
                // pros::delay(200);
                // temp_spin_dt(100, -100);
                // pros::delay(500);
                // temp_spin_dt(100, 100);
                // pros::delay(500);
                // temp_spin_dt(0, 0);



                // #ifdef USE_TAIPAN
//         }

//         printf("Freeing VM...");
//         free_VM();
//         printf("VM freed.\n");
// #endif

}

        void opcontrol() {
                pros::Task driver_read_input_handler(driver_read_input, "DRIVER INPUT READ");
                pros::Task driver_apply_dt_input_handler(driver_apply_dt_input, "DRIVER DT CTRL");
                pros::Task driver_appliy_lift_input_handler(driver_apply_lift_input, "DRIVER LIFT CTRL");

                while (true) {
                        pros::screen::print(TEXT_MEDIUM, 1, "Pos: (%.3f,   %.3f)", odom_sys.get_position().x, odom_sys.get_position().y);
                        pros::screen::print(TEXT_MEDIUM, 0, "Heading: %.3f", heading.load());
                        pros::screen::print(TEXT_MEDIUM, 2, "Net Error: %.3f", sqrt(pow(odom_sys.get_position().x, 2) + pow(odom_sys.get_position().y, 2)));
                        printf("Pos: (%.3f,%.3f)\n", odom_sys.get_position().x, odom_sys.get_position().y);
                        pros::screen::print(TEXT_MEDIUM, 4, "Wheel dist: %.3f", lat_wheel.get_total_length());
                        // Kill this thread
                        pros::delay(30);
                }
        }
