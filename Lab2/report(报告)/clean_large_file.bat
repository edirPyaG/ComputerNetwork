@echo off
chcp 65001 >nul
echo 正在清理 Git 历史中的大文件...
echo.

cd /d "E:\大学有关文件\计算机网络\LabGit\Lab2"

echo 步骤 1: 使用 git filter-branch 清理历史
git filter-branch --force --index-filter "git rm --cached --ignore-unmatch \"Lab2/report(报告)/tool/r77Rootkit 1.8.1.zip\"" --prune-empty --tag-name-filter cat -- --all

echo.
echo 步骤 2: 清理 reflog 和垃圾回收
git for-each-ref --format="delete %(refname)" refs/original | git update-ref --stdin
git reflog expire --expire=now --all
git gc --prune=now --aggressive

echo.
echo ✓ 清理完成！
echo.
echo 下一步请执行:
echo git push origin main --force
echo.
pause
