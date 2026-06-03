import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
from matplotlib.ticker import FuncFormatter
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
    #df['Cumulative_Time_s'] = df.groupby('Strategy')['time_ms'].cumsum() / 1000.0

    fig, axes = plt.subplots(1, 2, figsize=(12, 5))

    # --- Plot A: Convergence (Loss vs. Iterations) ---
    sns.lineplot(data=df, x='operations', y='total_edge_loss', hue='Strategy', 
                 marker='o', linewidth=2, ax=axes[0])
    axes[0].set_title('Convergence: Global Entropy vs. Operations', fontweight='bold')
    axes[0].set_ylabel('Total Entropy (Loss)')
    axes[0].set_xlabel('Operation Number')
    
    # Format x-axis labels to 'k' format
    formatter = FuncFormatter(lambda x, pos: f'{int(x/1000)}k' if x >= 1000 else (f'{int(x)}' if x == 0 else f'{x:g}'))
    axes[0].xaxis.set_major_formatter(formatter)
    axes[0].yaxis.set_major_formatter(formatter)

    # --- Plot B: Efficiency (Loss vs. Compute Time) ---
    #sns.lineplot(data=df, x='Cumulative_Time_s', y='total_edge_loss', hue='Strategy', 
    #             marker='s', linewidth=2, ax=axes[1])
    #axes[1].set_title('Efficiency: Global Entropy vs. Compute Time', fontweight='bold')
    #axes[1].set_ylabel('Total Entropy (Loss)')
    #axes[1].set_xlabel('Cumulative Time (Seconds)')

    # --- Plot C: Mesh Complexity (Vertices vs. Iteration) ---
    sns.lineplot(data=df, x='operations', y='vertex_count', hue='Strategy', 
                 marker='^', linewidth=2, ax=axes[1])
    axes[1].set_title('Mesh Complexity Tracking', fontweight='bold')
    axes[1].set_ylabel('Total Vertex Count')
    axes[1].set_xlabel('Operation Number')
    axes[1].xaxis.set_major_formatter(formatter)
    axes[1].yaxis.set_major_formatter(formatter)

    plt.tight_layout()
    output_filename = Path(output_filename)
    output_filename.parent.mkdir(parents=True, exist_ok=True)
    plt.savefig(output_filename, format='svg', bbox_inches='tight')
    print(f"Successfully generated high-resolution plot: {output_filename}")

if __name__ == "__main__":
    files_to_plot = {
        "../out/logs/results_Standard_stanford-bunny.csv": "Standard",
        "../out/logs/results_Priority Local_stanford-bunny.csv": "Priority Local",
        "../out/logs/results_Priority Global_stanford-bunny.csv": "Priority Global"
    }
    
    generate_thesis_plots(files_to_plot)