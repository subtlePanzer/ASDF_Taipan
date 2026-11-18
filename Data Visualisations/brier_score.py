import csv
import numpy as np
import pandas as pd

class HeuristicVEXMonteCarloEngine:
    """
    First-Principles Heuristic Monte Carlo Simulator for VEX Override.
    Features:
      - 4-Robot Parallel Discrete-Event Model (RED_1, RED_2 vs BLUE_1, BLUE_2)
      - Empirical Robot Capability Mapping (41103X=8.5s, 52111A=7.0s, Partner=14-18s)
      - Driver Heuristic Policy Tree (Score-Delta Gating & 88% Yellow Pin Scoring Bias)
      - Toggle Stability Damping (d_damp = 0.55)
      - Exact Match Snapshot Extraction and Brier Score Calibration
    """
    def __init__(self, field_states_path="field_states.csv", events_path="events.csv"):
        self.df_field = pd.read_csv(field_states_path)
        self.df_events = pd.read_csv(events_path)
        self._fit_ground_truth()
        
        # Alliance team assignments per match ID
        self.MATCH_TEAM_TO_ALLIANCE = {
            "T1_Q7":  {"41103X": "BLUE", "41103C": "BLUE", "12906A": "RED",  "23140A": "RED"},
            "T1_Q13": {"41103X": "RED",  "49898G": "RED",  "12223F": "BLUE", "76981A": "BLUE"},
            "T1_Q16": {"12906B": "BLUE", "49898G": "BLUE", "41103X": "RED",  "81321G": "RED"},
            "T1_Q17": {"41103X": "BLUE", "76981A": "BLUE", "41103C": "RED",  "12906A": "RED"},
            "T1_Q5":  {"12906B": "RED",  "41103X": "RED",  "52111A": "BLUE", "12223F": "BLUE"},
            "T1_Q20": {"76981B": "RED",  "49898G": "RED",  "41103X": "BLUE", "41103C": "BLUE"},
            "T1_SF1": {"41103X": "RED",  "41103C": "RED",  "12906A": "BLUE", "81321C": "BLUE"},
            "T1_F1":  {"41103X": "RED",  "41103C": "RED",  "52111A": "BLUE", "81321D": "BLUE"},
            "T2_Q2":  {"41103X": "RED",  "50911B": "RED",  "69153A": "BLUE", "81321B": "BLUE"},
            "T2_Q10": {"81321D": "RED",  "68076H": "RED",  "41103X": "BLUE", "52111A": "BLUE"},
            "T2_Q12": {"75477J": "RED",  "75477M": "RED",  "41103X": "BLUE", "14671A": "BLUE"},
            "T2_Q20": {"41103X": "RED",  "12223X": "RED",  "76981B": "BLUE", "81321C": "BLUE"}
        }

        # Empirical robot capabilities derived from telemetry & user specifications
        self.ROBOT_CAPABILITIES = {
            "41103X": {"cycle_sec": 8.5,  "flip_success": 0.82, "score_success": 0.88},
            "52111A": {"cycle_sec": 7.0,  "flip_success": 0.85, "score_success": 0.90},
            "41103C": {"cycle_sec": 11.0, "flip_success": 0.80, "score_success": 0.75},
            "76981B": {"cycle_sec": 15.8, "flip_success": 0.65, "score_success": 0.70},
            "69153A": {"cycle_sec": 15.0, "flip_success": 0.65, "score_success": 0.70},
            "12906A": {"cycle_sec": 14.5, "flip_success": 0.65, "score_success": 0.72},
            "12223X": {"cycle_sec": 15.0, "flip_success": 0.60, "score_success": 0.70},
            "12906B": {"cycle_sec": 14.0, "flip_success": 0.65, "score_success": 0.70},
            "81321D": {"cycle_sec": 14.0, "flip_success": 0.70, "score_success": 0.75},
            "49898G": {"cycle_sec": 14.0, "flip_success": 0.60, "score_success": 0.70},
            "23140A": {"cycle_sec": 18.0, "flip_success": 0.55, "score_success": 0.65},
            "76981A": {"cycle_sec": 15.0, "flip_success": 0.60, "score_success": 0.70},
            "DEFAULT": {"cycle_sec": 16.0, "flip_success": 0.60, "score_success": 0.70}
        }

    def _fit_ground_truth(self):
        self.ground_truth = {}
        for mid in self.df_field['Match_ID'].unique():
            sub = self.df_field[self.df_field['Match_ID'] == mid]
            final_row = sub.sort_values('Time').iloc[-1]
            self.ground_truth[mid] = 1 if final_row['RED_Score'] > final_row['BLUE_Score'] else 0

    def parse_stack_string(self, stack_str):
        if pd.isna(stack_str) or not str(stack_str).strip():
            return []
        return str(stack_str).strip().split('|')

    def extract_field_snapshot(self, row):
        toggles = {
            "Q1": str(row['Q1']).upper(),
            "Q2": str(row['Q2']).upper(),
            "Q3": str(row['Q3']).upper(),
            "Q4": str(row['Q4']).upper()
        }
        goals = {
            "GOAL_LOW_Q1": self.parse_stack_string(row['GOAL_LOW_Q1']),
            "GOAL_LOW_Q2": self.parse_stack_string(row['GOAL_LOW_Q2']),
            "GOAL_LOW_Q3": self.parse_stack_string(row['GOAL_LOW_Q3']),
            "GOAL_LOW_Q4": self.parse_stack_string(row['GOAL_LOW_Q4']),
            "GOAL_HIGH_Q1": self.parse_stack_string(row['GOAL_HIGH_Q1']),
            "GOAL_HIGH_Q2": self.parse_stack_string(row['GOAL_HIGH_Q2']),
            "GOAL_HIGH_Q3": self.parse_stack_string(row['GOAL_HIGH_Q3']),
            "GOAL_HIGH_Q4": self.parse_stack_string(row['GOAL_HIGH_Q4']),
            "CENTRE_GOAL": self.parse_stack_string(row['CENTRE_GOAL'])
        }
        return toggles, goals

    def evaluate_exact_field_score(self, toggles, goals, auto_bonus, red_midzone, blue_midzone):
        scores = {"RED": 0, "BLUE": 0}
        
        # Base Goal & Toggle Control Scoring
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

        scores["RED"] += auto_bonus.get("RED", 0)
        scores["BLUE"] += auto_bonus.get("BLUE", 0)
        scores["RED"] += min(2, red_midzone) * 8
        scores["BLUE"] += min(2, blue_midzone) * 8

        centre_yellows = goals.get("CENTRE_GOAL", []).count("YELLOW")
        if red_midzone > blue_midzone:
            scores["RED"] += centre_yellows * 10
        elif blue_midzone > red_midzone:
            scores["BLUE"] += centre_yellows * 10

        return scores

    def simulate_heuristic_rollout(self, match_id, t_rem, toggles, goals, yellow_pin_bias=0.88, toggle_damp=0.55):
        teams_dict = self.MATCH_TEAM_TO_ALLIANCE[match_id]
        red_teams = [t for t, a in teams_dict.items() if a == "RED"]
        blue_teams = [t for t, a in teams_dict.items() if a == "BLUE"]
        
        while len(red_teams) < 2: red_teams.append("DEFAULT")
        while len(blue_teams) < 2: blue_teams.append("DEFAULT")
        
        bots = {
            "RED_1": {"team": red_teams[0], "caps": self.ROBOT_CAPABILITIES.get(red_teams[0], self.ROBOT_CAPABILITIES["DEFAULT"]), "alliance": "RED"},
            "RED_2": {"team": red_teams[1], "caps": self.ROBOT_CAPABILITIES.get(red_teams[1], self.ROBOT_CAPABILITIES["DEFAULT"]), "alliance": "RED"},
            "BLUE_1": {"team": blue_teams[0], "caps": self.ROBOT_CAPABILITIES.get(blue_teams[0], self.ROBOT_CAPABILITIES["DEFAULT"]), "alliance": "BLUE"},
            "BLUE_2": {"team": blue_teams[1], "caps": self.ROBOT_CAPABILITIES.get(blue_teams[1], self.ROBOT_CAPABILITIES["DEFAULT"]), "alliance": "BLUE"}
        }

        cur_toggles = dict(toggles)
        cur_goals = {k: list(v) for k, v in goals.items()}
        t_bots = {b: t_rem for b in bots}
        red_midzone, blue_midzone = 0, 0

        max_stacks = {
            "GOAL_LOW_Q1": 5, "GOAL_LOW_Q2": 5, "GOAL_LOW_Q3": 5, "GOAL_LOW_Q4": 5,
            "GOAL_HIGH_Q1": 4, "GOAL_HIGH_Q2": 4, "GOAL_HIGH_Q3": 4, "GOAL_HIGH_Q4": 4,
            "CENTRE_GOAL": 2
        }

        while max(t_bots.values()) > 0:
            active_bot = max(t_bots, key=t_bots.get)
            t_curr = t_bots[active_bot]
            if t_curr <= 0:
                break

            bot_info = bots[active_bot]
            bot_alliance = bot_info["alliance"]
            caps = bot_info["caps"]
            cycle_mean = caps["cycle_sec"]

            curr_score = self.evaluate_exact_field_score(cur_toggles, cur_goals, {"RED": 6, "BLUE": 6}, red_midzone, blue_midzone)
            our_score = curr_score[bot_alliance]
            opp_alliance = "BLUE" if bot_alliance == "RED" else "RED"
            opp_score = curr_score[opp_alliance]
            score_diff = our_score - opp_score

            opp_toggles = [q for q, ctrl in cur_toggles.items() if ctrl == opp_alliance]
            neutral_toggles = [q for q, ctrl in cur_toggles.items() if ctrl == "NEUTRAL"]

            # HEURISTIC POLICY TREE
            if t_curr <= 15:
                # Endgame Phase: parking priority
                act = np.random.choice(["ENDGAME", "SCORE_PIN", "TOGGLE_FLIP"], p=[0.70, 0.20, 0.10])
            else:
                # Driver Control Phase: Trailing teams target toggles; otherwise heavy yellow pin scoring bias
                if score_diff < -15 and (opp_toggles or neutral_toggles) and np.random.rand() < 0.45:
                    act = "TOGGLE_FLIP"
                else:
                    act = np.random.choice(["SCORE_PIN", "TOGGLE_FLIP", "TRAVEL"], p=[0.75, 0.18, 0.07])

            if act == "SCORE_PIN":
                dur = np.random.gamma((cycle_mean**2)/3.5, 3.5/cycle_mean)
                t_bots[active_bot] -= dur
                if t_bots[active_bot] >= 0 and np.random.rand() < caps["score_success"]:
                    g_type = np.random.choice(["LOW", "HIGH"], p=[0.75, 0.25])
                    q = np.random.choice(["Q1", "Q2", "Q3", "Q4"])
                    target_goal = f"GOAL_{g_type}_{q}"
                    
                    if len(cur_goals[target_goal]) < max_stacks[target_goal]:
                        # Empirical Yellow Pin Scoring Bias
                        pin_col = "YELLOW" if np.random.rand() < yellow_pin_bias else bot_alliance
                        cur_goals[target_goal].append(pin_col)

            elif act == "TOGGLE_FLIP":
                dur = np.random.gamma((7.5**2)/3.5, 3.5/7.5)
                t_bots[active_bot] -= dur
                if t_bots[active_bot] >= 0 and np.random.rand() < (caps["flip_success"] * toggle_damp):
                    target_candidates = opp_toggles + neutral_toggles if (opp_toggles or neutral_toggles) else ["Q1", "Q2", "Q3", "Q4"]
                    q_target = np.random.choice(target_candidates)
                    cur_toggles[q_target] = bot_alliance

            elif act == "ENDGAME":
                dur = np.random.gamma((6.0**2)/3.0, 3.0/6.0)
                t_bots[active_bot] -= dur
                if t_bots[active_bot] >= 0 and np.random.rand() < 0.88:
                    if bot_alliance == "RED": red_midzone += 1
                    else: blue_midzone += 1

            else: # TRAVEL
                dur = np.random.gamma((3.0**2)/1.5, 1.5/3.0)
                t_bots[active_bot] -= dur

        auto_bonus = {"RED": 6, "BLUE": 6}
        final_scores = self.evaluate_exact_field_score(cur_toggles, cur_goals, auto_bonus, red_midzone, blue_midzone)
        return 1 if final_scores["RED"] > final_scores["BLUE"] else 0

    def compute_brier_calibration(self, n_rollouts=200, snapshot_times=[15, 30, 45, 60, 75, 90, 105]):
        records = []
        for snapshot_t in snapshot_times:
            sub = self.df_field[self.df_field['Time'] == snapshot_t]
            for _, row in sub.iterrows():
                mid = row['Match_ID']
                y_true = self.ground_truth[mid]
                t_rem = 120 - snapshot_t
                
                toggles, goals = self.extract_field_snapshot(row)

                wins = sum(self.simulate_heuristic_rollout(mid, t_rem, toggles, goals) for _ in range(n_rollouts))
                w_pred = wins / n_rollouts

                records.append({
                    "Match_ID": mid,
                    "Snapshot_Time": snapshot_t,
                    "Time_Remaining": t_rem,
                    "Predicted_Win_Prob": w_pred,
                    "Actual_Outcome": y_true,
                    "Squared_Error": (w_pred - y_true) ** 2
                })

        df_res = pd.DataFrame(records)
        overall_brier = df_res['Squared_Error'].mean()
        
        print("\n=======================================================")
        print("          BRIER CALIBRATION SCORE ANALYSIS             ")
        print("=======================================================")
        print(f"Overall Brier Calibration Score (BS): {overall_brier:.4f}")
        print("-------------------------------------------------------")
        print("Brier Score Breakdown by Match Snapshot Time:")
        time_breakdown = df_res.groupby('Snapshot_Time')['Squared_Error'].mean()
        for t_val, bs_val in time_breakdown.items():
            print(f"  t = {t_val:3d}s (t_rem = {120-t_val:3d}s) | Brier Score: {bs_val:.4f}")
        print("=======================================================\n")
        
        return df_res, overall_brier

if __name__ == "__main__":
    engine = HeuristicVEXMonteCarloEngine("field_states.csv", "events.csv")
    df_results, brier_score = engine.compute_brier_calibration(n_rollouts=200)