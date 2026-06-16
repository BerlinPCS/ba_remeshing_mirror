import argparse
import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
from matplotlib.ticker import FuncFormatter
from pathlib import Path
from collections import defaultdict
sns.set_theme(style="whitegrid", context="paper", font_scale=1.2)

def load_and_label_data(filepath, strategy_name):
    """Loads a CSV and tags it with the strategy name so we can group the lines."""
    df = pd.read_csv(filepath)
    # Strip whitespace from column names
    df.columns = df.columns.str.strip() 
    df['total_edge_loss'] /= df['edge_count'].replace(0, pd.NA)
    df['Strategy'] = strategy_name
    return df

def generate_thesis_plots(csv_files, output_filename="../out/plots/remeshing_results.svg",
                          title="Remeshing Metrics", metadata=None):
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

    fig, axes = plt.subplots(1, 2, figsize=(24, 10))
    fig.suptitle(title, fontweight='bold', y=1.05)
    if metadata:
        fig.text(0.5, 0.98, metadata, ha='center', va='top', fontsize=10)

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

def parse_filename(filepath):
    stem = filepath.stem
    if '_' in stem:
        mesh_name, strategy_raw = stem.split('_', 1)
        strategy_name = strategy_raw.replace('_', ' ').title()
        return mesh_name, strategy_name
    else:
        return stem, stem

def get_log_files(logs_dir):
    valid_files = []
    for filepath in Path(logs_dir).glob("*.csv"):
        try:
            with open(filepath, 'r') as f:
                lines = 0
                for _ in f:
                    lines += 1
                    if lines > 2:
                        valid_files.append(filepath)
                        break
        except Exception as e:
            print(f"Could not read {filepath}: {e}")
    return valid_files

def parse_csv_arg(value):
    if "=" in value:
        filepath, strategy_name = value.split("=", 1)
        return filepath, strategy_name
    filepath = Path(value)
    _, strategy_name = parse_filename(filepath)
    return str(filepath), strategy_name

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Plot remeshing metrics.')
    parser.add_argument('--mesh', type=str, help='Optional mesh name to plot. If not provided, plots all meshes found in logs.')
    parser.add_argument('--csv', action='append', default=[],
                        help='CSV to plot. Use path=Label to override the legend label. Can be passed multiple times.')
    parser.add_argument('--output', type=str, help='Output SVG path for explicit --csv mode.')
    parser.add_argument('--title', type=str, default='Remeshing Metrics', help='Plot title.')
    parser.add_argument('--metadata', type=str, default='', help='Extra run information rendered below the title.')
    parser.add_argument('--logs-dir', type=str, default="../out/logs", help='Directory to scan when --csv is not used.')
    args = parser.parse_args()

    if args.csv:
        csv_files = dict(parse_csv_arg(value) for value in args.csv)
        output = args.output if args.output else "../out/plots/remeshing_results.svg"
        generate_thesis_plots(csv_files, output_filename=output, title=args.title, metadata=args.metadata)
        exit(0)

    logs_dir = Path(args.logs_dir)
    if not logs_dir.exists():
        print(f"Logs directory {logs_dir} does not exist.")
        exit(1)

    valid_files = get_log_files(logs_dir)
    
    if not valid_files:
        print(f"No valid log files (with >2 lines) found in {logs_dir}.")
        exit(0)

    mesh_files = defaultdict(dict)
    for filepath in valid_files:
        mesh_name, strategy_name = parse_filename(filepath)
        mesh_files[mesh_name][str(filepath)] = strategy_name

    if args.mesh:
        if args.mesh in mesh_files:
            print(f"Plotting for mesh: {args.mesh}")
            generate_thesis_plots(mesh_files[args.mesh], output_filename=f"../out/plots/{args.mesh}_results.svg",
                                  title=f"{args.mesh} Remeshing Metrics")
        else:
            print(f"No valid logs found for mesh: {args.mesh}")
            print(f"Available meshes: {', '.join(mesh_files.keys())}")
    else:
        for mesh_name, files_to_plot in mesh_files.items():
            print(f"Plotting for mesh: {mesh_name}")
            generate_thesis_plots(files_to_plot, output_filename=f"../out/plots/{mesh_name}_results.svg",
                                  title=f"{mesh_name} Remeshing Metrics")
