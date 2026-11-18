import pandas as pd
import numpy as np


class EmpiricalMatchAnalyzer:
    """
    Analytically computes primary quadrant definitions, conditional toggle hold
    probabilities, realized pin Expected Values, defense degradation factors,
    and robot cycle benchmarks directly from events.csv and field_states.csv.
    """

    def __init__(self, events_path="events.csv", field_states_path="field_states.csv"):
        self.df_events = pd.read_csv(events_path)
        self.df_field = pd.read_csv(field_states_path)

        self.df_events["Quadrant"] = self.df_events["Target_ID"].apply(self._extract_quadrant)
        self.df_events["Duration"] = (
            self.df_events["Event_end_sec"] - self.df_events["Event_start_sec"]
        ).clip(lower=0.05)

        self.pq_df, self.pq_map = self._identify_primary_quadrants()

    @staticmethod
    def _extract_quadrant(target_id):
        if pd.isna(target_id):
            return None
        s = str(target_id).upper()
        for q in ["Q1", "Q2", "Q3", "Q4"]:
            if q in s:
                return q
        return None

    def _identify_primary_quadrants(self):
        """
        Determines each alliance's Primary Quadrant per match using weighted
        scoring & toggle investment:
          Investment(q) = 2.0 * Yellow_Pins + 1.5 * Toggle_Flips + 0.5 * Coloured_Pins
        """
        records = []
        pq_map = {}

        for mid in sorted(self.df_events["Match_ID"].unique()):
            final_row = self.df_field[
                (self.df_field["Match_ID"] == mid) & (self.df_field["Time"] == 120)
            ].iloc[0]

            if final_row["RED_Score"] > final_row["BLUE_Score"]:
                winner = "RED"
            elif final_row["BLUE_Score"] > final_row["RED_Score"]:
                winner = "BLUE"
            else:
                winner = "TIE"

            for alliance in ["RED", "BLUE"]:
                opp = "BLUE" if alliance == "RED" else "RED"
                sub = self.df_events[
                    (self.df_events["Match_ID"] == mid)
                    & (self.df_events["Alliance"] == alliance)
                ]

                yellows = sub[
                    (sub["Event_Type"] == "PLACE_YELLOW")
                    & (sub["Outcome"].isin(["SUCCESS", "PARTIAL"]))
                ]
                coloureds = sub[
                    (sub["Event_Type"] == "PLACE_COLOURED")
                    & (sub["Outcome"].isin(["SUCCESS", "PARTIAL"]))
                ]
                flips = sub[
                    (sub["Event_Type"] == "TOGGLE_FLIP") & (sub["Outcome"] == "SUCCESS")
                ]

                q_scores = {
                    q: 2.0 * (yellows["Quadrant"] == q).sum()
                    + 1.5 * (flips["Quadrant"] == q).sum()
                    + 0.5 * (coloureds["Quadrant"] == q).sum()
                    for q in ["Q1", "Q2", "Q3", "Q4"]
                }

                best_q = max(q_scores, key=q_scores.get)
                if q_scores[best_q] == 0:
                    best_q = None

                pq_map[(mid, alliance)] = best_q
                final_owner = final_row[best_q] if best_q else None

                records.append(
                    {
                        "Match_ID": mid,
                        "Alliance": alliance,
                        "Match_Won": int(winner == alliance),
                        "Primary_Q": best_q,
                        "Yellow_In_PQ": (yellows["Quadrant"] == best_q).sum() if best_q else 0,
                        "Coloured_In_PQ": (coloureds["Quadrant"] == best_q).sum() if best_q else 0,
                        "Flips_In_PQ": (flips["Quadrant"] == best_q).sum() if best_q else 0,
                        "Final_Owner": final_owner,
                        "Retained_120s": int(final_owner == alliance) if best_q else np.nan,
                        "Inverted_To_Opp": int(final_owner == opp) if best_q else np.nan,
                        "Ended_Neutral": int(final_owner == "NEUTRAL") if best_q else np.nan,
                    }
                )

        return pd.DataFrame(records), pq_map

    def analyze_primary_quadrant_retention(self):
        """
        Computes end-of-match retention, inversion, and neutral rates for Primary Quadrants.
        """
        valid = self.pq_df.dropna(subset=["Retained_120s"])
        heavy_yellow = valid[valid["Yellow_In_PQ"] >= 3]

        print("\n=========================================================================")
        print("1. PRIMARY QUADRANT IDENTIFICATION & END-OF-MATCH RETENTION (t = 120s)")
        print("=========================================================================")
        print(
            self.pq_df[
                [
                    "Match_ID",
                    "Alliance",
                    "Primary_Q",
                    "Yellow_In_PQ",
                    "Coloured_In_PQ",
                    "Flips_In_PQ",
                    "Final_Owner",
                    "Retained_120s",
                ]
            ].to_string(index=False)
        )
        print("-------------------------------------------------------------------------")
        print(
            f"All Active Primary Quadrants (n={len(valid)}): "
            f"Retained={valid['Retained_120s'].mean()*100:.1f}%, "
            f"Inverted={valid['Inverted_To_Opp'].mean()*100:.1f}%, "
            f"Neutral={valid['Ended_Neutral'].mean()*100:.1f}%"
        )
        print(
            f"Heavy-Yellow Primary Quadrants (>=3 yellows, n={len(heavy_yellow)}): "
            f"Retained={heavy_yellow['Retained_120s'].mean()*100:.1f}%, "
            f"Inverted={heavy_yellow['Inverted_To_Opp'].mean()*100:.1f}%, "
            f"Neutral={heavy_yellow['Ended_Neutral'].mean()*100:.1f}%"
        )

    def analyze_conditional_hold_probabilities(self):
        """
        Computes P(Hold at 120s | Holding at t) and P(Recover by 120s | Not Holding at t)
        across all 10-second timestamps in field_states.csv.
        """
        valid = self.pq_df.dropna(subset=["Retained_120s"])
        timestamps = sorted(t for t in self.df_field["Time"].unique() if 0 < t < 120)
        rows = []

        for t in timestamps:
            hold_given_hold = []
            hold_given_not = []
            win_given_hold = []

            for _, pq_row in valid.iterrows():
                mid = pq_row["Match_ID"]
                alliance = pq_row["Alliance"]
                q = pq_row["Primary_Q"]
                retained_end = pq_row["Retained_120s"]
                won_match = pq_row["Match_Won"]

                snap = self.df_field[
                    (self.df_field["Match_ID"] == mid) & (self.df_field["Time"] == t)
                ].iloc[0]

                if snap[q] == alliance:
                    hold_given_hold.append(retained_end)
                    win_given_hold.append(won_match)
                else:
                    hold_given_not.append(retained_end)

            rows.append(
                {
                    "Time_t": t,
                    "Time_Rem": 120 - t,
                    "N_Hold_t": len(hold_given_hold),
                    "P(Retain_120s|Hold_t)%": round(np.mean(hold_given_hold) * 100, 1)
                    if hold_given_hold
                    else np.nan,
                    "P(Win|Hold_t)%": round(np.mean(win_given_hold) * 100, 1)
                    if win_given_hold
                    else np.nan,
                    "N_NotHold_t": len(hold_given_not),
                    "P(Recover_120s|NotHold_t)%": round(np.mean(hold_given_not) * 100, 1)
                    if hold_given_not
                    else np.nan,
                }
            )

        df_cond = pd.DataFrame(rows)
        print("\n=========================================================================")
        print("2. CONDITIONAL PRIMARY TOGGLE RETENTION BY MATCH TIMESTAMP")
        print("=========================================================================")
        print(df_cond.to_string(index=False))
        return df_cond

    def analyze_realized_yellow_ev(self):
        """
        Computes empirical realized margin EV (+10, -10, or 0) for every placed
        yellow pin in Primary vs. Non-Primary quadrants.
        """
        yellows = self.df_events[
            (self.df_events["Event_Type"] == "PLACE_YELLOW")
            & (self.df_events["Outcome"].isin(["SUCCESS", "PARTIAL"]))
        ]
        records = []

        for _, row in yellows.iterrows():
            mid = row["Match_ID"]
            alliance = row["Alliance"]
            opp = "BLUE" if alliance == "RED" else "RED"
            q = row["Quadrant"]
            if not q:
                continue

            is_primary = q == self.pq_map.get((mid, alliance))
            final_row = self.df_field[
                (self.df_field["Match_ID"] == mid) & (self.df_field["Time"] == 120)
            ].iloc[0]
            final_ctrl = final_row[q]

            if final_ctrl == alliance:
                margin = 10.0
                status = "SCORED_FOR_US (+10)"
            elif final_ctrl == opp:
                margin = -10.0
                status = "INVERTED_TO_OPP (-10)"
            else:
                margin = 0.0
                status = "NEUTRAL (0)"

            records.append(
                {
                    "Is_Primary": is_primary,
                    "Status": status,
                    "Realized_Margin": margin,
                }
            )

        df_yev = pd.DataFrame(records)
        print("\n=========================================================================")
        print("3. EMPIRICAL REALIZED EV PER YELLOW PIN PLACED (vs +5.00 COLOURED)")
        print("=========================================================================")
        for is_p, grp in df_yev.groupby("Is_Primary"):
            label = "PRIMARY QUADRANT" if is_p else "NON-PRIMARY QUADRANT"
            dist = grp["Status"].value_counts(normalize=True).mul(100).round(1)
            print(f"\n{label} (n={len(grp)} pins):")
            for k, v in dist.items():
                print(f"  {k:24s}: {v:5.1f}%")
            print(f"  -> Empirical Realized Margin EV: {grp['Realized_Margin'].mean():+.2f} pts")

    def analyze_defense_and_endgame(self):
        """
        Computes empirical defense degradation factor d_def, endgame timing,
        and midzone win correlation.
        """
        def_rows = []
        for mid in sorted(self.df_events["Match_ID"].unique()):
            for alliance in ["RED", "BLUE"]:
                opp = "BLUE" if alliance == "RED" else "RED"
                opp_def = self.df_events[
                    (self.df_events["Match_ID"] == mid)
                    & (self.df_events["Alliance"] == opp)
                    & (self.df_events["Event_Type"] == "DEFENCE")
                ]
                our_places = self.df_events[
                    (self.df_events["Match_ID"] == mid)
                    & (self.df_events["Alliance"] == alliance)
                    & (self.df_events["Event_Type"].isin(["PLACE_YELLOW", "PLACE_COLOURED"]))
                    & (self.df_events["Outcome"].isin(["SUCCESS", "PARTIAL"]))
                ]
                def_rows.append(
                    {
                        "Heavy_Defense": opp_def["Duration"].sum() >= 15.0,
                        "Opp_Def_Sec": opp_def["Duration"].sum(),
                        "Pins_Scored": len(our_places),
                    }
                )

        df_def = pd.DataFrame(def_rows)
        uncontested = df_def[~df_def["Heavy_Defense"]]["Pins_Scored"].mean()
        contested = df_def[df_def["Heavy_Defense"]]["Pins_Scored"].mean()
        d_def = contested / uncontested if uncontested > 0 else 1.0

        print("\n=========================================================================")
        print("4. DEFENSE DEGRADATION & ENDGAME CONSTRAINTS")
        print("=========================================================================")
        print(
            f"Uncontested Scoring Mean (<15s def): {uncontested:.2f} pins/match "
            f"({uncontested/2.0:.2f} pins/min)"
        )
        print(
            f"Heavy Defense Scoring Mean (>=15s):  {contested:.2f} pins/match "
            f"({contested/2.0:.2f} pins/min)"
        )
        print(f"Empirical Defense Factor (d_def):    {d_def:.2f} ({(1-d_def)*100:.1f}% reduction)")

        eg = self.df_events[self.df_events["Event_Type"] == "ENDGAME"]
        print(
            f"\nEndgame Attempts: {len(eg)} | Success Rate: {(eg['Outcome']=='SUCCESS').mean()*100:.1f}% | "
            f"Median Start: {eg['Event_start_sec'].median():.1f}s | Median Duration: {eg['Duration'].median():.1f}s"
        )

    def analyze_robot_benchmarks(self):
        """
        Outputs empirical cycle durations and action success rates per Actor_ID.
        """
        rows = []
        for actor, grp in self.df_events.groupby("Actor_ID"):
            grp = grp.sort_values("Event_start_sec")
            places = grp[grp["Event_Type"].isin(["PLACE_YELLOW", "PLACE_COLOURED"])]
            flips = grp[grp["Event_Type"] == "TOGGLE_FLIP"]

            # Place-to-place full cycle time (< 25s to exclude defense/breakdowns)
            if len(places) > 1:
                deltas = places["Event_start_sec"].diff().dropna()
                deltas = deltas[deltas < 25.0]
                cycle_mean = deltas.mean() if len(deltas) > 0 else np.nan
            else:
                cycle_mean = np.nan

            rows.append(
                {
                    "Actor_ID": actor,
                    "Total_Events": len(grp),
                    "Pins_Attempted": len(places),
                    "Pin_Success%": round(
                        (places["Outcome"].isin(["SUCCESS", "PARTIAL"])).mean() * 100, 1
                    )
                    if len(places) > 0
                    else np.nan,
                    "Flips_Attempted": len(flips),
                    "Flip_Success%": round((flips["Outcome"] == "SUCCESS").mean() * 100, 1)
                    if len(flips) > 0
                    else np.nan,
                    "Mean_Cycle_Sec": round(cycle_mean, 2) if not np.isnan(cycle_mean) else np.nan,
                }
            )

        df_bots = pd.DataFrame(rows).sort_values("Total_Events", ascending=False)
        print("\n=========================================================================")
        print("5. EMPIRICAL ROBOT PERFORMANCE BENCHMARKS (events.csv)")
        print("=========================================================================")
        print(df_bots.to_string(index=False))
        return df_bots

    def run_full_report(self):
        self.analyze_primary_quadrant_retention()
        self.analyze_conditional_hold_probabilities()
        self.analyze_realized_yellow_ev()
        self.analyze_defense_and_endgame()
        self.analyze_robot_benchmarks()


if __name__ == "__main__":
    analyzer = EmpiricalMatchAnalyzer("events.csv", "field_states.csv")
    analyzer.run_full_report()