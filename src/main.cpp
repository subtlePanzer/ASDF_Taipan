// -----------------------------------------------------------------------------
// ASDF 2027 'Taipan' language - (c) 2026 Riley Lorenz  & ASDF Robotics
// -----------------------------------------------------------------------------

#include "main.h"
#include "odom.hpp"

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

#ifdef USE_TAIPAN
run_script_params* params;
#endif

static void home_claw(void) {
        uint32_t default_clear_col = pros::screen::get_eraser();
        pros::screen::set_eraser(0x00FF0000);
        pros::screen::erase();
        // pros::screen::fill_rect(5, 5, 240, 200);
        pros::screen::print(TEXT_MEDIUM, 0, "Warning: Setting claw");
        pros::screen::set_eraser(default_clear_col);

        save_claw_pos();
        pros::delay(800);
        pros::screen::erase();
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

        pros::screen::print(TEXT_MEDIUM, 0, "Initialised");
        pros::delay(500);
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

        printf("⌛ 5 second mark.\n");

        millis_start = pros::millis();
        while (pros::millis() - millis_start < 5'000 && !abort_auton.load()) {
                pros::delay(25);
        }

        printf("⌛ 10 second mark.\n");

        millis_start = pros::millis();
        while (pros::millis() - millis_start < 4'950 && !abort_auton.load()) {
                pros::delay(25);
        } // leave time for cleanup

        printf("⌛ 15 second mark.\n");
        printf("⌛ AUTON TIME PERIOD FINISHED.\n");
        printf("🐍 Script aborted.\n");
        pros::c::task_notify(main);
}
#endif

void autonomous() {
        pros::screen::print(pros::E_TEXT_MEDIUM, 0, "AUTON");

        pros::Task auton_sensor_task(auton_read_sensors, "AUTON SENSOR READ");
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
                pros::delay(1200);
                temp_c_spin_motor(motor_lift_a, -100);
                temp_c_spin_motor(motor_lift_b, -100);
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
                pros::delay(300);
                temp_spin_dt(0, 0);
                temp_c_spin_motor(motor_lift_a, 80);
                temp_c_spin_motor(motor_lift_b, 80);
                pros::delay(1000);
                temp_c_spin_motor(motor_lift_a, 0);
                temp_c_spin_motor(motor_lift_b, 0);
                temp_c_spin_motor(motor_claw, -100);
                pros::delay(300);
                temp_c_spin_motor(motor_claw, 0);
                temp_spin_dt(-100, -100);
                pros::delay(300);
                temp_spin_dt(0, -100);
                pros::delay(850);
                temp_spin_dt(100, 100);
                pros::delay(800);
                temp_spin_dt(0, 0);
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

        while (true) {
                // Kill this thread
                pros::delay(100);
        }
}
