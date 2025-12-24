#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
RDT协议性能测试数据可视化脚本
生成丢包率、延时对性能影响的对比图表
"""

import matplotlib.pyplot as plt
import matplotlib
import numpy as np

# 设置中文字体支持
matplotlib.rcParams['font.sans-serif'] = ['SimHei', 'Microsoft YaHei', 'Arial Unicode MS']
matplotlib.rcParams['axes.unicode_minus'] = False

# 设置绘图风格
plt.style.use('seaborn-v0_8-darkgrid')

def plot_loss_rate_impact():
    """绘制丢包率对性能的影响（延时固定5ms）"""
    fig, axes = plt.subplots(2, 2, figsize=(14, 10))
    fig.suptitle('丢包率对RDT协议性能的影响（延时=5ms）', fontsize=16, fontweight='bold')
    
    # 数据来源：测试数据.txt
    loss_rates = [0, 0.5, 1, 2, 3]
    throughputs = [0.393, 0.273, 0.276, 0.271, 0.193]  # Mbps
    transmission_times = [37.79, 54.37, 53.90, 54.92, 77.19]  # seconds
    retransmissions = [10, 19, 19, 38, 58]
    timeouts = [10, 10, 1, 1, 2]
    
    # 子图1：吞吐率
    axes[0, 0].plot(loss_rates, throughputs, marker='o', linewidth=2, markersize=8, color='#2E86AB')
    axes[0, 0].set_xlabel('丢包率 (%)', fontsize=12)
    axes[0, 0].set_ylabel('吞吐率 (Mbps)', fontsize=12)
    axes[0, 0].set_title('吞吐率随丢包率变化', fontsize=13, fontweight='bold')
    axes[0, 0].grid(True, alpha=0.3)
    for i, v in enumerate(throughputs):
        axes[0, 0].text(loss_rates[i], v + 0.01, f'{v:.3f}', ha='center', fontsize=9)
    
    # 子图2：传输时间
    axes[0, 1].plot(loss_rates, transmission_times, marker='s', linewidth=2, markersize=8, color='#A23B72')
    axes[0, 1].set_xlabel('丢包率 (%)', fontsize=12)
    axes[0, 1].set_ylabel('传输时间 (秒)', fontsize=12)
    axes[0, 1].set_title('传输时间随丢包率变化', fontsize=13, fontweight='bold')
    axes[0, 1].grid(True, alpha=0.3)
    for i, v in enumerate(transmission_times):
        axes[0, 1].text(loss_rates[i], v + 1.5, f'{v:.2f}s', ha='center', fontsize=9)
    
    # 子图3：重传包数
    axes[1, 0].bar(loss_rates, retransmissions, width=0.4, color='#F18F01', alpha=0.8, edgecolor='black')
    axes[1, 0].set_xlabel('丢包率 (%)', fontsize=12)
    axes[1, 0].set_ylabel('重传包数', fontsize=12)
    axes[1, 0].set_title('重传包数随丢包率变化', fontsize=13, fontweight='bold')
    axes[1, 0].grid(True, alpha=0.3, axis='y')
    for i, v in enumerate(retransmissions):
        axes[1, 0].text(loss_rates[i], v + 1, str(v), ha='center', fontsize=10, fontweight='bold')
    
    # 子图4：超时次数
    axes[1, 1].bar(loss_rates, timeouts, width=0.4, color='#C73E1D', alpha=0.8, edgecolor='black')
    axes[1, 1].set_xlabel('丢包率 (%)', fontsize=12)
    axes[1, 1].set_ylabel('超时次数', fontsize=12)
    axes[1, 1].set_title('超时次数随丢包率变化', fontsize=13, fontweight='bold')
    axes[1, 1].grid(True, alpha=0.3, axis='y')
    for i, v in enumerate(timeouts):
        axes[1, 1].text(loss_rates[i], v + 0.3, str(v), ha='center', fontsize=10, fontweight='bold')
    
    plt.tight_layout()
    plt.savefig('loss_rate_impact.png', dpi=300, bbox_inches='tight')
    print("✓ 已生成：loss_rate_impact.png")


def plot_delay_impact():
    """绘制延时对性能的影响（丢包率固定0.1%）"""
    fig, axes = plt.subplots(2, 2, figsize=(14, 10))
    fig.suptitle('延时对RDT协议性能的影响（丢包率=0.1%）', fontsize=16, fontweight='bold')
    
    # 数据来源：测试数据.txt
    delays = [0, 5, 20, 100]  # ms
    throughputs = [1.019, 0.272, 0.176, 0.056]  # Mbps
    transmission_times = [14.58, 54.58, 84.43, 263.07]  # seconds
    retransmissions = [2, 19, 41, 303]
    timeouts = [1, 18, 40, 301]
    
    # 子图1：吞吐率（对数坐标更清晰）
    axes[0, 0].plot(delays, throughputs, marker='o', linewidth=2, markersize=8, color='#2E86AB')
    axes[0, 0].set_xlabel('延时 (ms)', fontsize=12)
    axes[0, 0].set_ylabel('吞吐率 (Mbps)', fontsize=12)
    axes[0, 0].set_title('吞吐率随延时变化', fontsize=13, fontweight='bold')
    axes[0, 0].set_yscale('log')
    axes[0, 0].grid(True, alpha=0.3, which="both")
    for i, v in enumerate(throughputs):
        axes[0, 0].text(delays[i], v * 1.15, f'{v:.3f}', ha='center', fontsize=9)
    
    # 子图2：传输时间
    axes[0, 1].plot(delays, transmission_times, marker='s', linewidth=2, markersize=8, color='#A23B72')
    axes[0, 1].set_xlabel('延时 (ms)', fontsize=12)
    axes[0, 1].set_ylabel('传输时间 (秒)', fontsize=12)
    axes[0, 1].set_title('传输时间随延时变化', fontsize=13, fontweight='bold')
    axes[0, 1].grid(True, alpha=0.3)
    for i, v in enumerate(transmission_times):
        axes[0, 1].text(delays[i], v + 8, f'{v:.2f}s', ha='center', fontsize=9)
    
    # 子图3：重传包数（对数坐标）
    axes[1, 0].bar([str(d) for d in delays], retransmissions, color='#F18F01', alpha=0.8, edgecolor='black')
    axes[1, 0].set_xlabel('延时 (ms)', fontsize=12)
    axes[1, 0].set_ylabel('重传包数', fontsize=12)
    axes[1, 0].set_title('重传包数随延时变化', fontsize=13, fontweight='bold')
    axes[1, 0].set_yscale('log')
    axes[1, 0].grid(True, alpha=0.3, which="both", axis='y')
    for i, v in enumerate(retransmissions):
        axes[1, 0].text(i, v * 1.2, str(v), ha='center', fontsize=10, fontweight='bold')
    
    # 子图4：超时次数（对数坐标）
    axes[1, 1].bar([str(d) for d in delays], timeouts, color='#C73E1D', alpha=0.8, edgecolor='black')
    axes[1, 1].set_xlabel('延时 (ms)', fontsize=12)
    axes[1, 1].set_ylabel('超时次数', fontsize=12)
    axes[1, 1].set_title('超时次数随延时变化', fontsize=13, fontweight='bold')
    axes[1, 1].set_yscale('log')
    axes[1, 1].grid(True, alpha=0.3, which="both", axis='y')
    for i, v in enumerate(timeouts):
        axes[1, 1].text(i, v * 1.2, str(v), ha='center', fontsize=10, fontweight='bold')
    
    plt.tight_layout()
    plt.savefig('delay_impact.png', dpi=300, bbox_inches='tight')
    print("✓ 已生成：delay_impact.png")


def plot_congestion_window_simulation():
    """模拟拥塞窗口变化过程"""
    fig, ax = plt.subplots(figsize=(14, 6))
    
    # 模拟数据：展示Slow Start → Congestion Avoidance → Fast Recovery
    time = np.arange(0, 100, 0.5)
    cwnd = []
    ssthresh = 64 * 1024  # 初始ssthresh
    current_cwnd = 1024  # 初始MSS
    
    for t in time:
        if t < 20:  # Slow Start阶段
            current_cwnd = 1024 * (2 ** (t / 2))
            if current_cwnd > ssthresh:
                current_cwnd = ssthresh
        elif t < 50:  # Congestion Avoidance阶段
            current_cwnd += (1024 ** 2) / current_cwnd * 0.5
        elif t < 55:  # 3 dup ACKs触发Fast Recovery
            ssthresh = current_cwnd / 2
            current_cwnd = ssthresh + 3 * 1024
        elif t < 75:  # Fast Recovery期间
            current_cwnd += 1024 * 0.3
        elif t < 77:  # Timeout触发
            ssthresh = max(current_cwnd / 2, 1024)
            current_cwnd = 4 * 1024
        else:  # 重新Slow Start
            current_cwnd *= 1.1
            if current_cwnd > ssthresh:
                current_cwnd += 500
        
        cwnd.append(current_cwnd / 1024)  # 转换为KB
    
    # 绘制拥塞窗口变化
    ax.plot(time, cwnd, linewidth=2.5, color='#2E86AB', label='拥塞窗口 (cwnd)')
    ax.axhline(y=ssthresh / 1024, color='#C73E1D', linestyle='--', linewidth=2, label='慢启动阈值 (ssthresh)')
    
    # 标注关键事件
    ax.annotate('慢启动', xy=(10, 20), xytext=(10, 35),
                arrowprops=dict(arrowstyle='->', color='green', lw=2),
                fontsize=11, color='green', fontweight='bold')
    ax.annotate('拥塞避免', xy=(35, 60), xytext=(30, 75),
                arrowprops=dict(arrowstyle='->', color='blue', lw=2),
                fontsize=11, color='blue', fontweight='bold')
    ax.annotate('快速恢复\n(3 dup ACKs)', xy=(52, 45), xytext=(58, 30),
                arrowprops=dict(arrowstyle='->', color='orange', lw=2),
                fontsize=11, color='orange', fontweight='bold')
    ax.annotate('超时重传\n(cwnd → 4MSS)', xy=(76, 4), xytext=(80, 20),
                arrowprops=dict(arrowstyle='->', color='red', lw=2),
                fontsize=11, color='red', fontweight='bold')
    
    ax.set_xlabel('时间 (RTT)', fontsize=13)
    ax.set_ylabel('拥塞窗口 (KB)', fontsize=13)
    ax.set_title('TCP Reno拥塞控制算法 - 拥塞窗口动态变化', fontsize=15, fontweight='bold')
    ax.legend(loc='upper left', fontsize=12)
    ax.grid(True, alpha=0.3)
    
    plt.tight_layout()
    plt.savefig('cwnd_simulation.png', dpi=300, bbox_inches='tight')
    print("✓ 已生成：cwnd_simulation.png")


def plot_performance_comparison():
    """绘制理想环境vs实际环境性能对比"""
    fig, ax = plt.subplots(figsize=(10, 6))
    
    scenarios = ['理想环境\n(无丢包/延时)', '低丢包\n(0.1%, 0ms)', '中等丢包\n(1%, 5ms)', '高丢包\n(3%, 5ms)', '高延时\n(0.1%, 100ms)']
    throughputs = [782.043, 1.019, 0.276, 0.193, 0.056]
    colors = ['#27AE60', '#3498DB', '#F39C12', '#E74C3C', '#8E44AD']
    
    bars = ax.bar(scenarios, throughputs, color=colors, alpha=0.85, edgecolor='black', linewidth=1.5)
    
    # 添加数值标签
    for i, (bar, value) in enumerate(zip(bars, throughputs)):
        height = bar.get_height()
        if value > 10:
            ax.text(bar.get_x() + bar.get_width()/2., height,
                    f'{value:.1f}',
                    ha='center', va='bottom', fontsize=11, fontweight='bold')
        else:
            ax.text(bar.get_x() + bar.get_width()/2., height,
                    f'{value:.3f}',
                    ha='center', va='bottom', fontsize=11, fontweight='bold')
    
    ax.set_ylabel('吞吐率 (Mbps)', fontsize=13)
    ax.set_title('不同网络环境下的RDT协议性能对比', fontsize=15, fontweight='bold')
    ax.set_yscale('log')
    ax.grid(True, alpha=0.3, axis='y', which='both')
    
    # 添加参考线
    ax.axhline(y=1, color='gray', linestyle=':', linewidth=1, alpha=0.7)
    ax.text(0.5, 1.2, '1 Mbps', fontsize=9, color='gray')
    
    plt.tight_layout()
    plt.savefig('performance_comparison.png', dpi=300, bbox_inches='tight')
    print("✓ 已生成：performance_comparison.png")


if __name__ == '__main__':
    print("=" * 50)
    print("RDT协议性能数据可视化")
    print("=" * 50)
    
    plot_loss_rate_impact()
    plot_delay_impact()
    plot_congestion_window_simulation()
    plot_performance_comparison()
    
    print("\n" + "=" * 50)
    print("✓ 所有图表生成完成！")
    print("=" * 50)
    print("\n生成的图表文件：")
    print("  1. loss_rate_impact.png       - 丢包率影响分析（4子图）")
    print("  2. delay_impact.png            - 延时影响分析（4子图）")
    print("  3. cwnd_simulation.png         - 拥塞窗口动态变化")
    print("  4. performance_comparison.png  - 性能对比柱状图")
