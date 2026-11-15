import numpy as np
import pandas as pd
import csv

class FirstPrinciplesVEXEngine2v2:
    """
    First-Principles 2v2 Monte Carlo Simulation Engine for VEX Override.
    Features:
      - 4-Robot Parallel Discrete-Event Rollouts (2 Red vs 2 Blue)
      - Temporal Phase Gating (ENDGAME strictly restricted to t_rem <= 15s)
      - Physical Goal Stack Height Caps (h_max)
      - Custom Manual Robot Capability Overrides (Cycle time, variance, flip success)
      - Multi-Action Comparison at a Glance with Brier Calibration capability
    """
    def __init__(self, events_csv_path="events.csv"):
        self.df = pd.read_csv(events_csv_path)
        self.df['duration'] = (self.df['Event_end_sec'] - self.df['Event_start_sec']).clip(lower=0.1)
        self._fit_distributions()

    def _fit_distributions(self):
        # Gamma duration parameters per action type
        self.gamma_params = {}
        for action in ['TRAVEL', 'PICKUP_PIN', 'PLACE_YELLOW', 'PLACE_COLOURED', 'TOGGLE_FLIP', 'ENDGAME', 'DEFENCE']:
            sub = self.df[self.df['Event_Type'] == action]['duration']
            if len(sub) > 2:
                m, v = sub.mean(), max(sub.var(), 0.1)
                self.gamma_params[action] = ((m**2)/v, v/m)
            else:
                self.gamma_params[action] = (2.0, 1.5)

        # Beta reliability parameters (alpha, beta)
        self.beta_params = {}
        for action in ['TOGGLE_FLIP', 'PLACE_YELLOW', 'PLACE_COLOURED', 'ENDGAME']:
            sub = self.df[self.df['Event_Type'] == action]
            succ = (sub['Outcome'] == 'SUCCESS').sum()
            fail = (sub['Outcome'] == 'FAILURE').sum()
            self.beta_params[action] = (2 + succ, 1 + fail)

    def sample_duration(self, action_type, mean_override=None, var_override=None):
        if mean_override is not None and mean_override > 0:
            var = var_override if (var_override and var_override > 0) else (mean_override * 0.4)
            k = (mean_override ** 2) / var
            scale = var / mean_override
            return np.random.gamma(k, scale)
        k, scale = self.gamma_params.get(action_type, (2.0, 1.5))
        return np.random.gamma(k, scale)

    def sample_success(self, action_type, p_override=None):
        if p_override is not None:
            return np.random.rand() < max(0.0, min(1.0, p_override))
        a, b = self.beta_params.get(action_type, (2, 1))
        p = np.random.beta(a, b)
        return np.random.rand() < p

    def evaluate_exact_field(self, toggles, goals, auto_bonus, red_midzone, blue_midzone):
        """
        Exact field scoring logic based on reconstruction rules.
        """
        scores = {"RED": 0, "BLUE": 0}
        
        # 1. Base Goal & Quadrant Toggle Scoring
        for goal_name, stack in goals.items():
            for pin in stack:
                if pin in ["RED", "BLUE"]:
                    scores[pin] += 5
                elif pin == "YELLOW":
                    if goal_name.startswith("GOAL_LOW_") or goal_name.startswith("GOAL_HIGH_"):
                        quad = goal_name[-2:]
                        controller = toggles.get(quad, "NEUTRAL")
                        if controller in ["RED", "BLUE"]:
                            scores[controller] += 10

        # 2. Autonomous Bonus (Evaluated at t=105s / 15s elapsed)
        scores["RED"] += auto_bonus.get("RED", 0)
        scores["BLUE"] += auto_bonus.get("BLUE", 0)

        # 3. Midzone Parking Bonus (8 pts per robot, max 2 robots per alliance)
        scores["RED"] += min(2, red_midzone) * 8
        scores["BLUE"] += min(2, blue_midzone) * 8

        # 4. Centre Goal Yellow Pins (Awarded to side with strictly more midzone robots)
        centre_yellows = goals.get("CENTRE_GOAL", []).count("YELLOW")
        if red_midzone > blue_midzone:
            scores["RED"] += centre_yellows * 10
        elif blue_midzone > red_midzone:
            scores["BLUE"] += centre_yellows * 10

        return scores

    def simulate_rollout_2v2(self, t_rem, toggles, goals, alliance="RED", first_action=None, 
                             robot_caps=None, auto_bonus=None):
        
        if auto_bonus is None:
            auto_bonus = {"RED": 6, "BLUE": 6}

        if robot_caps is None:
            # Default 4-robot setup (2 Red, 2 Blue)
            robot_caps = {
                "us_1": {"cycle_sec": None, "variance": None, "flip_success": None, "max_stack": 4},
                "us_2": {"cycle_sec": None, "variance": None, "flip_success": None, "max_stack": 4},
                "opp_1": {"cycle_sec": None, "variance": None, "flip_success": None, "max_stack": 4},
                "opp_2": {"cycle_sec": None, "variance": None, "flip_success": None, "max_stack": 4}
            }

        cur_toggles = dict(toggles)
        cur_goals = {k: list(v) for k, v in goals.items()}
        opp_alliance = "BLUE" if alliance == "RED" else "RED"

        # Independent clock per robot to model 4-robot parallel execution
        t_bots = {
            "us_1": t_rem,
            "us_2": t_rem,
            "opp_1": t_rem,
            "opp_2": t_rem
        }
        
        red_midzone, blue_midzone = 0, 0

        # 1. Immediate Candidate Action on Primary Robot (us_1)
        if first_action and first_action != "DO_NOTHING":
            # TEMPORAL PHASE CHECK: Endgame only allowed in last 15 seconds!
            if first_action == "ENDGAME" and t_rem > 15:
                # Invalid action timing: action fails/wastes duration
                t_bots["us_1"] -= 2.0
            else:
                dur = self.sample_duration(first_action, robot_caps["us_1"]["cycle_sec"], robot_caps["us_1"]["variance"])
                t_bots["us_1"] -= dur
                if t_bots["us_1"] >= 0 and self.sample_success(first_action, robot_caps["us_1"]["flip_success"]):
                    target_goal = "GOAL_LOW_Q1"
                    if first_action == "TOGGLE_FLIP":
                        cur_toggles["Q1"] = alliance
                    elif first_action == "PLACE_COLOURED":
                        if len(cur_goals[target_goal]) < robot_caps["us_1"]["max_stack"]:
                            cur_goals[target_goal].append(alliance)
                    elif first_action == "PLACE_YELLOW":
                        if len(cur_goals[target_goal]) < robot_caps["us_1"]["max_stack"]:
                            cur_goals[target_goal].append("YELLOW")
                    elif first_action == "ENDGAME" and t_rem <= 15:
                        if alliance == "RED": red_midzone += 1
                        else: blue_midzone += 1

        # 2. Continuous 4-Robot Parallel Discrete-Event Rollout Loop
        while max(t_bots.values()) > 0:
            active_bot = max(t_bots, key=t_bots.get)
            t_curr = t_bots[active_bot]
            if t_curr <= 0:
                break

            is_us = active_bot.startswith("us")
            bot_alliance = alliance if is_us else opp_alliance
            caps = robot_caps[active_bot]

            # TEMPORAL ACTION GATING
            if t_curr <= 15:
                # Endgame Phase (Final 15 seconds): Robots prioritize parking
                act_choices = ["ENDGAME", "TOGGLE_FLIP", "PLACE_COLOURED", "PLACE_YELLOW"]
                act_probs = [0.60, 0.20, 0.10, 0.10]
            else:
                # Mid-Game Driver Control Phase: ENDGAME IS STRICTLY FORBIDDEN
                act_choices = ["TOGGLE_FLIP", "PLACE_COLOURED", "PLACE_YELLOW", "TRAVEL"]
                act_probs = [0.35, 0.30, 0.25, 0.10]

            act = np.random.choice(act_choices, p=act_probs)
            dur = self.sample_duration(act, caps["cycle_sec"], caps["variance"])
            t_bots[active_bot] -= dur

            if t_bots[active_bot] >= 0 and self.sample_success(act, caps["flip_success"]):
                q = np.random.choice(["Q1", "Q2", "Q3", "Q4"])
                target_goal = f"GOAL_LOW_{q}"
                
                if act == "TOGGLE_FLIP":
                    cur_toggles[q] = bot_alliance
                elif act in ["PLACE_COLOURED", "PLACE_YELLOW"]:
                    if len(cur_goals[target_goal]) < caps["max_stack"]:
                        cur_goals[target_goal].append(bot_alliance if act == "PLACE_COLOURED" else "YELLOW")
                elif act == "ENDGAME" and t_curr <= 15:
                    if bot_alliance == "RED": red_midzone += 1
                    else: blue_midzone += 1

        final_scores = self.evaluate_exact_field(cur_toggles, cur_goals, auto_bonus, red_midzone, blue_midzone)
        return 1 if final_scores[alliance] > final_scores[opp_alliance] else 0

    def compare_actions_at_a_glance(self, t_rem=45, toggles=None, goals=None, alliance="RED", 
                                    robot_caps=None, auto_bonus=None, n_sims=3000):
        if toggles is None:
            toggles = {"Q1": "RED", "Q2": "RED", "Q3": "BLUE", "Q4": "NEUTRAL"}
        if goals is None:
            goals = {
                "GOAL_LOW_Q1": ["YELLOW"], "GOAL_LOW_Q2": ["YELLOW"], "GOAL_LOW_Q3": ["YELLOW"], "GOAL_LOW_Q4": [],
                "GOAL_HIGH_Q1": ["YELLOW"], "GOAL_HIGH_Q2": ["YELLOW"], "GOAL_HIGH_Q3": ["YELLOW"], "GOAL_HIGH_Q4": ["YELLOW"],
                "CENTRE_GOAL": ["YELLOW"]
            }

        candidate_actions = ["DO_NOTHING", "TOGGLE_FLIP", "PLACE_COLOURED", "PLACE_YELLOW", "DEFENCE", "ENDGAME"]
        
        # Calculate baseline DO_NOTHING
        base_wins = sum(self.simulate_rollout_2v2(t_rem, toggles, goals, alliance, "DO_NOTHING", robot_caps, auto_bonus) for _ in range(n_sims))
        w_0 = base_wins / n_sims

        results = []
        for action in candidate_actions:
            if action == "DO_NOTHING":
                w_act = w_0
            else:
                act_wins = sum(self.simulate_rollout_2v2(t_rem, toggles, goals, alliance, action, robot_caps, auto_bonus) for _ in range(n_sims))
                w_act = act_wins / n_sims
            
            delta = w_act - w_0
            results.append({
                "Action (u)": action,
                "Win_Rate": w_act,
                "Win Prob W(s, u)": f"{w_act * 100:.2f}%",
                "Delta vs Base (ΔW)": f"{delta * 100:+.2f}%"
            })

        results.sort(key=lambda x: x["Win_Rate"], reverse=True)
        for rank, res in enumerate(results, start=1):
            res["Rank"] = rank
            del res["Win_Rate"]

        return pd.DataFrame(results)[["Rank", "Action (u)", "Win Prob W(s, u)", "Delta vs Base (ΔW)"]]

    def run_cli(self):
        print("\n=======================================================")
        print("  FIRST-PRINCIPLES 2v2 VEX MONTE CARLO ENGINE (CLI)   ")
        print("=======================================================\n")

        alliance = input("Alliance [RED/BLUE] (default: RED): ").strip().upper() or "RED"
        t_rem = float(input("Time Remaining (seconds) (default: 45): ") or 45)
        
        print("\n--- Toggle Control States ---")
        toggles = {}
        for q in ["Q1", "Q2", "Q3", "Q4"]:
            toggles[q] = input(f"Toggle {q} state [RED/BLUE/NEUTRAL] (default: NEUTRAL): ").strip().upper() or "NEUTRAL"

        print("\n--- Manual Capability Overrides per Robot ---")
        print("Our Robot 1:")
        us1_c = input("  Mean Cycle Time (sec) [e.g. 3.5]: ").strip()
        us1_f = input("  Toggle Flip Success (0.0 - 1.0) [e.g. 0.75]: ").strip()
        us1_h = input("  Max Stack Height Limit [e.g. 4]: ").strip()
        
        print("Opponent Alliance Robots (Opp 1 & Opp 2):")
        opp_c = input("  Opponent Mean Cycle Time (sec) [e.g. 2.8]: ").strip()
        opp_f = input("  Opponent Toggle Flip Success [e.g. 0.85]: ").strip()

        robot_caps = {
            "us_1": {
                "cycle_sec": float(us1_c) if us1_c else None, 
                "variance": None, 
                "flip_success": float(us1_f) if us1_f else None, 
                "max_stack": int(us1_h) if us1_h else 4
            },
            "us_2": {"cycle_sec": None, "variance": None, "flip_success": None, "max_stack": 4},
            "opp_1": {
                "cycle_sec": float(opp_c) if opp_c else None, 
                "variance": None, 
                "flip_success": float(opp_f) if opp_f else None, 
                "max_stack": 4
            },
            "opp_2": {
                "cycle_sec": float(opp_c) if opp_c else None, 
                "variance": None, 
                "flip_success": float(opp_f) if opp_f else None, 
                "max_stack": 4
            }
        }

        n_sims = int(input("\nNumber of Rollout Simulations (default: 3000): ") or 3000)

        print("\nExecuting 4-robot parallel rollouts with temporal phase gating...")
        df_res = self.compare_actions_at_a_glance(
            t_rem=t_rem, toggles=toggles, alliance=alliance, 
            robot_caps=robot_caps, n_sims=n_sims
        )

        print("\n=======================================================")
        print("               MULTI-ACTION DECISION MATRIX            ")
        print("=======================================================")
        print(df_res.to_string(index=False))
        print("=======================================================\n")

if __name__ == "__main__":
    engine = FirstPrinciplesVEXEngine2v2("events.csv")
    engine.run_cli()