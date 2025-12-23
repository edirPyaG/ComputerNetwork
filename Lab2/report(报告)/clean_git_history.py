#!/usr/bin/env python3
"""清理 Git 历史中的大文件"""
import subprocess
import os
import sys

# 切换到 Git 仓库根目录
repo_root = r"E:\大学有关文件\计算机网络\LabGit\Lab2"
os.chdir(repo_root)

# 要删除的文件路径
file_to_remove = "Lab2/report(报告)/tool/r77Rootkit 1.8.1.zip"

print(f"当前工作目录: {os.getcwd()}")
print(f"准备从 Git 历史中删除: {file_to_remove}")

# 使用 git filter-repo 清理历史
try:
    cmd = [
        sys.executable, "-m", "git_filter_repo",
        "--path", file_to_remove,
        "--invert-paths",
        "--force"
    ]
    print(f"执行命令: {' '.join(cmd)}")
    result = subprocess.run(cmd, capture_output=True, text=True, encoding='utf-8')
    
    print("\n=== 标准输出 ===")
    print(result.stdout)
    
    if result.stderr:
        print("\n=== 标准错误 ===")
        print(result.stderr)
    
    if result.returncode == 0:
        print("\n✓ 成功清理 Git 历史！")
        print("\n下一步操作:")
        print("1. git push origin main --force")
    else:
        print(f"\n✗ 命令执行失败，返回码: {result.returncode}")
        
except Exception as e:
    print(f"错误: {e}")
    print("\n备选方案：使用 git filter-branch")
    print("请手动执行以下命令：")
    print(f'git filter-branch --force --index-filter "git rm --cached --ignore-unmatch \\"{file_to_remove}\\"" --prune-empty --tag-name-filter cat -- --all')
