import pandas as pd
import numpy as np
import matplotlib.pyplot as plt

df_field = pd.read_csv('field_states.csv')
df_events = pd.read_csv('events.csv')

# 1. Controlling Percentage Analysis
# For each match, determine who won at t=120.
# Then count how many seconds / percentage of snapshot times (from t=10 to t=120) the winning team was leading.
controlling_stats = []

for mid, grp in df_field.groupby('Match_ID'):
    grp = grp.sort_values('Time')
    final_row = grp.iloc[-1]
    
    red_final = final_row['RED_Score']
    blue_final = final_row['BLUE_Score']
    
    if red_final > blue_final:
        winner = 'RED'
    elif blue_final > red_final:
        winner = 'BLUE'
    else:
        winner = 'TIE'
        
    if winner != 'TIE':
        lead_time_winner = 0
        lead_time_loser = 0
        tied_time = 0
        total_valid_time = 0
        
        for i in range(len(grp) - 1):
            r1, r2 = grp.iloc[i], grp.iloc[i+1]
            dt = r2['Time'] - r1['Time']
            s_red = r2['RED_Score']
            s_blue = r2['BLUE_Score']
            
            if winner == 'RED':
                if s_red > s_blue:
                    lead_time_winner += dt
                elif s_blue > s_red:
                    lead_time_loser += dt
                else:
                    tied_time += dt
            else:
                if s_blue > s_red:
                    lead_time_winner += dt
                elif s_red > s_blue:
                    lead_time_loser += dt
                else:
                    tied_time += dt
            total_valid_time += dt
            
        pct_winner_led = (lead_time_winner / total_valid_time) * 100
        pct_loser_led = (lead_time_loser / total_valid_time) * 100
        pct_tied = (tied_time / total_valid_time) * 100
        
        controlling_stats.append({
            'Match_ID': mid,
            'Winner': winner,
            'Red_Final': red_final,
            'Blue_Final': blue_final,
            'Winner_Led_Sec': lead_time_winner,
            'Pct_Winner_Led': pct_winner_led,
            'Pct_Loser_Led': pct_loser_led,
            'Pct_Tied': pct_tied
        })

df_control = pd.DataFrame(controlling_stats)
print("=== CONTROLLING PERCENTAGE PER MATCH ===")
print(df_control.to_string(index=False))

print("\n=== CONTROLLING PERCENTAGE SUMMARY STATISTICS ===")
print(df_control['Pct_Winner_Led'].describe())

# 2. Visualization: Match-by-Match Control & Overall Distribution
if not df_control.empty:
    # Sort ascending so highest controlling % appears at the top of the horizontal bar chart
    df_plot = df_control.sort_values(
        by=['Pct_Winner_Led', 'Pct_Tied'], ascending=[True, True]
    ).reset_index(drop=True)

    # Scale figure height dynamically based on match count
    fig_height = max(6, len(df_plot) * 0.28)
    fig, (ax1, ax2) = plt.subplots(
        1, 2, figsize=(15, fig_height), gridspec_kw={'width_ratios': [1.3, 1]}
    )

    # --- Panel 1: Stacked Horizontal Bar Chart per Match ---
    y_pos = np.arange(len(df_plot))
    y_labels = [
        f"Match {row['Match_ID']} ({row['Winner'][0]}: {int(row['Red_Final'])}-{int(row['Blue_Final'])})"
        for _, row in df_plot.iterrows()
    ]

    ax1.barh(y_pos, df_plot['Pct_Winner_Led'], color='#2a9d8f', label='Winner Leading')
    ax1.barh(
        y_pos,
        df_plot['Pct_Tied'],
        left=df_plot['Pct_Winner_Led'],
        color='#d9d9d9',
        label='Tied'
    )
    ax1.barh(
        y_pos,
        df_plot['Pct_Loser_Led'],
        left=df_plot['Pct_Winner_Led'] + df_plot['Pct_Tied'],
        color='#e76f51',
        label='Loser Leading'
    )

    ax1.axvline(50, color='#333333', linestyle='--', linewidth=1, alpha=0.7)
    ax1.set_yticks(y_pos)
    ax1.set_yticklabels(y_labels, fontsize=9)
    ax1.set_xlim(0, 100)
    ax1.set_xlabel('Percentage of Match Time (%)', fontsize=10)
    ax1.set_title('Match Control Breakdown (Sorted by Winner Lead %)', fontsize=12, fontweight='bold')
    ax1.legend(loc='lower right', framealpha=0.95)
    ax1.grid(axis='x', linestyle=':', alpha=0.5)

    # --- Panel 2: Distribution of Winner Controlling Percentage ---
    bins = np.arange(0, 110, 10)
    ax2.hist(
        df_control['Pct_Winner_Led'],
        bins=bins,
        color='#2a9d8f',
        edgecolor='white',
        alpha=0.85
    )

    mean_val = df_control['Pct_Winner_Led'].mean()
    median_val = df_control['Pct_Winner_Led'].median()

    ax2.axvspan(0, 50, color='#e76f51', alpha=0.1, label='Comeback Wins (<50% Led)')
    ax2.axvline(mean_val, color='#d62828', linestyle='--', linewidth=2, label=f'Mean: {mean_val:.1f}%')
    ax2.axvline(median_val, color='#1d3557', linestyle=':', linewidth=2.2, label=f'Median: {median_val:.1f}%')

    ax2.set_xlim(0, 100)
    ax2.set_xticks(bins)
    ax2.set_xlabel('Winner Controlling Percentage (%)', fontsize=10)
    ax2.set_ylabel('Number of Matches', fontsize=10)
    ax2.set_title('Distribution of Winner Lead Dominance', fontsize=12, fontweight='bold')
    ax2.legend(loc='upper left', framealpha=0.95)
    ax2.grid(axis='y', linestyle=':', alpha=0.5)

    plt.suptitle('Match Lead & Controlling Percentage Analysis', fontsize=14, fontweight='bold', y=1.01)
    plt.tight_layout()
    plt.savefig('controlling_percentage_analysis.png', dpi=300, bbox_inches='tight')
    plt.show()