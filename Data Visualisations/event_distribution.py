import pandas as pd
import numpy as np
import matplotlib.pyplot as plt

df_events = pd.read_csv('events.csv')

# Event frequency over match time (10-second intervals)
# Bins: 0-10, 10-20, ..., 110-120
bins = list(range(0, 130, 10))
labels = [f"{b}-{b+10}s" for b in range(0, 120, 10)]

df_events['Time_Interval'] = pd.cut(df_events['Event_start_sec'], bins=bins, labels=labels, right=False)

# Group by Time_Interval, Alliance, Event_Type
event_dist = df_events.groupby(['Time_Interval', 'Alliance', 'Event_Type'], observed=False).size().unstack(fill_value=0)
print("=== EVENT FREQUENCY BY 10-SECOND INTERVAL & EVENT TYPE ===")
print(df_events.groupby(['Time_Interval', 'Event_Type'], observed=False).size().unstack(fill_value=0))

# Let's plot event frequency over time
fig, ax = plt.subplots(figsize=(10, 5))
event_types = ['PLACE_YELLOW', 'PLACE_COLOURED', 'TOGGLE_FLIP', 'PICKUP_PIN', 'TRAVEL', 'ENDGAME', 'DEFENCE']
inter_counts = df_events.groupby(['Time_Interval', 'Event_Type'], observed=False).size().unstack(fill_value=0)

for et in ['PLACE_YELLOW', 'PLACE_COLOURED', 'TOGGLE_FLIP', 'ENDGAME', 'DEFENCE']:
    if et in inter_counts.columns:
        ax.plot(range(12), inter_counts[et], marker='o', label=et, linewidth=2)

ax.set_xticks(range(12))
ax.set_xticklabels(labels, rotation=45)
ax.set_title('Event Frequency over Match Time (10-Second Intervals)', fontsize=12, fontweight='bold')
ax.set_xlabel('Match Time Interval (Seconds)', fontsize=11)
ax.set_ylabel('Event Count across 12 Matches', fontsize=11)
ax.legend(loc='upper right')
plt.tight_layout()
plt.savefig('fig_event_frequency.png', dpi=300)
plt.close()

print("\nChart saved as fig_event_frequency.png")