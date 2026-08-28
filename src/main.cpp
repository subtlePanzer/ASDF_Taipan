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

straight_odom_2_wheel odom_sys(odom_perp, odom_para /*, 2 * 25.4 * M_PI */);

#ifdef USE_TAIPAN
run_script_params* params;
#endif

static void home_claw(void) {
        uint32_t default_clear_col = pros::screen::get_eraser();
        uint32_t default_pen_col = pros::screen::get_pen();
        pros::screen::set_eraser(0x00FF0000);
        pros::screen::erase();
        pros::screen::set_pen(pros::Color::red);
        pros::screen::fill_rect(5, 5, 240, 200);
        pros::screen::set_pen(default_pen_col);
        pros::screen::set_eraser(default_clear_col);
        pros::screen::print(TEXT_MEDIUM, 0, "Warning: Setting claw");

        auto m = pros::Motor(motor_claw);
        m.set_encoder_units(pros::motor_encoder_units_e_t::E_MOTOR_ENCODER_ROTATIONS);
        m.set_zero_position(0);
        double last_pos = 999999;
        m.move(80);
        pros::delay(50);
        while (m.get_position() != last_pos) {
                last_pos = m.get_position();
                printf("%f\n", m.get_position());
                pros::delay(35);
        }
        m.move(0);
        save_claw_pos();

        pros::delay(1800);
        pros::screen::erase();
}

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
                heading.store(imui.get_yaw());
                odom_sys.calc_position();

                vec2 pos = odom_sys.get_position();
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

                pros::delay(20);
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

        home_claw();

        calibrate_imu();

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

void autonomous() {
        pros::screen::print(pros::E_TEXT_MEDIUM, 0, "AUTON");

        printf("Tasks: %d\n", pros::Task::get_count());

        // pros::Task auton_sensor_task(auton_read_sensors, "AUTON SENSOR READ");
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

#ifdef NO 
                printf("Running temporary auton\n");
                pros::screen::print(pros::E_TEXT_MEDIUM, 1, "RUNNING TEMPORARY AUTON");

                temp_spin_dt(65, 65);
                temp_c_spin_motor(motor_claw, 100);
                pros::delay(200);

                temp_spin_dt(-60, -60);
                pros::delay(300);
                temp_spin_dt(10, 10);
                pros::delay(50);
                temp_spin_dt(0, 0);

                temp_c_spin_motor(motor_lift_a, -100);
                temp_c_spin_motor(motor_lift_b, -100);
                pros::delay(1200);
                temp_c_spin_motor(motor_lift_a, 100);
                temp_c_spin_motor(motor_lift_b, 100);

                pros::delay(800);
                temp_spin_dt(0, 0);
                temp_spin_dt(20, 20);
                pros::delay(300);
                temp_spin_dt(0, 0);
                temp_c_spin_motor(motor_lift_a, -100);
                temp_c_spin_motor(motor_lift_b, -100);
                temp_spin_dt(-30, -30);
                pros::delay(300);
                temp_spin_dt(0, 0);

                pros::delay(1200);
                temp_c_spin_motor(motor_lift_a, 100);
                temp_c_spin_motor(motor_lift_b, 100);
                pros::delay(800);
                temp_c_spin_motor(motor_lift_a, 0);
                temp_c_spin_motor(motor_lift_b, 0);
                temp_spin_dt(60, 60);
                pros::delay(300);
                temp_c_spin_motor(motor_lift_a, -80);
                temp_c_spin_motor(motor_lift_b, -80);
                temp_spin_dt(0, 100);
                pros::delay(850);
                temp_spin_dt(60, 60);
                pros::delay(800);
                temp_spin_dt(0, 0);
                temp_c_spin_motor(motor_lift_a, 60);
                temp_c_spin_motor(motor_lift_b, 60);
                pros::delay(2000);
                temp_c_spin_motor(motor_lift_a, 0);
                temp_c_spin_motor(motor_lift_b, 0);
                temp_c_spin_motor(motor_claw, -100);
                pros::delay(300);
                temp_c_spin_motor(motor_claw, 0);
                temp_spin_dt(-100, -100);
                // pros::delay(300);
                // temp_spin_dt(0, -100);
                // pros::delay(850);
                // temp_spin_dt(100, 100);
                // pros::delay(800);
                // temp_spin_dt(0, 0);
#endif
                temp_spin_dt(65, 65);
                temp_c_spin_motor(motor_claw, 100);
                pros::delay(200);

                temp_spin_dt(-60, -60);
                pros::delay(300);
                temp_spin_dt(10, 10);
                pros::delay(50);
                temp_spin_dt(0, 0);

                temp_c_spin_motor(motor_lift_a, -100);
                temp_c_spin_motor(motor_lift_b, -100);
                pros::delay(1200);
                temp_c_spin_motor(motor_lift_a, 100);
                temp_c_spin_motor(motor_lift_b, 100);

                pros::delay(800);
                temp_spin_dt(0, 0);
                temp_spin_dt(20, 20);
                pros::delay(300);
                temp_spin_dt(0, 0);
                temp_c_spin_motor(motor_lift_a, -100);
                temp_c_spin_motor(motor_lift_b, -100);
                temp_spin_dt(-30, -30);
                pros::delay(300);
                temp_spin_dt(0, 0);

                pros::delay(1200);
                temp_c_spin_motor(motor_lift_a, 100);
                temp_c_spin_motor(motor_lift_b, 100);
                pros::delay(800);
                temp_c_spin_motor(motor_lift_a, 0);
                temp_c_spin_motor(motor_lift_b, 0);
                temp_spin_dt(60, 60);
                pros::delay(300);
                temp_c_spin_motor(motor_lift_a, -80);
                temp_c_spin_motor(motor_lift_b, -80);
                #ifndef short_auto
                temp_spin_dt(100, 0);
                #else
                temp_spin_dt(0, 100);
                #endif
                pros::delay(850);
                temp_spin_dt(60, 60);
                pros::delay(800);
                temp_spin_dt(0, 0);
                temp_c_spin_motor(motor_lift_a, 60);
                temp_c_spin_motor(motor_lift_b, 60);
                pros::delay(1100);
                temp_c_spin_motor(motor_lift_a, 0);
                temp_c_spin_motor(motor_lift_b, 0);
                temp_spin_dt(-100, -100);
                pros::delay(50);
                temp_spin_dt(0, 0);
                temp_c_spin_motor(motor_claw, -100);
                pros::delay(500);
                temp_spin_dt(-100, -100);
                pros::delay(500);
        #ifndef short_auto
                temp_spin_dt(-100, 100);
                // turn towards pin+cup stack
                pros::delay(250);
                temp_spin_dt(60, 60);
                pros::delay(1400);
                temp_spin_dt(40, 40);
                pros::delay(250);
                temp_spin_dt(0, 0);
                temp_c_spin_motor(motor_claw, 100);
                pros::delay(200);
                temp_c_spin_motor(motor_lift_a, -100);
                temp_c_spin_motor(motor_lift_b, -100);
                temp_spin_dt(100, 100);
                pros::delay(80);
                // pros::delay(300);
                temp_spin_dt(100, -100);
                pros::delay(650);
                temp_spin_dt(100, 100);

                pros::delay(1100);
                temp_spin_dt(0, 0);
                temp_c_spin_motor(motor_lift_a, 60);
                temp_c_spin_motor(motor_lift_b, 60);
                pros::delay(1300);
                temp_c_spin_motor(motor_claw, -100);
                pros::delay(200);
        #endif
#ifdef SKILLS
                temp_spin_dt(-100, -100);
                pros::delay(200);
                temp_spin_dt(-100, 100);
                pros::delay(700);
                temp_spin_dt(100, 100);
                pros::delay(1000);
#endif

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
#ifdef USE_TAIPAN
        }

        printf("Freeing VM...");
        free_VM();
        printf("VM freed.\n");
#endif

        opcontrol();
}

void opcontrol() {
        pros::Task driver_read_input_handler(driver_read_input, "DRIVER INPUT READ");
        pros::Task driver_apply_dt_input_handler(driver_apply_dt_input, "DRIVER DT CTRL");
        pros::Task driver_appliy_lift_input_handler(driver_apply_lift_input, "DRIVER LIFT CTRL");
        // pros::Task auton_sensor_task(auton_read_sensors, "AUTON SENSOR READ");

        while (true) {
                // Kill this thread
                pros::delay(100);
        }
}
