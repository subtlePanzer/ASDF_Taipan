import matplotlib.pyplot as plt
import numpy as np

def plot_motion_trajectory(filename):
    # 1. Load data from file 
    # Assumes a text file with two columns (XX YY) separated by whitespace or commas
    try:
        data = np.loadtxt(filename, delimiter=',')
    except Exception as e:
        print(f"Error loading file: {e}")
        return

    if data.shape[1] < 2:
        print("Error: File must contain at least two columns for X and Y coordinates.")
        return

    x = data[:, 0]
    y = data[:, 1]
    
    # Create a time/sequence array based on the row order
    time_steps = np.arange(len(x))

    # 2. Set up the plot
    fig, ax = plt.subplots(figsize=(9, 7))

    # Plot a subtle background line to trace the overall path
    ax.plot(x, y, color='gray', alpha=0.3, linestyle='-', zorder=1)

    # 3. Scatter plot with color shading mapped to time progression
    sc = ax.scatter(x, y, c=time_steps, cmap='plasma', s=60, zorder=2, edgecolors='none')
  
    # Highlight the start and end points for clarity
    ax.scatter(x[0], y[0], color='lime', s=120, marker='o', label='Start', zorder=3, edgecolors='black')
    ax.scatter(x[-1], y[-1], color='red', s=120, marker='X', label='End', zorder=3, edgecolors='black')

    # 4. Formatting and aesthetics
    cbar = plt.colorbar(sc)
    cbar.set_label('Motion Progression (Start > End)', rotation=270, labelpad=15)
    
    # Force 1:1 aspect ratio so distances are true to scale
    ax.set_aspect('equal', adjustable='datalim')

    ax.set_title('Coordinate Motion Over Time', fontsize=14, pad=12)
    ax.set_xlabel('X Coordinate', fontsize=11)
    ax.set_ylabel('Y Coordinate', fontsize=11)
    ax.legend(loc='upper right')
    ax.grid(True, linestyle='--', alpha=0.5)

    plt.tight_layout()
    plt.show()

# Run the function (replace 'coords.txt' with your actual filename)
if __name__ == "__main__":
    plot_motion_trajectory('odom.log')
