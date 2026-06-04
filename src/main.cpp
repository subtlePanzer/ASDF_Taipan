// -----------------------------------------------------------------------------
// ASDF 2027 'Taipan' language - (c) 2026 Riley Lorenz  & ASDF Robotics
// -----------------------------------------------------------------------------

#include "main.h"

extern "C" {
#include "api.h"
#include "driver.h"
#include "hardware.h"
#include "object.h"
#include "tp_main.h"
#include "vm.h"
};

run_script_params* params;

void initialize() {
        printf("============================INIT============================\n");
        printf("🕰️  Initialisation period start...\n");

        // robot
        init_hardware();

        // Taipan
        std::atomic_init(&abort_auton, false);
        std::atomic_init(&vm_cleanup_done, false);

        params = new run_script_params();
        params->func = init_tp();
        params->interrupt = &abort_auton;
        params->vm_cleanup_atomic = &vm_cleanup_done;

        pros::screen::erase();
        pros::screen::print(TEXT_MEDIUM, 0, "Initialised");
        pros::screen::erase();
        pros::delay(500);

        printf("🕰️  Finished init.\n");
        printf("------------------------------------------------------------\n\n");
}

void disabled() {}

void competition_initialize() {}

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

void autonomous() {
        printf("=========================AUTONOMOUS=========================\n");
        printf("🕰️  Autonomous period start:\n");
        pros::screen::print(pros::E_TEXT_MEDIUM, 0, "AUTON");

        if (params->func != NULL) {
                printf("🐍 Running... \n");
                abort_auton.store(false);

                pros::task_t main_task = pros::c::task_get_current();

                params->main_handle = &main_task;

                printf("🐍 Delegating to runtime\n");

                pros::Task taipan_runtime_task(run_script, params, "TAIPAN_RUNTIME");
                pros::Task taipan_runtime_aborter_task(delay_thrd, (void*)main_task, "TAIPAN_RUNTIME_ABORTER");

                pros::c::task_notify_take(true, TIMEOUT_MAX);
                abort_auton.store(true);

                while (!vm_cleanup_done.load()) {
                        pros::delay(10); // wait until vm is ready for completion
                }

                if (!atomic_load(&abort_auton))
                        printf("🐍 Script finished.\n");
        } else {
                printf("🤖 No script found or compilation failed. Please review error logs.\n");
        }

        printf("🐍 Freeing VM...");
        free_VM();
        printf("VM freed.\n");

        printf("🕰️  Finished auton.\n");
        printf("------------------------------------------------------------\n\n");

        opcontrol();
}

void opcontrol() {
        printf("=========================OP CONTROL=========================\n");
        printf("🕰️  Driver control period start:\n");

        pros::Task driver_read_input_handler(driver_read_input, "DRIVER INPUT READ");
        pros::Task driver_apply_dt_input_handler(driver_apply_dt_input, "DRIVER DT CTRL");
        pros::Task driver_appliy_lift_input_handler(driver_apply_lift_input, "DRIVER LIFT CTRL");

        while (true) {
                pros::delay(100);
        }

        printf("🕰️  Finished driver control period.\n");
        printf("------------------------------------------------------------\n\n");
}
