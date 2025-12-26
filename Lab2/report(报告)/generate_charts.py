#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Visualization script for RDT performance metrics.
Generates English charts for academic reporting.
"""

import matplotlib.pyplot as plt
import numpy as np

# Set plotting style
plt.style.use('seaborn-v0_8-muted')
plt.rcParams.update({'font.size': 12})

def save_single_plot(x, y, xlabel, ylabel, title, filename, color, marker='o', is_bar=False, is_log=False):
    plt.figure(figsize=(10, 6))
    if is_bar:
        bars = plt.bar(x, y, color=color, alpha=0.8, edgecolor='black')
        for bar in bars:
            height = bar.get_height()
            plt.text(bar.get_x() + bar.get_width()/2., height,
                    f'{height:.2f}' if height < 100 else f'{int(height)}',
                    ha='center', va='bottom', fontsize=10, fontweight='bold')
    else:
        plt.plot(x, y, marker=marker, linewidth=2.5, markersize=10, color=color)
        for i, v in enumerate(y):
            plt.text(x[i], v, f'{v:.3f}' if v < 1 else f'{v:.1f}', ha='center', va='bottom', fontsize=10)
    
    plt.xlabel(xlabel)
    plt.ylabel(ylabel)
    if is_log:
        plt.yscale('log')
    plt.grid(True, linestyle='--', alpha=0.6)
    plt.title(title, fontweight='bold')
    plt.tight_layout()
    plt.savefig(filename, dpi=300, bbox_inches='tight')
    plt.close()
    print(f"✓ Saved: {filename}")

def generate_all_plots():
    # 1. Loss Rate Impact Data (Delay = 5ms)
    loss_rates = [0, 0.5, 1, 2, 3]
    throughputs_loss = [0.393, 0.273, 0.276, 0.271, 0.193]
    times_loss = [37.79, 54.37, 53.90, 54.92, 77.19]
    retrans_loss = [10, 19, 19, 38, 58]
    timeouts_loss = [10, 10, 1, 1, 2]

    save_single_plot(loss_rates, throughputs_loss, 'Packet Loss Rate (%)', 'Throughput (Mbps)', 
                    'Throughput vs Loss Rate', 'loss_throughput.png', '#2E86AB')
    
    save_single_plot(loss_rates, times_loss, 'Packet Loss Rate (%)', 'Transmission Time (s)', 
                    'Transmission Time vs Loss Rate', 'loss_time.png', '#A23B72', marker='s')
    
    save_single_plot([str(x) for x in loss_rates], retrans_loss, 'Packet Loss Rate (%)', 'Retransmissions', 
                    'Retransmissions vs Loss Rate', 'loss_retrans.png', '#F18F01', is_bar=True)
    
    save_single_plot([str(x) for x in loss_rates], timeouts_loss, 'Packet Loss Rate (%)', 'Timeouts', 
                    'Timeouts vs Loss Rate', 'loss_timeouts.png', '#C73E1D', is_bar=True)

    # 2. Delay Impact Data (Loss = 0.1%)
    delays = [0, 5, 20, 100]
    throughputs_delay = [1.019, 0.272, 0.176, 0.056]
    times_delay = [14.58, 54.58, 84.43, 263.07]
    retrans_delay = [2, 19, 41, 303]
    timeouts_delay = [1, 18, 40, 301]

    save_single_plot(delays, throughputs_delay, 'One-way Delay (ms)', 'Throughput (Mbps)', 
                    'Throughput vs Network Delay', 'delay_throughput.png', '#2E86AB', is_log=True)
    
    save_single_plot(delays, times_delay, 'One-way Delay (ms)', 'Transmission Time (s)', 
                    'Transmission Time vs Network Delay', 'delay_time.png', '#A23B72', marker='s')

    save_single_plot([str(d) for d in delays], retrans_delay, 'One-way Delay (ms)', 'Retransmissions', 
                    'Retransmissions vs Network Delay', 'delay_retrans.png', '#F18F01', is_bar=True, is_log=True)

    # 3. Congestion Window Simulation
    time = np.arange(0, 100, 0.5)
    cwnd = []
    ssthresh_val = 64 * 1024
    curr_cwnd = 1024
    for t in time:
        if t < 20: curr_cwnd = 1024 * (2 ** (t / 2))
        elif t < 50: curr_cwnd += (1024 ** 2) / curr_cwnd * 0.5
        elif t < 55: curr_cwnd = curr_cwnd / 2 + 3 * 1024
        elif t < 75: curr_cwnd += 1024 * 0.3
        elif t < 77: curr_cwnd = 4 * 1024
        else: curr_cwnd *= 1.1
        cwnd.append(curr_cwnd / 1024)
    
    plt.figure(figsize=(12, 6))
    plt.plot(time, cwnd, linewidth=2.5, color='#2E86AB', label='Congestion Window (cwnd)')
    plt.axhline(y=32, color='#C73E1D', linestyle='--', label='ssthresh')
    plt.annotate('Slow Start', xy=(10, 20), xytext=(10, 40), arrowprops=dict(arrowstyle='->', color='green'), fontweight='bold')
    plt.annotate('Congestion\nAvoidance', xy=(35, 60), xytext=(30, 80), arrowprops=dict(arrowstyle='->', color='blue'), fontweight='bold')
    plt.annotate('Fast Recovery', xy=(52, 45), xytext=(58, 30), arrowprops=dict(arrowstyle='->', color='orange'), fontweight='bold')
    plt.annotate('Timeout', xy=(76, 4), xytext=(80, 20), arrowprops=dict(arrowstyle='->', color='red'), fontweight='bold')
    plt.xlabel('Time (RTT)')
    plt.ylabel('Window Size (KB)')
    plt.title('TCP Reno Congestion Window Dynamics', fontweight='bold')
    plt.legend()
    plt.grid(True, alpha=0.3)
    plt.tight_layout()
    plt.savefig('cwnd_dynamics.png', dpi=300)
    plt.close()
    print("✓ Saved: cwnd_dynamics.png")

    # 4. Scenario Comparison
    scenarios = ['Ideal', 'Loss 0.1%\n0ms', 'Loss 1%\n5ms', 'Loss 3%\n5ms', 'Delay 100ms\n0.1%']
    tput_comp = [782.043, 1.019, 0.276, 0.193, 0.056]
    plt.figure(figsize=(10, 6))
    plt.bar(scenarios, tput_comp, color=['#27AE60', '#3498DB', '#F39C12', '#E74C3C', '#8E44AD'], alpha=0.8)
    plt.yscale('log')
    plt.ylabel('Throughput (Mbps)')
    plt.title('Performance Comparison across Network Scenarios', fontweight='bold')
    plt.grid(True, axis='y', linestyle=':', alpha=0.7)
    for i, v in enumerate(tput_comp):
        plt.text(i, v, f'{v:.1f}' if v > 10 else f'{v:.3f}', ha='center', va='bottom', fontweight='bold')
    plt.tight_layout()
    plt.savefig('perf_comparison.png', dpi=300)
    plt.close()
    print("✓ Saved: perf_comparison.png")

if __name__ == '__main__':
    generate_all_plots()
