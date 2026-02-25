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


average_cost = data[algorithms].mean()


fastest_algorithm = average_cost.idxmin()


slowest_algorithm = average_cost.idxmax()

def get_test_class_and_category(test_name):
    test_class = next((c for c in test_name if c.isupper()), '')
    
    if test_class == 'F':
        category = int(test_name[test_name.index(test_class)+1])
        test_class += str(category)
    else:
        category = int(test_name[test_name.index(test_class)+1:].split('_')[0])
    return test_class

def sort_tests_by_class_and_category(test_names):
    return sorted(test_names, key=lambda x: (get_test_class_and_category(x), data.loc[x, fastest_algorithm]))

test_names = data.index.tolist()

sorted_test_names = sort_tests_by_class_and_category(test_names)


sorted_tests = data.loc[sorted_test_names, algorithms]

def plot_comparison(log_scale=False):
    plt.figure(figsize=(10, 6))


    xticks_labels = []
    xticks_positions = []

    prev_class = None
    group_start = None  
    group_end = None  
    group_positions = []  
    
    plt.axvline(x=0, color='gray', linestyle=':', linewidth=1)

    for i, algorithm in enumerate(algorithms):
        if algorithm == fastest_algorithm:
            color = "#1E90FF"  
            linestyle = '-'  
        else:
            color = '#FF8C00'  
            linestyle = '-'  
        
        plt.plot([i+1 for i in range(len(sorted_tests[algorithm]))], sorted_tests[algorithm], 
                 label=algorithm_names[i], color=color, linestyle=linestyle, linewidth=2.2)

        
        for j, test_name in enumerate(sorted_test_names):
            test_class = get_test_class_and_category(test_name)

            
            if test_class != prev_class:
                if prev_class is not None:
        
                    plt.axvline(x=group_end, color='gray', linestyle=':', linewidth=1)

                    
                    middle_position = np.mean(group_positions)
                    xticks_labels.append(prev_class)
                    xticks_positions.append(middle_position)


                group_positions = [j + 1]  
            else:

                group_end = j + 1
                group_positions.append(j + 1)

            prev_class = test_class


    if group_positions:
        plt.axvline(x=group_end, color='gray', linestyle=':', linewidth=1)


        middle_position = np.mean(group_positions)
        xticks_labels.append(prev_class)
        xticks_positions.append(middle_position)
    

    plt.xlabel('Instances', fontsize=font)
    plt.ylabel('Total cost', fontsize=font)

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


plot_comparison(log_scale=False)

