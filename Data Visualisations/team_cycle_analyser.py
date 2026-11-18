import pandas as pd
import numpy as np

class TeamCycleAnalyzer:
    """
    Parses Attempt_ID logs from events.csv to compute team-level breakdown:
      - Pickup, Travel, and Placement durations
      - Placement Success Rate (P_success)
      - Expected Cycle Time per Successful Pin (E[T_success] = T_mean / P_success)
    """
    def __init__(self, events_csv_path="events.csv"):
        self.df_events = pd.read_csv(events_csv_path)
        self.df_events['Duration'] = (
            self.df_events['Event_end_sec'] - self.df_events['Event_start_sec']
        ).clip(lower=0.05)
        
    def process_attempts(self):
        attempt_records = []
        for (mid, actor, att_id), group in self.df_events.groupby(['Match_ID', 'Actor_ID', 'Attempt_ID']):
            group = group.sort_values('Event_start_sec')
            types = group['Event_Type'].tolist()
            
            # Filter for attempts that contain a pin placement action
            place_rows = group[group['Event_Type'].isin(['PLACE_YELLOW', 'PLACE_COLOURED'])]
            if len(place_rows) == 0:
                continue
                
            start_t = group['Event_start_sec'].min()
            end_t = group['Event_end_sec'].max()
            total_dur = end_t - start_t
            
            pickup_dur = group[group['Event_Type'] == 'PICKUP_PIN']['Duration'].sum()
            place_dur = place_rows['Duration'].sum()
            travel_dur = group[group['Event_Type'] == 'TRAVEL']['Duration'].sum()
            
            success = any(o in ['SUCCESS', 'PARTIAL'] for o in place_rows['Outcome'])
            
            attempt_records.append({
                'Match_ID': mid,
                'Team': actor,
                'Attempt_ID': att_id,
                'Total_Cycle_Sec': total_dur,
                'Pickup_Sec': pickup_dur,
                'Place_Sec': place_dur,
                'Travel_Sec': travel_dur,
                'Success': success
            })
            
        return pd.DataFrame(attempt_records)

    def generate_team_summary(self):
        df_attempts = self.process_attempts()
        team_rows = []
        
        for team, grp in df_attempts.groupby('Team'):
            n_att = len(grp)
            n_succ = grp['Success'].sum()
            p_succ = n_succ / n_att if n_att > 0 else 0.0
            
            mean_total = grp['Total_Cycle_Sec'].mean()
            mean_pickup = grp['Pickup_Sec'].mean()
            mean_place = grp['Place_Sec'].mean()
            mean_travel = grp['Travel_Sec'].mean()
            
            exp_cycle = (mean_total / p_succ) if p_succ > 0 else np.nan
            
            team_rows.append({
                'Team': team,
                'Attempts': n_att,
                'Successes': n_succ,
                'Success_Rate%': round(p_succ * 100, 1),
                'Mean_Pickup_s': round(mean_pickup, 2),
                'Mean_Place_s': round(mean_place, 2),
                'Mean_Travel_s': round(mean_travel, 2),
                'Mean_Total_Cycle_s': round(mean_total, 2),
                'Expected_Sec_Per_Success': round(exp_cycle, 2) if not np.isnan(exp_cycle) else np.nan
            })
            
        df_summary = pd.DataFrame(team_rows).sort_values('Attempts', ascending=False)
        print("\n=========================================================================")
        print("          TEAM CYCLE TIME & EXPECTED EFFICIENCY STATISTICS               ")
        print("=========================================================================")
        print(df_summary.to_string(index=False))
        print("=========================================================================\n")
        return df_summary

if __name__ == "__main__":
    analyzer = TeamCycleAnalyzer("events.csv")
    analyzer.generate_team_summary()