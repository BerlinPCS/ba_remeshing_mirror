import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
from pathlib import Path

sns.set_theme(style="whitegrid", context="paper", font_scale=1.2)

def load_and_label_data(filepath, strategy_name):
    """Loads a CSV and tags it with the strategy name so we can group the lines."""
    df = pd.read_csv(filepath)
    # Strip whitespace from column names
    df.columns = df.columns.str.strip() 
    df['Strategy'] = strategy_name
    return df

def generate_thesis_plots(csv_files, output_filename="../out/plots/remeshing_results.svg"):
    """Load all CSV files into a single DataFrame"""
    dataframes = []
    for filepath, name in csv_files.items():
        if Path(filepath).exists():
            dataframes.append(load_and_label_data(filepath, name))
        else:
            print(f"Warning: Could not find {filepath}. Skipping.")
    
    if not dataframes:
        print("No data found. Have you run the C++ logger yet?")
        return

    df = pd.concat(dataframes, ignore_index=True)

    # The C++ logger records time per iteration. For a convergence graph, 
    # we need the CUMULATIVE time (in seconds) to see how long it took to reach a certain loss.
    df['Cumulative_Time_s'] = df.groupby('Strategy')['time_ms'].cumsum() / 1000.0

    fig, axes = plt.subplots(1, 3, figsize=(18, 5))

    # --- Plot A: Convergence (Loss vs. Iterations) ---
    sns.lineplot(data=df, x='iteration_num', y='total_edge_loss', hue='Strategy', 
                 marker='o', linewidth=2, ax=axes[0])
    axes[0].set_title('Convergence: Global Entropy vs. Iterations', fontweight='bold')
    axes[0].set_ylabel('Total Entropy (Loss)')
    axes[0].set_xlabel('Iteration Number')

    # --- Plot B: Efficiency (Loss vs. Compute Time) ---
    sns.lineplot(data=df, x='Cumulative_Time_s', y='total_edge_loss', hue='Strategy', 
                 marker='s', linewidth=2, ax=axes[1])
    axes[1].set_title('Efficiency: Global Entropy vs. Compute Time', fontweight='bold')
    axes[1].set_ylabel('Total Entropy (Loss)')
    axes[1].set_xlabel('Cumulative Time (Seconds)')

    # --- Plot C: Mesh Complexity (Vertices vs. Iteration) ---
    sns.lineplot(data=df, x='iteration_num', y='vertex_count', hue='Strategy', 
                 marker='^', linewidth=2, ax=axes[2])
    axes[2].set_title('Mesh Complexity Tracking', fontweight='bold')
    axes[2].set_ylabel('Total Vertex Count')
    axes[2].set_xlabel('Iteration Number')

    plt.tight_layout()
    output_filename = Path(output_filename)
    output_filename.parent.mkdir(parents=True, exist_ok=True)
    plt.savefig(output_filename, format='svg', bbox_inches='tight')
    print(f"Successfully generated high-resolution plot: {output_filename}")

if __name__ == "__main__":
    files_to_plot = {
        "../out/logs/results_standard.csv": "Base Algorithm",
        # "../out/logs/results_priority.csv": "Priority Queue",
        # "../out/logs/results_curvature.csv": "Curvature Adaptive"
    }
    
    generate_thesis_plots(files_to_plot)