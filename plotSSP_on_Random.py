import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import random
import matplotlib.cm as cm

file_path = 'results/combined_results.csv'
font = 25

data = pd.read_csv(file_path)

data.set_index('test', inplace=True)

algorithms = [col for col in data.columns if col.startswith('total_cost_')]

algorithm_names = [algorithm.replace('total_cost_', '') for algorithm in algorithms]

average_times = data[algorithms].mean()

fastest_algorithm = average_times.idxmin()

slowest_algorithm = average_times.idxmax()


sorted_tests = data.sort_values(by='N', ascending=True)

def plot_comparison(log_scale=False, step=5):
    plt.figure(figsize=(10, 6))


    xticks_labels = []
    xticks_positions = []


    for i, algorithm in enumerate(algorithms):
        if algorithm == fastest_algorithm:
            color = "#1E90FF"
            linestyle = '-'  
        else:
            color = '#FF8C00' 
            linestyle = '-'  
        
        plt.plot(range(1, len(sorted_tests) + 1), sorted_tests[algorithm], 
                 label=algorithm_names[i], color=color, linestyle=linestyle, linewidth=2.2)

    plt.xlabel('Number of jobs', fontsize=font)
    plt.ylabel('Total cost', fontsize=font)

    for i in range(0, len(sorted_tests), step):
        xticks_positions.append(i + 1)
        xticks_labels.append(sorted_tests['N'].iloc[i])

    plt.xticks(
        ticks=xticks_positions,
        labels=xticks_labels,  
        rotation=0, fontsize=font
    )
    
    plt.yticks(fontsize=font)


    plt.legend(loc='upper left', fontsize=font)

    if log_scale:
        plt.yscale('log')

    plt.tight_layout()
    plt.show()

plot_comparison(log_scale=False, step=70)
